#include <stdio.h>
#include "websocket.h"
#include "producer.h"
#include "consumer.h"
#include "circ_buffer.h"
#include <pthread.h>
#include <unistd.h> // for sleep`

int commit_cnt, identity_cnt, account_cnt, info_cnt = 0;

 circ_buff_t buffer = {
    .data = {0},
    .head = 0,
    .tail = 0,
    .size = 0
};

int main(){
    pthread_mutex_t circ_buffer_mutex;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;

    pthread_mutex_t counter_mutex;

    circ_buff_init(&buffer);

    // pthread variables initialization
    pthread_mutex_init(&circ_buffer_mutex, NULL);
    pthread_mutex_init(&counter_mutex, NULL);
    pthread_cond_init(&not_full, NULL);
    pthread_cond_init(&not_empty, NULL);

    // create the producer object and initialize it
    producer_t producer;
    producer_init(&producer, &buffer, &circ_buffer_mutex, &not_full, &not_empty);

    consumer_t consumer;
    consumer_init(&consumer, &buffer, &circ_buffer_mutex, &counter_mutex, &not_full, &not_empty, &commit_cnt, &identity_cnt, &account_cnt, &info_cnt);

    // create the producer thread and pass the producer object to it as args
    pthread_t producer_thread;
    pthread_create(&producer_thread, NULL, producer_thread_func, &producer);

    pthread_t consumer_thread;
    pthread_create(&consumer_thread, NULL, consumer_thread_func, &consumer);

    sleep(1); // Let the producer run for 10 seconds

    // cleanup
    pthread_join(producer_thread, NULL);
    pthread_join(consumer_thread, NULL);
    pthread_mutex_destroy(&circ_buffer_mutex);
    pthread_mutex_destroy(&counter_mutex);
    pthread_cond_destroy(&not_full);
    pthread_cond_destroy(&not_empty);

}