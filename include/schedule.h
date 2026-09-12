#ifndef SCHEDULE_H
#define SCHEDULE_H

#include <time.h>
#include <stdbool.h>

/* Reads the start and end Unix timestamps from the specified configuration file.
 * Returns true on success, false if the file cannot be read or parsed. */
bool read_schedule(const char *filepath, time_t *start_time, time_t *end_time);

#endif