#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s <HH:mm:ss> [message]\n", argv[0]);
        fprintf(stderr, "Example: %s 14:30:00 \"Take your medication!\"\n", argv[0]);
        return EXIT_FAILURE;
    }

    int target_hour, target_min, target_sec;
    if (sscanf(argv[1], "%d:%d:%d", &target_hour, &target_min, &target_sec) != 3 ||
        target_hour < 0 || target_hour > 23 ||
        target_min < 0 || target_min > 59 ||
        target_sec < 0 || target_sec > 59) {
        fprintf(stderr, "Error: Invalid time format. Use HH:mm:ss in 24-hour format (00:00:00 to 23:59:59).\n");
        return EXIT_FAILURE;
    }

    // Default message if non provided
    const char *message = (argc == 3) ? argv[2] : "Time is up! Click dismiss to stop.";

    time_t now = time(NULL);
    struct tm *tm_target = localtime(&now);

    // Update target hour, minute, and second
    tm_target->tm_hour = target_hour;
    tm_target->tm_min = target_min;
    tm_target->tm_sec = target_sec;

    time_t target_time = mktime(tm_target);

    // If target time is earlier today, schedule it for tomorrow
    if (target_time <= now) {
        tm_target->tm_mday += 1;
        target_time = mktime(tm_target);
    }

    double seconds_to_wait = difftime(target_time, now);
    int wait_hours = (int)seconds_to_wait / 3600;
    int wait_mins = ((int)seconds_to_wait % 3600) / 60;
    int wait_secs = (int)seconds_to_wait % 60;

    printf("Alarm set for %02d:%02d:%02d (waiting %dh %dm %ds)\n",
           target_hour, target_min, target_sec, wait_hours, wait_mins, wait_secs);
    printf("Message: \"%s\"\n", message);

    sleep((unsigned int)seconds_to_wait);

    printf("⏰ Time is up! Triggering alarm notification...\n");

    // Construct command with custom message
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "notify-send -u critical -t 0 \"⏰ ALARM\" \"%s\" --action=\"dismiss=Dismiss\"",
             message);

    FILE *fp = popen(cmd, "r");
    if (fp == NULL) {
        perror("Failed to run notify-send");
        return EXIT_FAILURE;
    }

    char response[64];
    if (fgets(response, sizeof(response), fp) != NULL) {
        printf("Action received: %s", response);
    }

    pclose(fp);
    printf("Alarm dismissed!\n");

    return EXIT_SUCCESS;
}
