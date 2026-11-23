#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Order matters: index 0 is the largest unit. */
enum unit { U_YEAR, U_MONTH, U_WEEK, U_DAY, U_HOUR, U_MINUTE, U_SECOND, U_COUNT };

static const char *const symbols[U_COUNT] = { "Y", "M", "W", "D", "H", "mm", "s" };
static const char *const names[U_COUNT] = {
    "year", "month", "week", "day", "hour", "minute", "second"
};

#define MAX_INPUT 1000000000000LL /* keeps n * 86400 inside int64 */

static void usage(const char *program)
{
    printf("Usage: %s [options] <N><unit>\n\n", program);
    printf("Units: Y year, M month, W week, D day, H hour, mm minute, s second\n");
    printf("Options:\n");
    printf("  -u, --unit UNIT   break down only down to UNIT (default: s)\n");
    printf("  -t, --total UNIT  print the whole duration as one UNIT count (rounded down)\n");
    printf("  -f, --from DATE   calendar start date DD/MM/YY (default: today)\n");
    printf("  -h, --help        show this help\n");
    printf("Example: %s 90D   ->  2 months 29 days (from 23/11/26)\n", program);
}

static bool parse_unit(const char *text, enum unit *unit)
{
    for (int i = 0; i < U_COUNT; i++) {
        if (strcmp(text, symbols[i]) == 0) {
            *unit = (enum unit)i;
            return true;
        }
    }
    return false;
}

static bool is_leap(int64_t year)
{
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

static int days_in_month(int64_t year, int month)
{
    static const int days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    return days[month - 1] + (month == 2 && is_leap(year));
}

/* Days since 1970-01-01, using the proleptic Gregorian calendar. */
static int64_t days_from_civil(int64_t year, unsigned month, unsigned day)
{
    year -= month <= 2;
    int64_t era = (year >= 0 ? year : year - 399) / 400;
    unsigned year_of_era = (unsigned)(year - era * 400);
    unsigned day_of_year = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5
                           + day - 1;
    unsigned day_of_era = year_of_era * 365 + year_of_era / 4
                          - year_of_era / 100 + day_of_year;
    return era * 146097 + (int64_t)day_of_era - 719468;
}

/* Day number of (y, m, d) plus k calendar months; day is clamped to month end. */
static int64_t add_months(int64_t y, int m, int d, int64_t k)
{
    int64_t total = y * 12 + (m - 1) + k;
    int64_t ny = total / 12;
    int nm = (int)(total % 12) + 1;
    int nd = d < days_in_month(ny, nm) ? d : days_in_month(ny, nm);
    return days_from_civil(ny, (unsigned)nm, (unsigned)nd);
}

static bool parse_date(const char *text, int *year, int *month, int *day)
{
    int short_year, count = 0;
    if (sscanf(text, "%d/%d/%d%n", day, month, &short_year, &count) != 3 ||
        text[count] != '\0') {
        return false;
    }
    *year = (short_year >= 0 && short_year <= 99)
                ? (short_year >= 69 ? 1900 + short_year : 2000 + short_year)
                : short_year;
    return *year >= 1 && *year <= 9999 && *month >= 1 && *month <= 12 &&
           *day >= 1 && *day <= days_in_month(*year, *month);
}

/* "90D" -> amount 90, unit U_DAY. Digits only, no sign, no spaces. */
static bool parse_input(const char *text, int64_t *amount, enum unit *unit)
{
    if (!isdigit((unsigned char)text[0])) {
        return false;
    }
    char *end;
    errno = 0;
    long long n = strtoll(text, &end, 10);
    if (errno != 0 || n > MAX_INPUT) {
        return false;
    }
    *amount = n;
    return parse_unit(end, unit);
}

int main(int argc, char **argv)
{
    int year, month, day;
    enum unit limit = U_SECOND, total_unit = U_SECOND;
    bool total = false, have_unit = false;
    const char *input = NULL;

    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    year = local->tm_year + 1900;
    month = local->tm_mon + 1;
    day = local->tm_mday;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        bool want_unit = !strcmp(arg, "-u") || !strcmp(arg, "--unit");
        bool want_total = !strcmp(arg, "-t") || !strcmp(arg, "--total");
        bool want_from = !strcmp(arg, "-f") || !strcmp(arg, "--from");

        if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
            usage(argv[0]);
            return EXIT_SUCCESS;
        } else if (want_unit || want_total || want_from) {
            if (++i >= argc) {
                fprintf(stderr, "Error: %s needs a value\n", arg);
                return EXIT_FAILURE;
            }
            if ((want_unit || want_total) &&
                !parse_unit(argv[i], want_unit ? &limit : &total_unit)) {
                fprintf(stderr, "Error: unknown unit '%s' (use Y, M, W, D, H, mm, s)\n", argv[i]);
                return EXIT_FAILURE;
            }
            have_unit |= want_unit;
            total |= want_total;
            if (want_from && !parse_date(argv[i], &year, &month, &day)) {
                fprintf(stderr, "Error: invalid date '%s' (use DD/MM/YY)\n", argv[i]);
                return EXIT_FAILURE;
            }
        } else if (input == NULL && arg[0] != '-') {
            input = arg;
        } else {
            fprintf(stderr, "Error: unexpected argument '%s'\n", arg);
            usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (total && have_unit) {
        fprintf(stderr, "Error: use either -u or -t, not both\n");
        return EXIT_FAILURE;
    }

    int64_t amount;
    enum unit in_unit;
    if (input == NULL || !parse_input(input, &amount, &in_unit)) {
        fprintf(stderr, "Error: need input like 90D (non-negative integer + Y, M, W, D, H, mm or s)\n");
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    /* end = start + input, as (end_days, end_sod) with start at 00:00:00. */
    int64_t start_days = days_from_civil(year, (unsigned)month, (unsigned)day);
    int64_t end_days, end_sod = 0;
    static const int64_t secs[U_COUNT] = { 0, 0, 604800, 86400, 3600, 60, 1 };

    if (in_unit == U_YEAR || in_unit == U_MONTH) {
        int64_t months = in_unit == U_YEAR ? amount * 12 : amount;
        if (months > 12 * 8000) {
            fprintf(stderr, "Error: value too large\n");
            return EXIT_FAILURE;
        }
        end_days = add_months(year, month, day, months);
    } else {
        int64_t total = start_days * 86400 + amount * secs[in_unit];
        end_days = total / 86400;
        end_sod = total % 86400;
    }
    if (end_days > days_from_civil(9999, 12, 31)) {
        fprintf(stderr, "Error: result is past year 9999\n");
        return EXIT_FAILURE;
    }

    /* Largest k with start + k months <= end; k months span <= 31k days. */
    int64_t k = (end_days - start_days) / 31;
    while (add_months(year, month, day, k + 1) <= end_days) {
        k++;
    }
    if (total) {
        int64_t count = total_unit == U_YEAR ? k / 12 : k;
        if (total_unit >= U_WEEK) {
            count = ((end_days - start_days) * 86400 + end_sod) / secs[total_unit];
        }
        printf("%" PRId64 " %s%s\n", count, names[total_unit], count == 1 ? "" : "s");
        return EXIT_SUCCESS;
    }
    int64_t rem = (end_days - add_months(year, month, day, k)) * 86400 + end_sod;

    /* Weeks only appear when -u W is the stop unit; otherwise days stay days. */
    int64_t days = rem / 86400;
    int64_t weeks = limit == U_WEEK ? days / 7 : 0;
    if (limit == U_WEEK) {
        days = 0;
    }
    int64_t parts[U_COUNT] = {
        k / 12, k % 12, weeks, days, rem % 86400 / 3600, rem % 3600 / 60, rem % 60
    };

    bool printed = false;
    for (int i = 0; i <= (int)limit; i++) {
        if (parts[i] == 0) {
            continue;
        }
        printf("%s%" PRId64 " %s%s", printed ? " " : "", parts[i], names[i],
               parts[i] == 1 ? "" : "s");
        printed = true;
    }
    if (!printed) {
        printf("0 %ss", names[limit]);
    }
    putchar('\n');
    return EXIT_SUCCESS;
}
