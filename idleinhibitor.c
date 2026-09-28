#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/file.h>
#include <sys/inotify.h>
#include <sys/wait.h>
#include <systemd/sd-bus.h>

static char lock_path[512];

static void init_paths(void) {
    const char *dir = getenv("XDG_RUNTIME_DIR");
    snprintf(lock_path, sizeof(lock_path), "%s/eww-idle-inhibit.lock",
             (dir && *dir) ? dir : "/tmp");
}

static int open_lock(void) {
    return open(lock_path, O_RDWR | O_CREAT | O_CLOEXEC, 0600);
}

static int lock_held(int fd) {
    if (flock(fd, LOCK_SH | LOCK_NB) == 0) {
        flock(fd, LOCK_UN);
        return 0;
    }
    return errno == EWOULDBLOCK;
}

static int is_running(void) {
    int fd = open_lock();
    if (fd < 0) return 0;
    int held = lock_held(fd);
    close(fd);
    return held;
}

static int take_inhibitor(void) {
    sd_bus *bus = NULL;
    sd_bus_message *reply = NULL;
    sd_bus_error err = SD_BUS_ERROR_NULL;
    int fd = -1;

    int r = sd_bus_open_system(&bus);
    if (r < 0) {
        fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-r));
        goto out;
    }

    r = sd_bus_call_method(bus,
                           "org.freedesktop.login1",
                           "/org/freedesktop/login1",
                           "org.freedesktop.login1.Manager",
                           "Inhibit", &err, &reply, "ssss",
                           "idle", "eww", "manual idle inhibit from eww", "block");
    if (r < 0) {
        fprintf(stderr, "Inhibit failed: %s\n", err.message ? err.message : strerror(-r));
        goto out;
    }

    int raw = -1;
    r = sd_bus_message_read(reply, "h", &raw);
    if (r < 0) {
        fprintf(stderr, "Failed to read inhibitor fd: %s\n", strerror(-r));
        goto out;
    }

    fd = fcntl(raw, F_DUPFD_CLOEXEC, 3);

out:
    sd_bus_error_free(&err);
    sd_bus_message_unref(reply);
    sd_bus_flush_close_unref(bus);
    return fd;
}

static void eww_update(int on) {
    pid_t pid = fork();
    if (pid == 0) {
        int dn = open("/dev/null", O_RDWR);
        if (dn >= 0) {
            dup2(dn, 0);
            dup2(dn, 1);
            dup2(dn, 2);
        }
        execlp("eww", "eww", "update",
               on ? "idle_inhibit=true" : "idle_inhibit=false", (char *)NULL);
        _exit(127);
    }
    if (pid > 0) waitpid(pid, NULL, 0);
}

static int start_inhibit(void) {
    int lfd = open_lock();
    if (lfd < 0) {
        perror("open lockfile");
        return 1;
    }
    if (flock(lfd, LOCK_EX | LOCK_NB) < 0) {
        close(lfd); 
        return 0;
    }

    int p[2];
    if (pipe2(p, O_CLOEXEC) < 0) {
        perror("pipe2");
        close(lfd);
        return 1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        close(lfd);
        return 1;
    }

    if (pid == 0) {
        close(p[0]);
        setsid();

        sigset_t set;
        sigemptyset(&set);
        sigaddset(&set, SIGTERM);
        sigaddset(&set, SIGINT);
        sigprocmask(SIG_BLOCK, &set, NULL);

        int ifd = take_inhibitor();
        char ok = (ifd >= 0);

        if (ok) {
            char b[32];
            int n = snprintf(b, sizeof(b), "%d\n", (int)getpid());
            if (ftruncate(lfd, 0) < 0 || pwrite(lfd, b, n, 0) < 0) { /* pid is best-effort */ }
        }

        if (write(p[1], &ok, 1) < 0) { /* parent gone */ }
        close(p[1]);
        if (!ok) _exit(1);

        int dn = open("/dev/null", O_RDWR);
        if (dn >= 0) {
            dup2(dn, 0);
            dup2(dn, 1);
            dup2(dn, 2);
        }

        int sig;
        sigwait(&set, &sig); 
        close(ifd);          
        _exit(0);
    }

    close(p[1]);
    close(lfd); 
    char ok = 0;
    ssize_t n;
    do {
        n = read(p[0], &ok, 1);
    } while (n < 0 && errno == EINTR);
    close(p[0]);

    if (n != 1 || !ok) {
        waitpid(pid, NULL, 0);
        fprintf(stderr, "Failed to acquire idle inhibitor\n");
        return 1;
    }

    eww_update(1);
    return 0;
}

static int stop_inhibit(void) {
    if (is_running()) {
        int fd = open_lock();
        if (fd >= 0) {
            char b[32];
            ssize_t n = pread(fd, b, sizeof(b) - 1, 0);
            close(fd);
            if (n > 0) {
                b[n] = '\0';
                pid_t pid = (pid_t)strtol(b, NULL, 10);
                if (pid > 1) kill(pid, SIGTERM);
            }
        }
    }
    eww_update(0);
    return 0;
}

static void emit_state(int on, int *last) {
    if (*last == on) return; 
    *last = on;
    puts(on ? "true" : "false");
    fflush(stdout);
}

static int listen_status(void) {
    int lfd = open_lock();
    if (lfd < 0) {
        perror("open lockfile");
        return 1;
    }

    int ifd = inotify_init1(IN_CLOEXEC);
    if (ifd < 0 || inotify_add_watch(ifd, lock_path, IN_MODIFY) < 0) {
        perror("inotify");
        return 1;
    }

    int last = -1;
    char ev[1024] __attribute__((aligned(__alignof__(struct inotify_event))));

    for (;;) {
        if (lock_held(lfd)) {
            emit_state(1, &last);
            while (flock(lfd, LOCK_SH) < 0 && errno == EINTR) {
            }
            flock(lfd, LOCK_UN); 
            emit_state(0, &last);
        } else {
            emit_state(0, &last);
            ssize_t n = read(ifd, ev, sizeof(ev)); 
            if (n < 0 && errno != EINTR) {
                perror("read inotify");
                return 1;
            }
        }
    }
}

int main(int argc, char **argv) {
    init_paths();
    const char *cmd = argc > 1 ? argv[1] : "toggle";

    if (strcmp(cmd, "listen") == 0) return listen_status();

    if (strcmp(cmd, "status") == 0) {
        puts(is_running() ? "true" : "false");
        return 0;
    }
    if (strcmp(cmd, "start") == 0) return start_inhibit();
    if (strcmp(cmd, "stop") == 0) return stop_inhibit();

    return is_running() ? stop_inhibit() : start_inhibit();
}