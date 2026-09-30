#ifndef ATENA_SYNC_H
#define ATENA_SYNC_H
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef CRITICAL_SECTION AtenaMutex;
typedef struct AtenaOnce { INIT_ONCE once; void (*fn)(void); } AtenaOnce;
#define ATENA_ONCE_INIT { INIT_ONCE_STATIC_INIT, NULL }
#else
#include <pthread.h>
typedef pthread_mutex_t AtenaMutex;
typedef pthread_once_t AtenaOnce;
#define ATENA_ONCE_INIT PTHREAD_ONCE_INIT
#endif

int atena_mutex_init(AtenaMutex *m);
void atena_mutex_destroy(AtenaMutex *m);
void atena_mutex_lock(AtenaMutex *m);
void atena_mutex_unlock(AtenaMutex *m);
int atena_once(AtenaOnce *once, void (*fn)(void));
#endif
