#include "atena/core.h"
#include "atena/provider.h"
#include "test_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct Buffer { char text[8192]; size_t len; int done; } Buffer;
static int on_event(const AtenaStreamEvent*ev,void*ud){Buffer*b=ud;if(ev->type==ATENA_EVENT_TEXT_DELTA&&ev->text){size_t n=strlen(ev->text);ATENA_TEST_ASSERT(b->len+n<sizeof(b->text));memcpy(b->text+b->len,ev->text,n+1);b->len+=n;}if(ev->type==ATENA_EVENT_DONE)b->done=1;return 0;}
int main(void){char db[]="/tmp/atena-core-test-XXXXXX";int fd=mkstemp(db);ATENA_TEST_ASSERT(fd>=0);close(fd);unlink(db);AtenaCoreConfig cfg={db,"identity",12000,3,1};AtenaCore*c=NULL;ATENA_TEST_ASSERT(atena_core_create(&cfg,&c)==ATENA_OK);ATENA_TEST_ASSERT(atena_core_register_provider(c,atena_mock_provider_create("mock","atena-mock"))==ATENA_OK);char sid[37];ATENA_TEST_ASSERT(atena_core_session_create(c,"test",sid)==ATENA_OK);ATENA_TEST_ASSERT(atena_core_memory_put(c,"explanation","clear examples")==ATENA_OK);Buffer b={0};AtenaChatRequest r={sid,"mock",NULL,"Quem é você?","core-1",0,128,ATENA_REASONING_DISABLED};char op[37];ATENA_TEST_ASSERT(atena_core_chat_send(c,&r,on_event,&b,op)==ATENA_OK);ATENA_TEST_ASSERT(b.done);ATENA_TEST_ASSERT(strstr(b.text,"identity=present")!=NULL);AtenaMessage*m=NULL;size_t n=0;ATENA_TEST_ASSERT(atena_core_session_history(c,sid,&m,&n)==ATENA_OK);ATENA_TEST_ASSERT(n==2);ATENA_TEST_ASSERT(m[0].role==ATENA_ROLE_USER);ATENA_TEST_ASSERT(m[1].role==ATENA_ROLE_ASSISTANT);atena_core_messages_free(m,n);atena_core_destroy(c);unlink(db);puts("test_core: PASS");return 0;}
