#include "json_handler.h"
#include <cjson/cJSON.h>
#include <string.h>

message_kind_t parse_msg_kind(const char *json_string) {
    message_kind_t type = MSG_KIND_UNKNOWN;

    // Parse the raw JSON string
    cJSON *root = cJSON_Parse(json_string);
    if (!root) {
        return MSG_KIND_UNKNOWN;
    }

    // Extract the "kind" field
    cJSON *kind_field = cJSON_GetObjectItemCaseSensitive(root, "kind");
    if (cJSON_IsString(kind_field) && (kind_field->valuestring != NULL)) {
        const char *kind_str = kind_field->valuestring;

        if (strcmp(kind_str, "commit") == 0) {
            type = MSG_KIND_COMMIT;
        } else if (strcmp(kind_str, "identity") == 0) {
            type = MSG_KIND_IDENTITY;
        } else if (strcmp(kind_str, "account") == 0) {
            type = MSG_KIND_ACCOUNT;
        } else if (strcmp(kind_str, "info") == 0) {
            type = MSG_KIND_INFO;
        }
    }

    // Clean up cJSON memory allocation to prevent memory leaks
    cJSON_Delete(root);

    return type;
}