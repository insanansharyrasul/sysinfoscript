#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
char buf[2048];

static int read_mem_stats(int fd, unsigned long long *mem_total,
                          unsigned long long *mem_available) {

    if (lseek(fd, 0, SEEK_SET) < 0)
        return 0;

    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n <= 0)
        return 0;
    buf[n] = '\0';

    char *p;
    *mem_total = 0;
    *mem_available = 0;

    if ((p = strstr(buf, "MemTotal:")) != NULL)
        sscanf(p, "MemTotal: %llu kB", mem_total);

    if ((p = strstr(buf, "MemAvailable:")) != NULL)
        sscanf(p, "MemAvailable: %llu kB", mem_available);

    return (*mem_total > 0);
}

int main(int argc, char *argv[]) {
    int fd = open("/proc/meminfo", O_RDONLY);
    if (fd < 0) {
        perror("Gagal membuka /proc/meminfo");
        return 1;
    }

    int mode = argc > 1 ? atoi(argv[1]) : 3;
    if (mode < 1 || mode > 3)
        mode = 3;

    struct timespec interval = {.tv_sec = 0, .tv_nsec = 100000000L};
    unsigned long long mem_total, mem_available;

    while (1) {
        if (read_mem_stats(fd, &mem_total, &mem_available) &&
            mem_available <= mem_total) {
            unsigned long long mem_used = mem_total - mem_available;
            unsigned long long usage_pct =
                ((double)mem_used / (double)mem_total) * 100;
            if (mode == 1)
                printf("%llu", usage_pct);
            else
                printf(mode == 2 ? "\r%llu" : "%llu\n", usage_pct);
            fflush(stdout);
        }

        if (mode == 1)
            break;

        nanosleep(&interval, NULL);
    }

    close(fd);
    return 0;
}
