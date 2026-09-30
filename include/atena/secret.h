#ifndef ATENA_SECRET_H
#define ATENA_SECRET_H

#include "atena/status.h"

#ifdef __cplusplus
extern "C" {
#endif

AtenaStatus atena_secret_store(const char *provider_id, const char *secret);
AtenaStatus atena_secret_lookup(const char *provider_id, char **out_secret);
AtenaStatus atena_secret_delete(const char *provider_id);
void atena_secret_free(char *secret);
int atena_secret_persistence_available(void);

#ifdef __cplusplus
}
#endif

#endif
