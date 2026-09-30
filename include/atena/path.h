#ifndef ATENA_PATH_H
#define ATENA_PATH_H

#include <stddef.h>
#include "atena/status.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ATENA_PATH_MAX
#define ATENA_PATH_MAX 4096
#endif

typedef struct AtenaPaths {
    char config_dir[ATENA_PATH_MAX];
    char data_dir[ATENA_PATH_MAX];
    char cache_dir[ATENA_PATH_MAX];
    char runtime_dir[ATENA_PATH_MAX];

    char database[ATENA_PATH_MAX];
    char endpoint[ATENA_PATH_MAX];
    char identity_dir[ATENA_PATH_MAX];
    char identity_user_dir[ATENA_PATH_MAX];
    char core_executable[ATENA_PATH_MAX];
    char startup_lock[ATENA_PATH_MAX];
} AtenaPaths;

AtenaStatus atena_paths_resolve(AtenaPaths *paths);
AtenaStatus atena_paths_prepare(const AtenaPaths *paths);

#ifdef __cplusplus
}
#endif
#endif
