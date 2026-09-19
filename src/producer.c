#include "producer.h"
#include "websocket.h"
#include <stdio.h>

static void on_websocket_data(const char *payload, size_t len, void *user_data) {
    producer_t *prod = (producer_t *)user_data;

    pthread_mutex_lock(prod->circ_buff_mutex);

    // Enforce the Bounded Circular Queue requirement: block if full
    while (circ_buff_is_full(prod->buffer)) {
        pthread_cond_wait(prod->not_full, prod->circ_buff_mutex);
    }

    circ_buff_push(prod->buffer, payload, len);
    //printf("Producer pushed data to buffer: %.*s\n", (int)len, payload);
    // Wake up the consumer thread
    pthread_cond_signal(prod->not_empty);
    pthread_mutex_unlock(prod->circ_buff_mutex);
}

void producer_init(producer_t *prod, circ_buff_t *buffer, pthread_mutex_t *circ_buff_mutex, 
                   pthread_cond_t *not_full, pthread_cond_t *not_empty, volatile int *is_connected) {
    prod->buffer = buffer;
    prod->circ_buff_mutex = circ_buff_mutex;
    prod->not_full = not_full;
    prod->not_empty = not_empty;
    prod->running = 1;
    prod->is_connected = is_connected;
}

void* producer_thread_func(void *arg) {
    producer_t *prod = (producer_t *)arg;

    ws_client_ctx_t *ws_client = ws_client_create(
        "jetstream1.us-east.bsky.network",
        443,
        "/subscribe?wantedCollections=app.bsky.feed.post",
        on_websocket_data,
        prod,
        prod->is_connected
    );

    if (!ws_client) {
        fprintf(stderr, "Producer failed to initialize WebSocket client connection.\n");
        return NULL;
    }

    ws_client_run(ws_client, &prod->running);

    ws_client_destroy(ws_client);
    return NULL;
}