#include "atena/platform.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>

AtenaStatus atena_transport_connect(const char *endpoint, int timeout_ms, AtenaTransport *out) {
    if (!endpoint || !*endpoint || !out) return ATENA_ERR_INVALID_ARGUMENT;
    out->handle = NULL;
    DWORD wait_ms = timeout_ms > 0 ? (DWORD)timeout_ms : 2000U;
    if (!WaitNamedPipeA(endpoint, wait_ms)) {
        DWORD e = GetLastError();
        return e == ERROR_SEM_TIMEOUT ? ATENA_ERR_TIMEOUT : ATENA_ERR_NETWORK;
    }
    HANDLE h = CreateFileA(endpoint, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return ATENA_ERR_NETWORK;
    DWORD mode = PIPE_READMODE_BYTE;
    (void)SetNamedPipeHandleState(h, &mode, NULL, NULL);
    out->handle = h;
    return ATENA_OK;
}

void atena_transport_close(AtenaTransport *transport) {
    if (!transport || !transport->handle) return;
    CloseHandle((HANDLE)transport->handle);
    transport->handle = NULL;
}

AtenaNativeHandle atena_transport_native_handle(const AtenaTransport *transport) {
    return transport && transport->handle ? (AtenaNativeHandle)(intptr_t)transport->handle : (AtenaNativeHandle)-1;
}
#endif
