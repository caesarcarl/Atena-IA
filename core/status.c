#include "atena/status.h"

const char *atena_status_string(AtenaStatus status) {
    switch (status) {
        case ATENA_OK: return "ok";
        case ATENA_ERR_INVALID_ARGUMENT: return "invalid_argument";
        case ATENA_ERR_NO_MEMORY: return "no_memory";
        case ATENA_ERR_IO: return "io_error";
        case ATENA_ERR_DB: return "database_error";
        case ATENA_ERR_JSON: return "json_error";
        case ATENA_ERR_NOT_FOUND: return "not_found";
        case ATENA_ERR_CONFLICT: return "conflict";
        case ATENA_ERR_PROVIDER_UNAVAILABLE: return "provider_unavailable";
        case ATENA_ERR_PROVIDER_INVALID: return "provider_invalid";
        case ATENA_ERR_CANCELLED: return "cancelled";
        case ATENA_ERR_POLICY_DENIED: return "policy_denied";
        case ATENA_ERR_SCHEMA_TOO_NEW: return "schema_too_new";
        case ATENA_ERR_NETWORK: return "network_error";
        case ATENA_ERR_TIMEOUT: return "timeout";
        case ATENA_ERR_PROTOCOL: return "protocol_error";
        case ATENA_ERR_BUSY: return "busy";
        case ATENA_ERR_UNSUPPORTED: return "unsupported";
        case ATENA_ERR_INTERNAL: return "internal_error";
        case ATENA_ERR_PATH: return "path_error";
        case ATENA_ERR_PERMISSION: return "permission_error";
    }
    return "unknown";
}
