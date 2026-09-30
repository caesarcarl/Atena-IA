#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

int atena_uuid4(char out[37]) {
    unsigned char b[16];
#ifdef _WIN32
    if (BCryptGenRandom(NULL, b, (ULONG)sizeof(b), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) return 0;
#else
    int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (fd < 0) return 0;
    ssize_t got = read(fd, b, sizeof(b));
    close(fd);
    if (got != (ssize_t)sizeof(b)) return 0;
#endif
    b[6] = (unsigned char)((b[6] & 0x0fU) | 0x40U);
    b[8] = (unsigned char)((b[8] & 0x3fU) | 0x80U);
    snprintf(out, 37,
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7],
             b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
    return 1;
}

void atena_now_iso8601(char out[32]) {
    time_t t = time(NULL);
    struct tm tmv;
#ifdef _WIN32
    gmtime_s(&tmv, &t);
#else
    gmtime_r(&t, &tmv);
#endif
    strftime(out, 32, "%Y-%m-%dT%H:%M:%SZ", &tmv);
}

char *atena_strdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char*)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

char *atena_strndup(const char *s, size_t n) {
    if (!s) return NULL;
    size_t len = 0;
    while (len < n && s[len]) ++len;
    char *p = (char*)malloc(len + 1);
    if (!p) return NULL;
    memcpy(p, s, len);
    p[len] = '\0';
    return p;
}

uint64_t atena_now_monotonic_ms(void) {
#ifdef _WIN32
    return (uint64_t)GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
#endif
}
