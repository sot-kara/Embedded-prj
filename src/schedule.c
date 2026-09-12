#include "schedule.h"
#include <stdio.h>

bool read_schedule(const char *filepath, time_t *start_time, time_t *end_time) {
    FILE *sched_file = fopen(filepath, "r");
    if (!sched_file) {
        perror("Failed to open schedule file");
        return false;
    }

    if (fscanf(sched_file, "%ld\n%ld", start_time, end_time) != 2) {
        fprintf(stderr, "Failed to parse start and end times from %s\n", filepath);
        fclose(sched_file);
        return false;
    }

    fclose(sched_file);
    return true;
}