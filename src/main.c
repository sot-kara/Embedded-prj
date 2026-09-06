#include <stdio.h>
#include "websocket.h"
#include "producer.h"
#include "circ_buffer.h"
#include <pthread.h>

int commit_cnt, identity_cnt, account_cnt, info_cnt = 0;


int main(){
    circ_buffer_t *buffer;
    pthread_mutex_t circ_buffer_mutex;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;

    pthread_mutex_t counter_mutex;

    circ_buff_init(buffer);

    // pthread variables initialization
    pthread_mutex_init(&circ_buffer_mutex, NULL);
    pthread_mutex_init(&counter_mutex, NULL);
    pthread_cond_init(&not_full, NULL);
    pthread_cond_init(&not_empty, NULL);

    // create the producer object and initialize it
    producer_t producer;
    producer_init(&producer, buffer, &circ_buffer_mutex, &not_full, &not_empty);

    // create the producer thread and pass the producer object to it as args
    pthread_t producer_thread;
    pthread_create(&producer_thread, NULL, producer_thread_func, &producer);

}