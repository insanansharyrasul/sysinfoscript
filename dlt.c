#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum unit {
    UNIT_AUTO,
    UNIT_DAY,
    UNIT_HOUR,
    UNIT_MINUTE,
    UNIT_SECOND
};

struct value {
    int64_t seconds;
    bool has_date;
};

static void usage(const char *program)
{
    printf("Usage: %s [options] <left> <right>\n", program);
    printf("       %s [options] \"<left> - <right>\"\n\n", program);
    printf("Values: HH:MM[:SS] or HH.MM[.SS], DD/MM/YY, or a date-time\n");
    printf("Options:\n");
    printf("  -u, --unit UNIT  auto, day, hour, minute, or second (default: auto)\n");
    printf("  -d, --decimal    print one decimal value in the selected unit\n");
    printf("  -s, --signed     keep the sign instead of using an absolute delta\n");
    printf("  -h, --help       show this help\n");
}

static bool valid_date(int year, int month, int day)
{
    static const int days_in_month[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);

    if (month < 1 || month > 12 || day < 1) {
        return false;
    }
    return day <= days_in_month[month - 1] + (month == 2 && leap);
}

/* Days since 1970-01-01, using the proleptic Gregorian calendar. */
static int64_t days_from_civil(int year, unsigned month, unsigned day)
{
    year -= month <= 2;
    int era = (year >= 0 ? year : year - 399) / 400;
    unsigned year_of_era = (unsigned)(year - era * 400);
    unsigned day_of_year = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5
                           + day - 1;
    unsigned day_of_era = year_of_era * 365 + year_of_era / 4
                          - year_of_era / 100 + day_of_year;
    return (int64_t)era * 146097 + (int64_t)day_of_era - 719468;
}

static bool parse_time(const char *text, int *hour, int *minute, int *second)
{
    char normalized[32];
    size_t length = strlen(text);
    bool has_dot = strchr(text, '.') != NULL;
    bool has_colon = strchr(text, ':') != NULL;
    if (has_dot && has_colon) {
        return false;
    }
    if (has_dot) {
        if (length >= sizeof(normalized)) {
            return false;
        }
        memcpy(normalized, text, length + 1);
        for (size_t i = 0; i < length; i++) {
            if (normalized[i] == '.') {
                normalized[i] = ':';
            }
        }
        text = normalized;
    }

    int count = 0;
    if (sscanf(text, "%d:%d:%d%n", hour, minute, second, &count) == 3 &&
        text[count] == '\0') {
        return *hour >= 0 && *hour <= 23 &&
               *minute >= 0 && *minute <= 59 &&
               *second >= 0 && *second <= 59;
    }

    count = 0;
    if (sscanf(text, "%d:%d%n", hour, minute, &count) == 2 &&
        text[count] == '\0') {
        *second = 0;
        return *hour >= 0 && *hour <= 23 &&
               *minute >= 0 && *minute <= 59;
    }
    return false;
}

static bool parse_date(const char *text, int *year, int *month, int *day)
{
    int short_year;
    int count = 0;
    if (sscanf(text, "%d/%d/%d%n", day, month, &short_year, &count) != 3 ||
        text[count] != '\0') {
        return false;
    }

    if (short_year >= 0 && short_year <= 99) {
        *year = short_year >= 69 ? 1900 + short_year : 2000 + short_year;
    } else {
        *year = short_year;
    }
    return valid_date(*year, *month, *day);
}

static bool parse_value(const char *text, struct value *value)
{
    char buffer[128];
    size_t length = strlen(text);
    int year, month, day, hour, minute, second;
    const char *separator;

    while (length > 0 && isspace((unsigned char)text[0])) {
        text++;
        length--;
    }
    while (length > 0 && isspace((unsigned char)text[length - 1])) {
        length--;
    }
    if (length == 0 || length >= sizeof(buffer)) {
        return false;
    }
    memcpy(buffer, text, length);
    buffer[length] = '\0';

    if (parse_time(buffer, &hour, &minute, &second)) {
        value->seconds = (int64_t)hour * 3600 + minute * 60 + second;
        value->has_date = false;
        return true;
    }

    if (parse_date(buffer, &year, &month, &day)) {
        value->seconds = days_from_civil(year, (unsigned)month, (unsigned)day)
                         * 86400;
        value->has_date = true;
        return true;
    }

    separator = strchr(buffer, 'T');
    if (separator == NULL) {
        separator = strchr(buffer, ' ');
    }
    if (separator == NULL || separator == buffer) {
        return false;
    }

    char date_part[32];
    size_t date_length = (size_t)(separator - buffer);
    if (date_length >= sizeof(date_part) ||
        strlen(separator + 1) >= sizeof(date_part)) {
        return false;
    }
    memcpy(date_part, buffer, date_length);
    date_part[date_length] = '\0';
    if (!parse_date(date_part, &year, &month, &day) ||
        !parse_time(separator + 1, &hour, &minute, &second)) {
        return false;
    }
    value->seconds = days_from_civil(year, (unsigned)month, (unsigned)day)
                     * 86400 + (int64_t)hour * 3600 + minute * 60 + second;
    value->has_date = true;
    return true;
}

static char *trim(char *text)
{
    char *end;
    while (isspace((unsigned char)*text)) {
        text++;
    }
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) {
        *--end = '\0';
    }
    return text;
}

static bool split_expression(char *expression, char **left, char **right)
{
    char *separator = strchr(expression, '-');
    if (separator == NULL) {
        return false;
    }
    *separator = '\0';
    *left = trim(expression);
    *right = trim(separator + 1);
    return **left != '\0' && **right != '\0';
}

static bool parse_unit(const char *text, enum unit *unit)
{
    if (strcmp(text, "auto") == 0) *unit = UNIT_AUTO;
    else if (strcmp(text, "day") == 0 || strcmp(text, "d") == 0) *unit = UNIT_DAY;
    else if (strcmp(text, "hour") == 0 || strcmp(text, "h") == 0) *unit = UNIT_HOUR;
    else if (strcmp(text, "minute") == 0 || strcmp(text, "min") == 0 ||
             strcmp(text, "m") == 0) *unit = UNIT_MINUTE;
    else if (strcmp(text, "second") == 0 || strcmp(text, "sec") == 0 ||
             strcmp(text, "s") == 0) *unit = UNIT_SECOND;
    else return false;
    return true;
}

static const char *unit_name(enum unit unit, uint64_t amount)
{
    static const char *singular[] = {"", "day", "hour", "minute", "second"};
    static const char *plural[] = {"", "days", "hours", "minutes", "seconds"};
    return amount == 1 ? singular[unit] : plural[unit];
}

static void print_delta(uint64_t seconds, enum unit selected, bool decimal,
                        bool negative)
{
    const uint64_t factors[] = {0, 86400, 3600, 60, 1};
    enum unit unit = selected;
    uint64_t amount;
    char sign = negative ? '-' : '\0';

    if (unit == UNIT_AUTO) {
        unit = seconds >= factors[UNIT_DAY] ? UNIT_DAY :
               seconds >= factors[UNIT_HOUR] ? UNIT_HOUR :
               seconds >= factors[UNIT_MINUTE] ? UNIT_MINUTE : UNIT_SECOND;
    }
    amount = seconds / factors[unit];

    if (decimal) {
        double result = (double)seconds / (double)factors[unit];
        if (sign) putchar(sign);
        printf("%.6f %s\n", result, unit_name(unit, amount));
        return;
    }

    if (sign) putchar(sign);
    if (selected == UNIT_AUTO) {
        uint64_t remainder = seconds;
        bool printed = false;
        for (enum unit current = UNIT_DAY; current <= UNIT_SECOND; current++) {
            uint64_t current_amount = remainder / factors[current];
            remainder %= factors[current];
            if (current_amount > 0) {
                if (printed) printf(" ");
                printf("%" PRIu64 " %s", current_amount,
                       unit_name(current, current_amount));
                printed = true;
            }
        }
        if (!printed) {
            printf("0 seconds");
        }
        putchar('\n');
        return;
    }

    printf("%" PRIu64 " %s", amount, unit_name(unit, amount));
    uint64_t remainder = seconds % factors[unit];
    for (enum unit current = unit + 1; current <= UNIT_SECOND; current++) {
        uint64_t current_amount = remainder / factors[current];
        remainder %= factors[current];
        if (current_amount > 0) {
            printf(" %" PRIu64 " %s", current_amount,
                   unit_name(current, current_amount));
        }
    }
    putchar('\n');
}

int main(int argc, char **argv)
{
    enum unit unit = UNIT_AUTO;
    bool decimal = false;
    bool signed_output = false;
    char *operands[2] = {NULL, NULL};
    char expression[256];
    int operand_count = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return EXIT_SUCCESS;
        }
        if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--decimal") == 0) {
            decimal = true;
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--signed") == 0) {
            signed_output = true;
        } else if (strcmp(argv[i], "-u") == 0 || strcmp(argv[i], "--unit") == 0) {
            if (++i >= argc || !parse_unit(argv[i], &unit)) {
                fprintf(stderr, "Error: unit must be auto, day, hour, minute, or second.\n");
                return EXIT_FAILURE;
            }
        } else if (strncmp(argv[i], "--unit=", 7) == 0) {
            if (!parse_unit(argv[i] + 7, &unit)) {
                fprintf(stderr, "Error: invalid unit '%s'.\n", argv[i] + 7);
                return EXIT_FAILURE;
            }
        } else if (operand_count < 2) {
            operands[operand_count++] = argv[i];
        } else {
            fprintf(stderr, "Error: expected two values or one subtraction expression.\n");
            return EXIT_FAILURE;
        }
    }

    if (operand_count == 1) {
        size_t length = strlen(operands[0]);
        if (length >= sizeof(expression)) {
            fprintf(stderr, "Error: expression is too long.\n");
            return EXIT_FAILURE;
        }
        memcpy(expression, operands[0], length + 1);
        if (!split_expression(expression, &operands[0], &operands[1])) {
            fprintf(stderr, "Error: use '<left> - <right>'.\n");
            return EXIT_FAILURE;
        }
        operand_count = 2;
    }

    if (operand_count != 2) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    struct value left, right;
    if (!parse_value(operands[0], &left) || !parse_value(operands[1], &right)) {
        fprintf(stderr, "Error: invalid value. Use HH:MM[:SS], DD/MM/YY, or a date with time.\n");
        return EXIT_FAILURE;
    }
    if (left.has_date != right.has_date) {
        fprintf(stderr, "Error: both values must be times, dates, or date-times.\n");
        return EXIT_FAILURE;
    }

    int64_t difference = left.seconds - right.seconds;
    bool negative = difference < 0;
    uint64_t magnitude = negative ? (uint64_t)(-(difference + 1)) + 1
                                  : (uint64_t)difference;
    print_delta(magnitude, unit, decimal, signed_output && negative);
    return EXIT_SUCCESS;
}