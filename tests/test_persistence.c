#include "atena/core.h"
#include "atena/provider.h"
#include "test_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static int sink(const AtenaStreamEvent*e,void*u){(void)e;(void)u;return 0;}
int main(void){char db[]="/tmp/atena-persist-XXXXXX";int fd=mkstemp(db);ATENA_TEST_ASSERT(fd>=0);close(fd);unlink(db);AtenaCoreConfig cfg={db,"identity",12000,2,1};AtenaCore*c=NULL;ATENA_TEST_ASSERT(atena_core_create(&cfg,&c)==ATENA_OK);ATENA_TEST_ASSERT(atena_core_register_provider(c,atena_mock_provider_create("mock","m"))==ATENA_OK);char sid[37],op[37];ATENA_TEST_ASSERT(atena_core_session_create(c,"persist",sid)==ATENA_OK);AtenaChatRequest r={sid,"mock",NULL,"turno um","persist-1",0,64,ATENA_REASONING_AUTO};ATENA_TEST_ASSERT(atena_core_chat_send(c,&r,sink,NULL,op)==ATENA_OK);atena_core_destroy(c);c=NULL;ATENA_TEST_ASSERT(atena_core_create(&cfg,&c)==ATENA_OK);ATENA_TEST_ASSERT(atena_core_register_provider(c,atena_mock_provider_create("mock","m"))==ATENA_OK);AtenaMessage*m=NULL;size_t n=0;ATENA_TEST_ASSERT(atena_core_session_history(c,sid,&m,&n)==ATENA_OK);ATENA_TEST_ASSERT(n==2);ATENA_TEST_ASSERT(strcmp(m[0].content,"turno um")==0);atena_core_messages_free(m,n);r.idempotency_key="persist-2";r.user_text="turno dois";ATENA_TEST_ASSERT(atena_core_chat_send(c,&r,sink,NULL,op)==ATENA_OK);ATENA_TEST_ASSERT(atena_core_session_history(c,sid,&m,&n)==ATENA_OK);ATENA_TEST_ASSERT(n==4);atena_core_messages_free(m,n);atena_core_destroy(c);unlink(db);puts("test_persistence: PASS");return 0;}
