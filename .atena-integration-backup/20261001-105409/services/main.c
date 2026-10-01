#include "atena/core.h"
#include "atena/ipc.h"
#include "atena/path.h"
#include "atena/provider.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static volatile LONG stop_requested=0;
static BOOL WINAPI on_console(DWORD type){(void)type;InterlockedExchange(&stop_requested,1);return TRUE;}
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/file.h>
#include <unistd.h>
static volatile sig_atomic_t stop_requested=0;
static void on_signal(int signal_number){(void)signal_number;stop_requested=1;}
#endif

static const char *option_value(int argc,char **argv,const char *name){for(int i=1;i+1<argc;i++)if(!strcmp(argv[i],name))return argv[i+1];return NULL;}
static int has_option(int argc,char **argv,const char *name){for(int i=1;i<argc;i++)if(!strcmp(argv[i],name))return 1;return 0;}

int main(int argc,char **argv){
    AtenaPaths paths;
    AtenaStatus status=atena_paths_resolve(&paths);
    if(status!=ATENA_OK){fprintf(stderr,"Atena: falha ao resolver paths: %s\n",atena_status_string(status));return 1;}
    status=atena_paths_prepare(&paths);
    if(status!=ATENA_OK){fprintf(stderr,"Atena: falha ao preparar paths: %s\n",atena_status_string(status));return 1;}

    const char *value;
    const char *db_option = option_value(argc,argv,"--db");
    const char *storage_option = option_value(argc,argv,"--storage");
    const char *storage_env = getenv("ATENA_STORAGE_MODE");
    const char *storage_mode = storage_option && *storage_option ? storage_option :
                               (storage_env && *storage_env ? storage_env :
                               (db_option && *db_option ? "persistent" : "memory"));
    if(strcmp(storage_mode,"memory")!=0 && strcmp(storage_mode,"persistent")!=0){
        fprintf(stderr,"Atena: modo de armazenamento inválido '%s' (use memory ou persistent).\n",storage_mode);
        return 1;
    }
    if(!strcmp(storage_mode,"memory"))
        snprintf(paths.database,sizeof(paths.database),":memory:");
    else if(db_option && *db_option)
        snprintf(paths.database,sizeof(paths.database),"%s",db_option);
    if((value=option_value(argc,argv,"--identity")))snprintf(paths.identity_dir,sizeof(paths.identity_dir),"%s",value);
    if((value=option_value(argc,argv,"--socket")))snprintf(paths.endpoint,sizeof(paths.endpoint),"%s",value);

#ifdef _WIN32
    HANDLE instance=CreateMutexA(NULL,TRUE,"Local\\Atena-Core-v2");
    if(!instance){fprintf(stderr,"Atena: falha ao criar mutex do Core.\n");return 1;}
    if(GetLastError()==ERROR_ALREADY_EXISTS){fprintf(stderr,"Atena Core já está em execução para este usuário.\n");CloseHandle(instance);return 2;}
#else
    char lock_path[ATENA_PATH_MAX];
    int length=snprintf(lock_path,sizeof(lock_path),"%s/core.lock",paths.runtime_dir);
    if(length<0||(size_t)length>=sizeof(lock_path)){fprintf(stderr,"Atena: caminho de runtime inválido.\n");return 1;}
    int lock_fd=open(lock_path,O_CREAT|O_RDWR,0600);
    if(lock_fd<0||flock(lock_fd,LOCK_EX|LOCK_NB)!=0){fprintf(stderr,"Atena Core já está em execução para este usuário.\n");if(lock_fd>=0)close(lock_fd);return 2;}
#endif

    AtenaCoreConfig config={paths.database,paths.identity_dir,16000,4,0};
    AtenaCore *core=NULL;
    status=atena_core_create(&config,&core);
    if(status!=ATENA_OK){
        fprintf(stderr,"Atena: falha ao iniciar armazenamento %s (%s)\npath: %s\n",
                storage_mode,atena_status_string(status),paths.database);
#ifdef _WIN32
        ReleaseMutex(instance);CloseHandle(instance);
#else
        close(lock_fd);
#endif
        return 1;
    }

    const char *model=getenv("ATENA_OLLAMA_MODEL"),*url=getenv("ATENA_OLLAMA_URL"),*token=getenv("ATENA_OLLAMA_TOKEN");
    status=atena_core_provider_configure(core,"ollama","ollama",url,model,token);
    if(status!=ATENA_OK){
        fprintf(stderr,"Atena: falha ao configurar Ollama: %s\n",atena_status_string(status));
        atena_core_destroy(core);
#ifdef _WIN32
        ReleaseMutex(instance);CloseHandle(instance);
#else
        close(lock_fd);
#endif
        return 1;
    }
    /* Restore only the provider selected by the user. This avoids probing every
     * system-keyring entry at startup and keeps Ollama as the safe fallback. */
    status=atena_core_restore_default_provider(core);
    if(status!=ATENA_OK&&status!=ATENA_ERR_PROVIDER_UNAVAILABLE&&status!=ATENA_ERR_INVALID_ARGUMENT&&status!=ATENA_ERR_UNSUPPORTED){
        fprintf(stderr,"Atena: aviso ao restaurar provider padrão: %s\n",atena_status_string(status));
    }

    if(has_option(argc,argv,"--demo")||getenv("ATENA_ENABLE_MOCK")){
        AtenaProvider *mock=atena_mock_provider_create("mock","atena-mock");
        if(mock&&atena_core_register_provider(core,mock)!=ATENA_OK)mock->vtable->destroy(mock);
    }

    AtenaIpcServer *server=NULL;
    status=atena_ipc_server_start(core,paths.endpoint,&server);
    if(status!=ATENA_OK){
        fprintf(stderr,"Atena: falha ao iniciar IPC: %s\n",atena_status_string(status));
        atena_core_destroy(core);
#ifdef _WIN32
        ReleaseMutex(instance);CloseHandle(instance);
#else
        close(lock_fd);
#endif
        return 1;
    }

#ifdef _WIN32
    SetConsoleCtrlHandler(on_console,TRUE);
#else
    signal(SIGINT,on_signal);signal(SIGTERM,on_signal);
#endif
    fprintf(stdout,"Atena Core 0.5.0-base pronto em %s | storage=%s\n",paths.endpoint,storage_mode);fflush(stdout);
    while(!stop_requested){
#ifdef _WIN32
        Sleep(250);
#else
        sleep(1);
#endif
    }

    atena_ipc_server_stop(server);
    atena_core_destroy(core);
#ifdef _WIN32
    ReleaseMutex(instance);CloseHandle(instance);
#else
    flock(lock_fd,LOCK_UN);close(lock_fd);
#endif
    return 0;
}
