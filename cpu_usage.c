#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
} CpuStats;

static int read_cpu_stats(int fd, CpuStats *stats) {
    char buf[256];

    if (lseek(fd, 0, SEEK_SET) < 0)
        return 0;

    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n <= 0)
        return 0;
    buf[n] = '\0';

    char label[16];
    int ret =
        sscanf(buf, "%15s %llu %llu %llu %llu %llu %llu %llu %llu", label,
               &stats->user, &stats->nice, &stats->system, &stats->idle,
               &stats->iowait, &stats->irq, &stats->softirq, &stats->steal);
    return ret == 9;
}

int main(int argc, char *argv[]) {
    int fd = open("/proc/stat", O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Gagal membuka /proc/stat\n");
        return 1;
    }

    CpuStats prev, curr;
    if (!read_cpu_stats(fd, &prev)) {
        fprintf(stderr, "Gagal membaca /proc/stat\n");
        close(fd);
        return 1;
    }

    int mode = argc > 1 ? atoi(argv[1]) : 3;
    if (mode < 1 || mode > 3) {
        mode = 3;
    }

    struct timespec interval = {.tv_sec = 0, .tv_nsec = 100000000L};

    while (1) {
        nanosleep(&interval, NULL);

        if (!read_cpu_stats(fd, &curr))
            continue;

        unsigned long long prev_idle = prev.idle + prev.iowait;
        unsigned long long curr_idle = curr.idle + curr.iowait;

        unsigned long long prev_non_idle = prev.user + prev.nice + prev.system +
                                           prev.irq + prev.softirq + prev.steal;
        unsigned long long curr_non_idle = curr.user + curr.nice + curr.system +
                                           curr.irq + curr.softirq + curr.steal;

        unsigned long long prev_total = prev_idle + prev_non_idle;
        unsigned long long curr_total = curr_idle + curr_non_idle;

        if (curr_total > prev_total) {
            unsigned long long total_d = curr_total - prev_total;
            unsigned long long idle_d = curr_idle - prev_idle;
            double cpu_pct =
                ((double)(total_d - idle_d) / (double)total_d) * 100.0;
            if (mode == 1)
                printf("%.2f%%", cpu_pct);
            else 
                printf(mode == 2 ? "\r%.2f%%   " : "%.2f%%\n", cpu_pct);
            fflush(stdout);
        }

        if (mode == 1)
            break;
        prev = curr;
    }

    close(fd);
    return 0;
}
