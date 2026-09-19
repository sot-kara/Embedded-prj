#include "circ_buffer.h"
#include <string.h>
#define MIN(a,b) ((a) < (b) ? (a) : (b))

void circ_buff_init(circ_buff_t *cb) {
    cb->head = 0;
    cb->tail = 0;
    cb->size = 0;
}

bool circ_buff_is_full(const circ_buff_t *cb) {
    return cb->size >= CIRC_BUFF_CAPACITY;
}

bool circ_buff_is_empty(const circ_buff_t *cb) {
    return cb->size <= 0;
}

bool circ_buff_push(circ_buff_t *cb, const char *item, size_t len) {
    if (circ_buff_is_full(cb)) {
        return false;
    }

    // Copy string into the tail position
    strncpy(cb->data[cb->tail], item, (size_t)MIN(len, MAX_JSON_LEN - 1));
    cb->data[cb->tail][(int)MIN(len, MAX_JSON_LEN - 1)] = '\0';

    cb->tail = (cb->tail + 1) % CIRC_BUFF_CAPACITY;
    cb->size++;

    return true;
}

bool circ_buff_pop(circ_buff_t *cb, char *item_dest) {
    if (circ_buff_is_empty(cb)) {
        return false;
    }

    // Copy string out from the head position
    strncpy(item_dest, cb->data[cb->head], MAX_JSON_LEN - 1);
    item_dest[MAX_JSON_LEN - 1] = '\0';

    cb->head = (cb->head + 1) % CIRC_BUFF_CAPACITY;
    cb->size--;

    return true;
}

double circ_buff_get_occupancy_pct(const circ_buff_t *cb) {
    return ((double)cb->size / CIRC_BUFF_CAPACITY) * 100.0;
}