#include "atena/sync.h"
#ifdef _WIN32
static BOOL CALLBACK once_bridge(PINIT_ONCE init_once, PVOID parameter, PVOID *context) {
    (void)init_once; (void)context;
    AtenaOnce *once=(AtenaOnce*)parameter;
    if (!once || !once->fn) return FALSE;
    once->fn();
    return TRUE;
}
int atena_mutex_init(AtenaMutex *m){ InitializeCriticalSection(m); return 0; }
void atena_mutex_destroy(AtenaMutex *m){ DeleteCriticalSection(m); }
void atena_mutex_lock(AtenaMutex *m){ EnterCriticalSection(m); }
void atena_mutex_unlock(AtenaMutex *m){ LeaveCriticalSection(m); }
int atena_once(AtenaOnce *once, void (*fn)(void)){ if(!once||!fn)return -1; once->fn=fn; return InitOnceExecuteOnce(&once->once, once_bridge, once, NULL) ? 0 : -1; }
#else
int atena_mutex_init(AtenaMutex *m){ return pthread_mutex_init(m,NULL); }
void atena_mutex_destroy(AtenaMutex *m){ (void)pthread_mutex_destroy(m); }
void atena_mutex_lock(AtenaMutex *m){ (void)pthread_mutex_lock(m); }
void atena_mutex_unlock(AtenaMutex *m){ (void)pthread_mutex_unlock(m); }
int atena_once(AtenaOnce *once, void (*fn)(void)){ return pthread_once(once, fn); }
#endif
