#ifndef ATENA_CORE_UTIL_H
#define ATENA_CORE_UTIL_H
#include <stddef.h>
#include <stdint.h>
int atena_uuid4(char out[37]);
void atena_now_iso8601(char out[32]);
char *atena_strdup(const char *s);
char *atena_strndup(const char *s, size_t n);
uint64_t atena_now_monotonic_ms(void);
#endif
