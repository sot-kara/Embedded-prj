#ifndef CONSUMER_H
#define CONSUMER_H
#include "circ_buffer.h"
#include <pthread.h>

// Consumer function declarations
typedef struct {
    circ_buff_t *buffer;
    pthread_mutex_t *circ_buff_mutex;
    pthread_mutex_t *counter_mutex;
    pthread_cond_t *not_full;
    pthread_cond_t *not_empty;
    volatile int running;
    int *commit_count;
    int *identity_count;
    int *account_count;
    int *info_count;
} consumer_t;

void consumer_init(consumer_t *cons, circ_buff_t *buffer, pthread_mutex_t *circ_buff_mutex, pthread_mutex_t * counter_mutex, 
                   pthread_cond_t *not_full, pthread_cond_t *not_empty, int *commit_count, int *identity_count, int *account_count, int *info_count);

void* consumer_thread_func(void* args);

#endif // CONSUMER_H