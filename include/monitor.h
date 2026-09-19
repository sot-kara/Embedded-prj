#ifndef MONITOR_H
#define MONITOR_H

#include "circ_buffer.h"
#include <pthread.h>

/* Structure to hold references to shared state for the monitor thread */
typedef struct {
    circ_buff_t *buffer;
    pthread_mutex_t *counters_mutex;
    int *received_count;
    int *commit_count;
    int *identity_count;
    int *account_count;
    int *unknown_count;
    int *info_count;
    volatile int running;
    volatile int *is_connected;
} monitor_t;

extern volatile int peak_buffer_size; // Global variable to track peak buffer size

void monitor_init(monitor_t *mon, circ_buff_t *buffer, pthread_mutex_t *counters_mutex, int *received_count,
                  int *commit_count, int *identity_count,
                  int *account_count, int *unknown_count, int *info_count,volatile int *is_connected);

void* monitor_thread_func(void *arg);

#endif