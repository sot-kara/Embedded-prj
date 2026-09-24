#include "websocket.h"
#include <libwebsockets.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

/*
 * Reconnection configuration
 */
#define WS_RECONNECT_INITIAL_DELAY 1
#define WS_RECONNECT_MAX_DELAY 60
#define WS_IDLE_TIMEOUT_MS 5000

/* Get the current monotonic time. */
static void ts_now(struct timespec *ts)
{
    clock_gettime(CLOCK_MONOTONIC, ts);
}

/* Milliseconds elapsed from `earlier` to `later`. */
static long long ts_diff_ms(const struct timespec *later, const struct timespec *earlier)
{
    return (long long)(later->tv_sec - earlier->tv_sec) * 1000LL +
           (later->tv_nsec - earlier->tv_nsec) / 1000000LL;
}

/*
 * Client context
 */
struct ws_client_ctx
{
    struct lws_context *context;
    struct lws *wsi;

    ws_data_callback_t callback;
    void *user_data;
    
    /* Pointer to shared connection status flag */
    volatile int *is_connected; 

    /*
     * Connection information.
     *
     * Store copies so they remain available when
     * trying to reconnect.
     */
    char *address;
    int port;
    char *path;

    /*
     * Reconnection state.
     */
    int reconnect_delay;
    time_t next_reconnect_time;

    bool connected;

    // Buffer for handling fragmented data
    char *rx_buffer;
    size_t rx_buffer_len;

    /* Monotonic time we last received any data (used for idle watchdog) */
    struct timespec last_rx_ts;
};

static int ws_client_connect(ws_client_ctx_t *client);

/*
 * WebSocket callback
 */
static int ws_client_callback(
    struct lws *wsi,
    enum lws_callback_reasons reason,
    void *user,
    void *in,
    size_t len)
{
    (void)user;

    /*
     * Retrieve our client context from the lws context.
     */
    ws_client_ctx_t *client =
        (ws_client_ctx_t *)lws_context_user(
            lws_get_context(wsi));

    if (!client)
    {
        return 0;
    }

    switch (reason)
    {

    /*
     * ---------------------------------------------------------
     * Connection established
     * ---------------------------------------------------------
     */
    case LWS_CALLBACK_CLIENT_ESTABLISHED:

        client->connected = true;
        client->wsi = wsi;
        
        /* Update the shared connection flag to valid (1) */
        if (client->is_connected) {
            *(client->is_connected) = 1;
        }

        /*
         * The connection worked, so reset the reconnect
         * backoff.
         */
        client->reconnect_delay =
            WS_RECONNECT_INITIAL_DELAY;

        client->next_reconnect_time = 0;

        /* Reset the idle watchdog clock for the new connection */
        ts_now(&client->last_rx_ts);

        lwsl_user(
            "WebSocket connection established successfully.\n");

        break;
     /*       
     * ---------------------------------------------------------
     * Data received
     * ---------------------------------------------------------
     */
    case LWS_CALLBACK_CLIENT_RECEIVE:
        if (in && len > 0 && client->callback)
        {
            /* Any bytes at all count as proof the connection is alive */
            ts_now(&client->last_rx_ts);

            /* 1. Expand the buffer to fit the new fragment */
            char *new_buf = realloc(client->rx_buffer, client->rx_buffer_len + len + 1);
            if (!new_buf)
            {
                lwsl_err("OOM allocating receive buffer\n");
                return -1; // Returning non-zero drops the connection
            }

            client->rx_buffer = new_buf;

            /* 2. Copy the new data to the end of our buffer */
            memcpy(client->rx_buffer + client->rx_buffer_len, in, len);
            client->rx_buffer_len += len;
            
            /* Null-terminate it just to be safe for string parsing */
            client->rx_buffer[client->rx_buffer_len] = '\0'; 

            /* 3. Check if this is the end of the full message */
            size_t remaining = lws_remaining_packet_payload(wsi);
            int is_final = lws_is_final_fragment(wsi);

            if (is_final && remaining == 0)
            {
                /* We have the complete JSON message. Dispatch it. */
                if (client->callback)
                {
                    client->callback(
                        (const char *)client->rx_buffer,
                        client->rx_buffer_len,
                        client->user_data);
                }

                /* 4. Reset the buffer for the next incoming message */
                free(client->rx_buffer);
                client->rx_buffer = NULL;
                client->rx_buffer_len = 0;
            }
        }
        break;

    /*
     * ---------------------------------------------------------
     * Connection error
     * ---------------------------------------------------------
     */
    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:

        lwsl_err(
            "WebSocket connection error: %s\n",
            in ? (char *)in : "(null)");

        /*
         * The wsi is no longer usable.
         */
        client->wsi = NULL;
        client->connected = false;
        
        /* Update the shared connection flag to invalid (0) */
        if (client->is_connected) {
            *(client->is_connected) = 0;
        }

        /* Free any partially assembled message */
        if (client->rx_buffer) {
            free(client->rx_buffer);
            client->rx_buffer = NULL;
            client->rx_buffer_len = 0;
        }

        /*
         * Schedule a reconnect.
         */
        client->next_reconnect_time =
            time(NULL) + client->reconnect_delay;

        lwsl_user(
            "Will attempt to reconnect in %d seconds.\n",
            client->reconnect_delay);

        /*
         * Increase the delay exponentially.
         */
        client->reconnect_delay *= 2;

        if (client->reconnect_delay >
            WS_RECONNECT_MAX_DELAY)
        {

            client->reconnect_delay =
                WS_RECONNECT_MAX_DELAY;
        }

        break;

    /*
     * ---------------------------------------------------------
     * Connection closed
     * ---------------------------------------------------------
     */
    case LWS_CALLBACK_CLIENT_CLOSED:

        lwsl_user(
            "WebSocket connection closed.\n");

        client->wsi = NULL;
        client->connected = false;

        /* Update the shared connection flag to invalid (0) */
        if (client->is_connected) {
            *(client->is_connected) = 0;
        }

        /* Free any partially assembled message */
        if (client->rx_buffer) {
            free(client->rx_buffer);
            client->rx_buffer = NULL;
            client->rx_buffer_len = 0;
        }

        /*
         * Schedule a reconnect.
         */
        client->next_reconnect_time =
            time(NULL) + client->reconnect_delay;

        lwsl_user(
            "Will attempt to reconnect in %d seconds.\n",
            client->reconnect_delay);


        if (client->reconnect_delay >
            WS_RECONNECT_MAX_DELAY)
        {

            client->reconnect_delay =
                WS_RECONNECT_MAX_DELAY;
        }

        break;

    default:
        break;
    }

    return 0;
}

/*
 * Protocol definition
 */
static struct lws_protocols protocols[] = {
    {
        .name = "jetstream-protocol",
        .callback = ws_client_callback,
        .per_session_data_size = 0,
        .rx_buffer_size = 65536,
    },

    LWS_PROTOCOL_LIST_TERM};

/*
 * Connect to the WebSocket server.
 *
 * This function can be called multiple times, which is what
 * allows us to reconnect after a connection failure.
 */
static int ws_client_connect(ws_client_ctx_t *client)
{
    if (!client)
    {
        return -1;
    }

    if (!client->context)
    {
        return -1;
    }

    /*
     * Do not attempt another connection if one already exists.
     */
    if (client->wsi)
    {
        return 0;
    }

    struct lws_client_connect_info ccinfo;

    memset(&ccinfo, 0, sizeof(ccinfo));

    ccinfo.context = client->context;

    ccinfo.address = client->address;
    ccinfo.port = client->port;
    ccinfo.path = client->path;

    /*
     * Host header.
     */
    ccinfo.host = client->address;

    /*
     * Origin.
     */
    ccinfo.origin = client->address;

    /*
     * WebSocket subprotocol.
     */
    ccinfo.protocol = protocols[0].name;

    /*
     * Use TLS.
     */
    ccinfo.ssl_connection =
        LCCSCF_USE_SSL |
        LCCSCF_ALLOW_SELFSIGNED |
        LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;

    lwsl_user(
        "Attempting WebSocket connection to %s:%d%s...\n",
        client->address,
        client->port,
        client->path);

    client->wsi =
        lws_client_connect_via_info(&ccinfo);

    if (!client->wsi)
    {

        lwsl_err("Failed to initiate WebSocket connection.\n");

        client->connected = false;

        /*
         * Schedule another attempt.
         */
        client->next_reconnect_time =
            time(NULL) + client->reconnect_delay;

        lwsl_user(
            "Will retry in %d seconds.\n",
            client->reconnect_delay);


        if (client->reconnect_delay >
            WS_RECONNECT_MAX_DELAY)
        {

            client->reconnect_delay =
                WS_RECONNECT_MAX_DELAY;
        }

        return -1;
    }

    return 0;
}

/*
 * Create client
 */
ws_client_ctx_t *ws_client_create(
    const char *address,
    int port,
    const char *path,
    ws_data_callback_t callback,
    void *user_data,
    volatile int *is_connected) /* Added the new parameter here */
{
    /*
     * Validate arguments.
     */
    if (!address || !path || !callback)
    {
        return NULL;
    }

    /*
     * Allocate client context.
     */
    ws_client_ctx_t *client =
        malloc(sizeof(ws_client_ctx_t));

    if (!client)
    {
        return NULL;
    }

    memset(client, 0, sizeof(ws_client_ctx_t));

    /*
     * Store callback information.
     */
    client->callback = callback;
    client->user_data = user_data;
    
    /* Store the shared connection flag */
    client->is_connected = is_connected;

    /*
     * Store connection parameters.
     *
     * strdup() creates our own copies, which is important
     * because we need these values when reconnecting later.
     */
    client->address = strdup(address);

    if (!client->address)
    {
        free(client);
        return NULL;
    }

    client->path = strdup(path);

        if (!client->path)
    {
        free(client->address);
        free(client);
        return NULL;
    }

    client->port = port;

    /*
     * Initial reconnect delay.
     */
    client->reconnect_delay =
        WS_RECONNECT_INITIAL_DELAY;

    client->next_reconnect_time = 0;
    client->connected = false;
    client->wsi = NULL;

    /* Initialize the flag to 0 before attempting connection */
    if (client->is_connected) {
        *(client->is_connected) = 0;
    }

    /*
     * ---------------------------------------------------------
     * Create libwebsockets context
     * ---------------------------------------------------------
     */
    struct lws_context_creation_info info;

    memset(&info, 0, sizeof(info));

    /*
     * Client-only context.
     */
    info.port = CONTEXT_PORT_NO_LISTEN;

    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

    /*
     * Protocols used by the client.
     */
    info.protocols = protocols;

    /*
     * TCP keepalive so a socket that has actually died at the network
     * level (as opposed to being merely idle) is detected and torn
     * down by the OS/lws instead of sitting silent indefinitely.
     */
    info.ka_time = 10;
    info.ka_interval = 5;
    info.ka_probes = 3;
    

    /*
     * No specific user/group.
     */
    info.gid = -1;
    info.uid = -1;

    /*
     * Make our client structure available through:
     *
     * lws_context_user(...)
     */
    info.user = client;

    client->context =
        lws_create_context(&info);

    if (!client->context)
    {

        lwsl_err(
            "Failed to create libwebsockets context.\n");

        free(client->path);
        free(client->address);
        free(client);

        return NULL;
    }

    if (ws_client_connect(client) != 0)
    {
        lwsl_user(
            "Initial connection attempt failed; will keep retrying "
            "via the normal reconnect loop.\n");
    }

    return client;
}

/*
 * Run WebSocket event loop
 */
void ws_client_run(
    ws_client_ctx_t *ctx,
    volatile int *running)
{
    if (!ctx || !ctx->context || !running)
    {
        return;
    }

    // time_t last_debug_print = 0;

    while (*running)
    {

        /*
         * Process libwebsockets events.
         *
         * 50 ms timeout
         */
        lws_service(ctx->context, 50);

        /*
         * -----------------------------------------------------
         * Idle / zombie-connection watchdog
         * -----------------------------------------------------
         *
         */
        {
            struct timespec now_ts;
            ts_now(&now_ts);
            if (ctx->connected && ctx->wsi &&
                ts_diff_ms(&now_ts, &ctx->last_rx_ts) > WS_IDLE_TIMEOUT_MS)
            {
                lwsl_user(
                    "No data received in %lld ms, assuming connection "
                    "is dead. Forcing reconnect.\n",
                    ts_diff_ms(&now_ts, &ctx->last_rx_ts));

                /* Ask lws to close the stale wsi as soon as possible */
                lws_set_timeout(ctx->wsi, PENDING_TIMEOUT_CLOSE_ACK, LWS_TO_KILL_ASYNC);

                ctx->connected = false;
                ctx->wsi = NULL;

                if (ctx->is_connected) {
                    *(ctx->is_connected) = 0;
                }

                /* Free any partially assembled message */
                if (ctx->rx_buffer) {
                    free(ctx->rx_buffer);
                    ctx->rx_buffer = NULL;
                    ctx->rx_buffer_len = 0;
                }

                /* Try again immediately; normal backoff still applies
                 * to subsequent failures via ws_client_connect(). */
                ctx->next_reconnect_time = time(NULL);
            }
        }

        /*
         * -----------------------------------------------------
         * Reconnection handling
         * -----------------------------------------------------
         *
         * If there is no active WebSocket connection and
         * the scheduled reconnect time has arrived, try again.
         */
        if (!ctx->wsi && !ctx->connected)
        {

            time_t now = time(NULL);

            if (now >= ctx->next_reconnect_time)
            {
                dbg_log(ctx, "attempting reconnect...\n");

                int rc = ws_client_connect(ctx);

                dbg_log(ctx,
                        "ws_client_connect() returned %d, wsi is now %s\n",
                        rc, ctx->wsi ? "non-NULL" : "NULL");

                if (rc == 0)
                {

                    /*
                     * Connection attempt was initiated.
                     *
                     * LWS_CALLBACK_CLIENT_ESTABLISHED will
                     * set connected=true and reset the
                     * backoff once the server actually
                     * accepts the connection.
                     */
                    lwsl_user(
                        "WebSocket reconnect attempt initiated.\n");
                }
            }
        }
    }
}

/*
 * Destroy client
 */
void ws_client_destroy(ws_client_ctx_t *ctx)
{
    if (!ctx)
    {
        return;
    }

    if (ctx->context)
    {
        lws_context_destroy(ctx->context);
        ctx->context = NULL;
    }

    /*
     * Free our copies of the connection information.
     */
    if (ctx->rx_buffer) free(ctx->rx_buffer);
    free(ctx->address);
    free(ctx->path);

    free(ctx);
}