#ifndef ATENA_TYPES_H
#define ATENA_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum AtenaRole {
    ATENA_ROLE_SYSTEM = 0,
    ATENA_ROLE_USER = 1,
    ATENA_ROLE_ASSISTANT = 2,
    ATENA_ROLE_TOOL = 3
} AtenaRole;

typedef enum AtenaMessageState {
    ATENA_MSG_COMPLETE = 0,
    ATENA_MSG_PARTIAL = 1,
    ATENA_MSG_CANCELLED = 2,
    ATENA_MSG_INTERRUPTED = 3,
    ATENA_MSG_FAILED = 4
} AtenaMessageState;

typedef enum AtenaReasoningLevel {
    ATENA_REASONING_AUTO = 0,
    ATENA_REASONING_DISABLED = 1,
    ATENA_REASONING_LOW = 2,
    ATENA_REASONING_MEDIUM = 3,
    ATENA_REASONING_HIGH = 4
} AtenaReasoningLevel;

typedef struct AtenaMessage {
    char id[37];
    char session_id[37];
    AtenaRole role;
    AtenaMessageState state;
    char *content;
    char created_at[32];
} AtenaMessage;

typedef enum AtenaEventType {
    ATENA_EVENT_START = 0,
    ATENA_EVENT_TEXT_DELTA = 1,
    ATENA_EVENT_REASONING_DELTA = 2,
    ATENA_EVENT_TOOL_CALL = 3,
    ATENA_EVENT_TOOL_RESULT = 4,
    ATENA_EVENT_CITATION = 5,
    ATENA_EVENT_USAGE = 6,
    ATENA_EVENT_ERROR = 7,
    ATENA_EVENT_DONE = 8
} AtenaEventType;

typedef struct AtenaMetrics {
    double time_to_first_token_ms;
    double total_generation_ms;
    double retrieval_ms;
    double tool_ms;
    uint64_t prompt_tokens;
    uint64_t completion_tokens;
} AtenaMetrics;

typedef struct AtenaStreamEvent {
    AtenaEventType type;
    const char *operation_id;
    uint64_t seq;
    const char *text;
    const char *tool_name;
    const char *tool_json;
    const char *citation_id;
    const char *citation_document_id;
    const char *citation_title;
    const char *citation_locator;
    const char *citation_content_sha256;
    AtenaMetrics metrics;
    int error_code;
} AtenaStreamEvent;

typedef int (*AtenaEventCallback)(const AtenaStreamEvent *event, void *userdata);

#ifdef __cplusplus
}
#endif
#endif
