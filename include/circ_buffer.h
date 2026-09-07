#ifndef CIRC_BUFF_H
#define CIRC_BUFF_H

#include <stdbool.h>
#include <stdlib.h>

#define CIRC_BUFF_CAPACITY 1024  /* Fixed size for the bounded circular buffer */
#define MAX_JSON_LEN 2048        /* Maximum buffer size to hold an individual raw JSON frame */

typedef struct {
    char data[CIRC_BUFF_CAPACITY][MAX_JSON_LEN];
    int head;
    int tail;
    int size;
} circ_buff_t;

void circ_buff_init(circ_buff_t *cb);
bool circ_buff_is_full(const circ_buff_t *cb);
bool circ_buff_is_empty(const circ_buff_t *cb);
bool circ_buff_push(circ_buff_t *cb, const char *item);
bool circ_buff_pop(circ_buff_t *cb, char *item_dest);
double circ_buff_get_occupancy_pct(const circ_buff_t *cb);

#endif