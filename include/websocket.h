#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <stddef.h>
#include <stdbool.h>

/*
 * Callback invoked when data is received from the WebSocket.
 *
 * payload  - pointer to received data
 * len      - number of bytes received
 * user_data - user-defined pointer passed to ws_client_create()
 *
 * NOTE:
 * payload is NOT guaranteed to be null-terminated.
 */
typedef void (*ws_data_callback_t)(
    const char *payload,
    size_t len,
    void *user_data
);

typedef struct ws_client_ctx ws_client_ctx_t;

/*
 * Create a WebSocket client.
 *
 * address  - server hostname/IP
 * port     - server port
 * path     - WebSocket path, e.g. "/api/ws"
 * callback - function called when data is received
 * user_data - user-defined pointer passed to callback
 *
 * Returns:
 *     Pointer to client context on success
 *     NULL on failure
 */
ws_client_ctx_t* ws_client_create(
    const char *address,
    int port,
    const char *path,
    ws_data_callback_t callback,
    void *user_data
);

/*
 * Run the WebSocket event loop.
 *
 * The function keeps processing WebSocket events while:
 *
 *     *running != 0
 *
 * If the connection is lost, the client automatically attempts
 * to reconnect using exponential backoff.
 */
void ws_client_run(
    ws_client_ctx_t *ctx,
    volatile int *running
);

/*
 * Destroy the WebSocket client and release all resources.
 */
void ws_client_destroy(ws_client_ctx_t *ctx);

#endif