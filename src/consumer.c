#include "consumer.h"
#include "json_handler.h"

void consumer_init(consumer_t *cons, circ_buff_t *buffer, pthread_mutex_t *circ_buff_mutex, pthread_mutex_t *counter_mutex,
                   pthread_cond_t *not_full, pthread_cond_t *not_empty, int *commit_count, int *identity_count, int *account_count, int *info_count) {
    cons->buffer = buffer;
    cons->circ_buff_mutex = circ_buff_mutex;
    cons->counter_mutex = counter_mutex;
    cons->not_full = not_full;
    cons->not_empty = not_empty;
    cons->commit_count = commit_count;
    cons->identity_count = identity_count;
    cons->account_count = account_count;
    cons->info_count = info_count;
    cons->running = 1;
}

void *consumer_thread_func(void *arg) {
    consumer_t *cons = (consumer_t *)arg;
    char data[MAX_JSON_LEN];

    while (cons->running) {
        pthread_mutex_lock(cons->circ_buff_mutex);

        // Wait until the buffer is not empty, pthread_cond_wait releases the mutex and blocks the thread until the condition variable is signaled. 
        // When it wakes up, it re-acquires the mutex.
        while (circ_buff_is_empty(cons->buffer)) {
            pthread_cond_wait(cons->not_empty, cons->circ_buff_mutex);
        }

        // Pop data from the circular buffer
        circ_buff_pop(cons->buffer, data);

        // Signal that the buffer is not full
        pthread_cond_signal(cons->not_full);
        pthread_mutex_unlock(cons->circ_buff_mutex);

        // --- PROCESS DATA ---
        message_kind_t kind = parse_msg_kind(data);

        // lock the counter mutex before updating counters
        pthread_mutex_lock(cons->counter_mutex);
        switch (kind)
        {
        case MSG_KIND_COMMIT:
            (*cons->commit_count)++;
            break;
        case MSG_KIND_IDENTITY:
            // Handle identity message
            (*cons->identity_count)++;
            break;
        case MSG_KIND_ACCOUNT:
            // Handle account message
            (*cons->account_count)++;
            break;
        case MSG_KIND_INFO:
            // Handle info message
            (*cons->info_count)++;
            break;
        default:
            break;
        }
        pthread_mutex_unlock(cons->counter_mutex); // release the counter mutex after updating counters so the monitor thread can read the values
        

    }

    return NULL;
}