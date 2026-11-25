#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* "3 day 2 hour 15 minute", "3d2h15m", "-1 week" -> date and time from now. */
int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <N unit> [N unit ...]\n"
                        "Units: year month week day hour minute second (or y mo w d h m s)\n"
                        "Example: %s 3 day 2 hour 15 minute\n", argv[0], argv[0]);
        return 1;
    }

    time_t now = time(NULL);
    struct tm tm = *localtime(&now);

    for (int i = 1; i < argc; i++) {
        for (char *p = argv[i]; *p;) {
            if (isspace((unsigned char)*p) || *p == ',') { p++; continue; }
            char *end;
            long n = strtol(p, &end, 10);
            if (end == p) goto bad;
            p = end;
            while (isspace((unsigned char)*p)) p++;
            if (!*p && i + 1 < argc) p = argv[++i]; /* unit is the next arg */
            char u[16];
            size_t len = 0;
            while (isalpha((unsigned char)*p) && len < sizeof u - 1) u[len++] = (char)tolower((unsigned char)*p++);
            u[len] = '\0';
            if (!len) goto bad;
            if (!strncmp(u, "mo", 2))      tm.tm_mon += (int)n;
            else switch (u[0]) {
                case 'y': tm.tm_year += (int)n; break;
                case 'w': tm.tm_mday += 7 * (int)n; break;
                case 'd': tm.tm_mday += (int)n; break;
                case 'h': tm.tm_hour += (int)n; break;
                case 'm': tm.tm_min += (int)n; break;
                case 's': tm.tm_sec += (int)n; break;
                default: goto bad;
            }
            continue;
        bad:
            fprintf(stderr, "%s: can't parse near '%s'\n", argv[0], p);
            return 1;
        }
    }

    tm.tm_isdst = -1; /* mktime normalizes overflow and handles DST */
    time_t then = mktime(&tm);
    if (then == (time_t)-1) { fprintf(stderr, "%s: date out of range\n", argv[0]); return 1; }
    char buf[64];
    strftime(buf, sizeof buf, "%A, %d/%m/%Y %H:%M:%S %Z", localtime(&then));
    puts(buf);
    return 0;
}
