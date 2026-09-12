#ifndef MONITOR_H
#define MONITOR_H

#include "circ_buffer.h"
#include <pthread.h>

/* Structure to hold references to shared state for the monitor thread */
typedef struct {
    circ_buff_t *buffer;
    pthread_mutex_t *counters_mutex;
    unsigned int *commit_count;
    unsigned int *identity_count;
    unsigned int *account_count;
    unsigned int *info_count;
    volatile int running;
    volatile int *is_connected;
} monitor_t;

void monitor_init(monitor_t *mon, circ_buff_t *buffer, pthread_mutex_t *counters_mutex,
                  unsigned int *commit_count, unsigned int *identity_count,
                  unsigned int *account_count, unsigned int *info_count,volatile int *is_connected);

void* monitor_thread_func(void *arg);

#endif