#ifndef ATENA_IPC_H
#define ATENA_IPC_H

#include "atena/status.h"
#include "atena/core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AtenaIpcServer AtenaIpcServer;
AtenaStatus atena_ipc_server_start(AtenaCore *core, const char *endpoint, AtenaIpcServer **out_server);
void atena_ipc_server_stop(AtenaIpcServer *server);

#ifdef __cplusplus
}
#endif
#endif
