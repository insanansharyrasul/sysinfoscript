#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/inotify.h>

#define BL_DIR "/sys/class/backlight/"
#define STEP_PCT 5

static char dev_path[512];

static int find_device(void) {
    DIR *d = opendir(BL_DIR);
    if (!d) return 0;

    struct dirent *e;
    int ok = 0;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        snprintf(dev_path, sizeof(dev_path), BL_DIR "%s", e->d_name);
        ok = 1;
        break;
    }
    closedir(d);
    return ok;
}

static int open_attr(const char *name, int flags) {
    char p[640];
    snprintf(p, sizeof(p), "%s/%s", dev_path, name);
    return open(p, flags | O_CLOEXEC);
}

static long read_long(int fd) {
    char buf[32];
    ssize_t n = pread(fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) return -1;
    buf[n] = '\0';
    return strtol(buf, NULL, 10);
}

static int percent(long cur, long max) {
    return (int)((cur * 100 + max / 2) / max);
}

static long clamp(long v, long max) {
    if (v < 0) return 0;
    if (v > max) return max;
    return v;
}

static int write_brightness(long v) {
    int fd = open_attr("brightness", O_WRONLY);
    if (fd < 0) {
        perror("open brightness (write)");
        return 0;
    }
    char buf[32];
    int n = snprintf(buf, sizeof(buf), "%ld", v);
    ssize_t w = write(fd, buf, n);
    int err = errno;
    close(fd);
    if (w != n) {
        errno = err;
        perror("write brightness");
        return 0;
    }
    return 1;
}

static void emit(int bfd, long max) {
    long cur = read_long(bfd);
    printf("%d\n", cur < 0 ? 0 : percent(cur, max));
    fflush(stdout);
}

static int listen_loop(int bfd, long max) {
    emit(bfd, max);

    int ifd = inotify_init1(IN_CLOEXEC);
    char path[640];
    snprintf(path, sizeof(path), "%s/brightness", dev_path);

    if (ifd >= 0 && inotify_add_watch(ifd, path, IN_MODIFY) >= 0) {
        char ev[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
        for (;;) {
            ssize_t n = read(ifd, ev, sizeof(ev)); // blocks, zero CPU while idle
            if (n < 0) {
                if (errno == EINTR) continue;
                break;
            }
            emit(bfd, max); 
        }
        return 0;
    }

    for (;;) {
        sleep(1);
        emit(bfd, max);
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s get | set <percent> | scroll up|down | listen\n", argv[0]);
        return 1;
    }

    if (!find_device()) {
        fprintf(stderr, "No backlight device found in %s\n", BL_DIR);
        return 1;
    }

    int mfd = open_attr("max_brightness", O_RDONLY);
    int bfd = open_attr("brightness", O_RDONLY);
    if (mfd < 0 || bfd < 0) {
        perror("open sysfs attribute");
        return 1;
    }
    long max = read_long(mfd);
    close(mfd);
    if (max <= 0) {
        fprintf(stderr, "Invalid max_brightness\n");
        return 1;
    }

    const char *cmd = argv[1];
    int rc = 0;

    if (strcmp(cmd, "get") == 0) {
        long cur = read_long(bfd);
        printf("%d\n", cur < 0 ? 0 : percent(cur, max));
    } else if (strcmp(cmd, "set") == 0) {
        double pct = argc > 2 ? strtod(argv[2], NULL) : 0.0;
        long raw = clamp((long)(pct * max / 100.0 + 0.5), max);
        rc = write_brightness(raw) ? 0 : 1;
    } else if (strcmp(cmd, "scroll") == 0 && argc > 2) {
        long step = max * STEP_PCT / 100;
        if (step < 1) step = 1;
        long cur = read_long(bfd);
        if (cur < 0) return 1;
        if (strcmp(argv[2], "up") == 0)
            rc = write_brightness(clamp(cur + step, max)) ? 0 : 1;
        else if (strcmp(argv[2], "down") == 0)
            rc = write_brightness(clamp(cur - step, max)) ? 0 : 1;
    } else if (strcmp(cmd, "listen") == 0) {
        rc = listen_loop(bfd, max);
    } else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        rc = 1;
    }

    close(bfd);
    return rc;
}