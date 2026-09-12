#ifndef PRODUCER_H
#define PRODUCER_H

#include "circ_buffer.h"
#include <pthread.h>

typedef struct {
    circ_buff_t *buffer;
    pthread_mutex_t *circ_buff_mutex;
    pthread_cond_t *not_full;
    pthread_cond_t *not_empty;
    volatile int running;
    volatile int *is_connected;
} producer_t;

void producer_init(producer_t *prod, circ_buff_t *buffer, pthread_mutex_t *mutex, 
                   pthread_cond_t *not_full, pthread_cond_t *not_empty, volatile int *is_connected);
void* producer_thread_func(void *arg);

#endif