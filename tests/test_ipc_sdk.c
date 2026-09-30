#include "atena/client.h"
#include "atena/core.h"
#include "atena/ipc.h"
#include "atena/provider.h"
#include "test_common.h"
#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct Buffer { char text[8192]; size_t size; int done; } Buffer;
static int on_event(const AtenaStreamEvent *event,void *userdata){Buffer*b=userdata;if(event->type==ATENA_EVENT_TEXT_DELTA&&event->text){size_t n=strlen(event->text);ATENA_TEST_ASSERT(b->size+n<sizeof(b->text));memcpy(b->text+b->size,event->text,n+1);b->size+=n;}if(event->type==ATENA_EVENT_DONE)b->done=1;return 0;}

int main(void){
    char database[]="/tmp/atena-ipc-db-XXXXXX";int file=mkstemp(database);ATENA_TEST_ASSERT(file>=0);close(file);unlink(database);
    char endpoint[128];snprintf(endpoint,sizeof(endpoint),"/tmp/atena-ipc-%ld.sock",(long)getpid());
    AtenaCoreConfig core_config={database,"identity",12000,2,1};AtenaCore*core=NULL;ATENA_TEST_ASSERT(atena_core_create(&core_config,&core)==ATENA_OK);ATENA_TEST_ASSERT(atena_core_register_provider(core,atena_mock_provider_create("mock","m"))==ATENA_OK);
    AtenaIpcServer*server=NULL;ATENA_TEST_ASSERT(atena_ipc_server_start(core,endpoint,&server)==ATENA_OK);
    AtenaClientConfig config={endpoint,2000,600000};AtenaClient*client=NULL;ATENA_TEST_ASSERT(atena_client_connect(&config,&client)==ATENA_OK);
    char*hello=NULL;ATENA_TEST_ASSERT(atena_client_hello(client,&hello)==ATENA_OK);json_object *hello_json=json_tokener_parse(hello);json_object *hello_protocol=NULL;
    ATENA_TEST_ASSERT(hello_json&&json_object_object_get_ex(hello_json,"protocol",&hello_protocol));
    ATENA_TEST_ASSERT(strcmp(json_object_get_string(hello_protocol),"atena.ipc/2")==0);
    json_object_put(hello_json);ATENA_TEST_ASSERT(strstr(hello,"chat.start"));atena_client_free_string(hello);
    char session[37];ATENA_TEST_ASSERT(atena_client_session_create(client,"sdk",session)==ATENA_OK);char operation[37];Buffer buffer={0};AtenaStatus chat_status=atena_client_chat_send(client,session,"mock","Olá pelo SDK",on_event,&buffer,operation);if(chat_status!=ATENA_OK)fprintf(stderr,"chat_status=%s (%d)\n",atena_status_string(chat_status),(int)chat_status);ATENA_TEST_ASSERT(chat_status==ATENA_OK);ATENA_TEST_ASSERT(buffer.done);ATENA_TEST_ASSERT(strstr(buffer.text,"identity=present"));ATENA_TEST_ASSERT(operation[0]);
    atena_client_close(client);atena_ipc_server_stop(server);atena_core_destroy(core);unlink(database);unlink(endpoint);puts("test_ipc_sdk: PASS");return 0;
}
