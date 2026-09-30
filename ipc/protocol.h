#ifndef ATENA_IPC_PROTOCOL_H
#define ATENA_IPC_PROTOCOL_H
#include <stddef.h>
#include "atena/status.h"
#include "atena/platform.h"
AtenaStatus atena_ipc_write_frame(AtenaNativeHandle handle,const char *json);
AtenaStatus atena_ipc_read_frame(AtenaNativeHandle handle,char **out_json,size_t max_size);
#endif
