#ifndef ATENA_PLATFORM_H
#define ATENA_PLATFORM_H

#include <stdint.h>
#include "atena/path.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef intptr_t AtenaNativeHandle;

typedef struct AtenaTransport {
#ifdef _WIN32
    void *handle;
#else
    int fd;
#endif
} AtenaTransport;

AtenaStatus atena_transport_connect(const char *endpoint, int timeout_ms, AtenaTransport *out);
void atena_transport_close(AtenaTransport *transport);
AtenaNativeHandle atena_transport_native_handle(const AtenaTransport *transport);
AtenaStatus atena_process_start_core(const AtenaPaths *paths);

#ifdef __cplusplus
}
#endif
#endif
