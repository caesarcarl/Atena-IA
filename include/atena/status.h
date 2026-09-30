#ifndef ATENA_STATUS_H
#define ATENA_STATUS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum AtenaStatus {
    ATENA_OK = 0,
    ATENA_ERR_INVALID_ARGUMENT = 1,
    ATENA_ERR_NO_MEMORY = 2,
    ATENA_ERR_IO = 3,
    ATENA_ERR_DB = 4,
    ATENA_ERR_JSON = 5,
    ATENA_ERR_NOT_FOUND = 6,
    ATENA_ERR_CONFLICT = 7,
    ATENA_ERR_PROVIDER_UNAVAILABLE = 8,
    ATENA_ERR_PROVIDER_INVALID = 9,
    ATENA_ERR_CANCELLED = 10,
    ATENA_ERR_POLICY_DENIED = 11,
    ATENA_ERR_SCHEMA_TOO_NEW = 12,
    ATENA_ERR_NETWORK = 13,
    ATENA_ERR_TIMEOUT = 14,
    ATENA_ERR_PROTOCOL = 15,
    ATENA_ERR_BUSY = 16,
    ATENA_ERR_UNSUPPORTED = 17,
    ATENA_ERR_INTERNAL = 18,
    ATENA_ERR_PATH = 19,
    ATENA_ERR_PERMISSION = 20
} AtenaStatus;

const char *atena_status_string(AtenaStatus status);

#ifdef __cplusplus
}
#endif
#endif
