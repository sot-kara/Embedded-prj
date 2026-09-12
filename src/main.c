#include <stdio.h>
#include "websocket.h"
#include "producer.h"
#include "consumer.h"
#include "monitor.h"
#include "circ_buffer.h"
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h> 

#define SLEEP_SECONDS 10 // Sleep duration in seconds

int commit_cnt, identity_cnt, account_cnt, info_cnt = 0;

 circ_buff_t buffer = {
    .data = {{0}},
    .head = 0,
    .tail = 0,
    .size = 0
};

volatile int is_connected = 0; // Shared connection status flag

int main(){
    struct timespec end_time; // dont start your crying its the end of the times

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
    producer_init(&producer, &buffer, &circ_buffer_mutex, &not_full, &not_empty, &is_connected);

    consumer_t consumer;
    consumer_init(&consumer, &buffer, &circ_buffer_mutex, &counter_mutex, &not_full, &not_empty, &commit_cnt, &identity_cnt, &account_cnt, &info_cnt);

    monitor_t monitor;
    monitor_init(&monitor, &buffer, &counter_mutex, (unsigned int*)&commit_cnt, (unsigned int*)&identity_cnt, (unsigned int*)&account_cnt, (unsigned int*)&info_cnt, &is_connected);

    // get current time after initialization and befire starting the threads
   clock_gettime(CLOCK_REALTIME, &end_time);

    // Add exactly 24 hours (86400 seconds) to the current time
    end_time.tv_sec += SLEEP_SECONDS;
   
    // create the producer thread and pass the producer object to it as args
    pthread_t producer_thread;
    pthread_create(&producer_thread, NULL, producer_thread_func, &producer);

    pthread_t consumer_thread;
    pthread_create(&consumer_thread, NULL, consumer_thread_func, &consumer);

    pthread_t monitor_thread;
    pthread_create(&monitor_thread, NULL, monitor_thread_func, &monitor);

    // Sleep until the exact absolute time for 24 hours is reached
    // If interrupted by a system signal, the while loop forces it right back to sleep
    while (clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &end_time, NULL) != 0) {
        // Optionally check an external kill switch here if needed
    }

    // Signal the threads to stop running
    printf("Stopping threads...\n");
    atomic_store(&producer.running, 0);
    atomic_store(&consumer.running, 0);
    atomic_store(&monitor.running, 0);

    // cleanup
    pthread_join(producer_thread, NULL);
    pthread_join(consumer_thread, NULL);
    pthread_join(monitor_thread, NULL);
    pthread_mutex_destroy(&circ_buffer_mutex);
    pthread_mutex_destroy(&counter_mutex);
    pthread_cond_destroy(&not_full);
    pthread_cond_destroy(&not_empty);

    printf("Experiment finished...\n");
}