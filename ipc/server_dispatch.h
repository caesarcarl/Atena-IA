#ifndef ATENA_IPC_SERVER_DISPATCH_H
#define ATENA_IPC_SERVER_DISPATCH_H

#include "atena/core.h"
#include "atena/platform.h"
#include <json-c/json.h>

AtenaStatus atena_ipc_dispatch_request(AtenaCore *core, AtenaNativeHandle fd, json_object *request);

#endif
