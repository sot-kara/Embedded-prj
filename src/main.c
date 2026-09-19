#include <stdio.h>
#include "websocket.h"
#include "producer.h"
#include "consumer.h"
#include "monitor.h"
#include "schedule.h"
#include "circ_buffer.h"
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h> 

#define SLEEP_SECONDS 5 // Sleep duration in seconds
#define SCHEDULE_PATH "/home/sot/bluesky-telemetry/schedule.txt" // Path to the schedule file

int commit_cnt =0, identity_cnt =0, account_cnt =0, info_cnt = 0;

 circ_buff_t buffer = {
    .data = {{0}},
    .head = 0,
    .tail = 0,
    .size = 0
};

volatile int is_connected = 0; // Shared connection status flag

int main(){
    time_t start_time, end_time; // dont start your crying its the end of the times

    if (!read_schedule(SCHEDULE_PATH, &start_time, &end_time)) {
        fprintf(stderr, "Could not read schedule file: %s, continuing with default values\n", SCHEDULE_PATH);
        start_time = time(NULL);
        end_time = start_time + SLEEP_SECONDS; // Default to SLEEP_SECONDS seconds from now
    }



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
    monitor_init(&monitor, &buffer, &counter_mutex, &commit_cnt, &identity_cnt, &account_cnt, &info_cnt, &is_connected);
    
    // wait till the exact absolute time for the start of the experiment is reached
    struct timespec ts_start;
    ts_start.tv_sec = start_time;
    ts_start.tv_nsec = 0;
    while (clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &ts_start, NULL) != 0) {
        // Interrupted by signal, loop back to sleep
    }
   
    // create the producer thread and pass the producer object to it as args
    pthread_t producer_thread;
    pthread_create(&producer_thread, NULL, producer_thread_func, &producer);

    pthread_t consumer_thread;
    pthread_create(&consumer_thread, NULL, consumer_thread_func, &consumer);

    pthread_t monitor_thread;
    pthread_create(&monitor_thread, NULL, monitor_thread_func, &monitor);

    struct timespec ts_end;
    ts_end.tv_sec = end_time;
    ts_end.tv_nsec = 0;
    while (clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &ts_end, NULL) != 0) {
        // Interrupted by signal, loop back to sleep
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