#ifndef JSON_HANDLER_H
#define JSON_HANDLER_H

#include <stdbool.h>

typedef enum {
    MSG_KIND_UNKNOWN = 0,
    MSG_KIND_COMMIT,
    MSG_KIND_IDENTITY,
    MSG_KIND_ACCOUNT,
    MSG_KIND_INFO
} message_kind_t;

message_kind_t parse_msg_kind(const char *json_string);

#endif