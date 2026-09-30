#ifdef _WIN32
#include "atena/ipc.h"
#include "protocol.h"
#include "server_dispatch.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sddl.h>
#include <json-c/json.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct AtenaIpcServer {
    AtenaCore *core;
    char endpoint[ATENA_PATH_MAX];
    HANDLE accept_thread;
    CRITICAL_SECTION lock;
    CONDITION_VARIABLE cond;
    size_t active_clients;
    int stopping;
};

typedef struct ClientCtx { AtenaIpcServer *server; HANDLE pipe; } ClientCtx;

static DWORD WINAPI client_thread(LPVOID arg) {
    ClientCtx *c = (ClientCtx *)arg;
    char *raw = NULL;
    AtenaNativeHandle fd = (AtenaNativeHandle)(intptr_t)c->pipe;
    AtenaStatus s = atena_ipc_read_frame(fd, &raw, 4U * 1024U * 1024U);
    if (s == ATENA_OK) {
        json_object *r = json_tokener_parse(raw);
        free(raw);
        if (r && json_object_is_type(r, json_type_object))
            (void)atena_ipc_dispatch_request(c->server->core, fd, r);
        if (r) json_object_put(r);
    }
    FlushFileBuffers(c->pipe);
    DisconnectNamedPipe(c->pipe);
    CloseHandle(c->pipe);
    AtenaIpcServer *server = c->server;
    free(c);
    EnterCriticalSection(&server->lock);
    if (server->active_clients) server->active_clients--;
    WakeAllConditionVariable(&server->cond);
    LeaveCriticalSection(&server->lock);
    return 0;
}

static SECURITY_ATTRIBUTES pipe_security(PSECURITY_DESCRIPTOR *out_sd) {
    SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), NULL, FALSE};
    *out_sd = NULL;
    HANDLE token = NULL; DWORD bytes = 0;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return sa;
    (void)GetTokenInformation(token, TokenUser, NULL, 0, &bytes);
    TOKEN_USER *u = bytes ? (TOKEN_USER *)malloc(bytes) : NULL;
    if (!u || !GetTokenInformation(token, TokenUser, u, bytes, &bytes)) { free(u); CloseHandle(token); return sa; }
    LPSTR sid = NULL;
    if (ConvertSidToStringSidA(u->User.Sid, &sid) && sid) {
        char sddl[512];
        if (snprintf(sddl, sizeof(sddl), "D:P(A;;GA;;;%s)", sid) > 0)
            (void)ConvertStringSecurityDescriptorToSecurityDescriptorA(sddl, SDDL_REVISION_1, out_sd, NULL);
        LocalFree(sid);
    }
    free(u); CloseHandle(token); sa.lpSecurityDescriptor = *out_sd; return sa;
}

static HANDLE create_pipe(AtenaIpcServer *s) {
    PSECURITY_DESCRIPTOR sd = NULL; SECURITY_ATTRIBUTES sa = pipe_security(&sd);
    HANDLE p = CreateNamedPipeA(s->endpoint,
                                PIPE_ACCESS_DUPLEX,
                                PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                                PIPE_UNLIMITED_INSTANCES,
                                65536, 65536, 0,
                                sa.lpSecurityDescriptor ? &sa : NULL);
    if (sd) LocalFree(sd);
    return p;
}

static DWORD WINAPI accept_thread(LPVOID arg) {
    AtenaIpcServer *s = (AtenaIpcServer *)arg;
    for (;;) {
        EnterCriticalSection(&s->lock); int stop = s->stopping; LeaveCriticalSection(&s->lock);
        if (stop) break;
        HANDLE pipe = create_pipe(s);
        if (pipe == INVALID_HANDLE_VALUE) break;
        BOOL connected = ConnectNamedPipe(pipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        EnterCriticalSection(&s->lock); stop = s->stopping; LeaveCriticalSection(&s->lock);
        if (!connected || stop) { CloseHandle(pipe); if (stop) break; continue; }
        ClientCtx *c = (ClientCtx *)calloc(1, sizeof(*c));
        if (!c) { CloseHandle(pipe); continue; }
        c->server = s; c->pipe = pipe;
        EnterCriticalSection(&s->lock); s->active_clients++; LeaveCriticalSection(&s->lock);
        HANDLE t = CreateThread(NULL, 0, client_thread, c, 0, NULL);
        if (t) CloseHandle(t);
        else {
            CloseHandle(pipe); free(c);
            EnterCriticalSection(&s->lock); s->active_clients--; LeaveCriticalSection(&s->lock);
        }
    }
    return 0;
}

AtenaStatus atena_ipc_server_start(AtenaCore *core, const char *endpoint, AtenaIpcServer **out) {
    if (!core || !endpoint || !out) return ATENA_ERR_INVALID_ARGUMENT;
    *out = NULL;
    AtenaIpcServer *s = (AtenaIpcServer *)calloc(1, sizeof(*s));
    if (!s) return ATENA_ERR_NO_MEMORY;
    if (strlen(endpoint) >= sizeof(s->endpoint)) { free(s); return ATENA_ERR_INVALID_ARGUMENT; }
    s->core = core; snprintf(s->endpoint, sizeof(s->endpoint), "%s", endpoint);
    InitializeCriticalSection(&s->lock); InitializeConditionVariable(&s->cond);
    s->accept_thread = CreateThread(NULL, 0, accept_thread, s, 0, NULL);
    if (!s->accept_thread) { DeleteCriticalSection(&s->lock); free(s); return ATENA_ERR_IO; }
    *out = s; return ATENA_OK;
}

void atena_ipc_server_stop(AtenaIpcServer *s) {
    if (!s) return;
    EnterCriticalSection(&s->lock); s->stopping = 1; LeaveCriticalSection(&s->lock);
    HANDLE wake = CreateFileA(s->endpoint, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (wake != INVALID_HANDLE_VALUE) CloseHandle(wake);
    WaitForSingleObject(s->accept_thread, INFINITE); CloseHandle(s->accept_thread);
    EnterCriticalSection(&s->lock);
    while (s->active_clients) SleepConditionVariableCS(&s->cond, &s->lock, INFINITE);
    LeaveCriticalSection(&s->lock);
    DeleteCriticalSection(&s->lock); free(s);
}
#endif
