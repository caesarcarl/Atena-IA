#include "atena/core.h"
#include "atena/provider.h"
#include "test_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
typedef struct B{char text[8192];size_t n;}B;static int cb(const AtenaStreamEvent*e,void*u){B*b=u;if(e->type==ATENA_EVENT_TEXT_DELTA&&e->text){size_t n=strlen(e->text);ATENA_TEST_ASSERT(b->n+n<sizeof(b->text));memcpy(b->text+b->n,e->text,n+1);b->n+=n;}return 0;}
int main(void){char db[]="/tmp/atena-rag-XXXXXX";int fd=mkstemp(db);ATENA_TEST_ASSERT(fd>=0);close(fd);unlink(db);AtenaCoreConfig cfg={db,"identity",16000,3,1};AtenaCore*c=NULL;ATENA_TEST_ASSERT(atena_core_create(&cfg,&c)==ATENA_OK);ATENA_TEST_ASSERT(atena_core_register_provider(c,atena_mock_provider_create("mock","m"))==ATENA_OK);char did[37];const char*doc="O protocolo Blue Phoenix usa a porta lógica 4242 para o laboratório Atena. Esta evidência é específica e deve ser recuperável.";ATENA_TEST_ASSERT(atena_core_rag_import_text(c,"manual","manual.txt",doc,did)==ATENA_OK);char sid[37];ATENA_TEST_ASSERT(atena_core_session_create(c,"rag",sid)==ATENA_OK);AtenaChatRequest r={sid,"mock",NULL,"Qual porta o protocolo Blue Phoenix usa?","rag-1",1,128,ATENA_REASONING_AUTO};char op[37];B b={0};ATENA_TEST_ASSERT(atena_core_chat_send(c,&r,cb,&b,op)==ATENA_OK);ATENA_TEST_ASSERT(strstr(b.text,"rag=present")!=NULL);atena_core_destroy(c);unlink(db);puts("test_rag: PASS");return 0;}
