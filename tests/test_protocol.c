#include "atena/status.h"
#include "../ipc/protocol.h"
#include <arpa/inet.h>
#include "test_common.h"
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct Writer{int fd;const unsigned char*data;size_t size;}Writer;
static void *fragmented(void*arg){Writer*w=arg;for(size_t i=0;i<w->size;i++){ATENA_TEST_ASSERT(write(w->fd,w->data+i,1)==1);}close(w->fd);return NULL;}
int main(void){int pair[2];ATENA_TEST_ASSERT(socketpair(AF_UNIX,SOCK_STREAM,0,pair)==0);const char*json="{\"texto\":\"Olá\"}";uint32_t length=htonl((uint32_t)strlen(json));unsigned char frame[128]={0};memcpy(frame,"ATN2",4);memcpy(frame+4,&length,4);memcpy(frame+8,json,strlen(json));Writer writer={pair[1],frame,8+strlen(json)};pthread_t thread;ATENA_TEST_ASSERT(pthread_create(&thread,NULL,fragmented,&writer)==0);char*out=NULL;ATENA_TEST_ASSERT(atena_ipc_read_frame(pair[0],&out,1024)==ATENA_OK);ATENA_TEST_ASSERT(strcmp(out,json)==0);free(out);close(pair[0]);pthread_join(thread,NULL);
ATENA_TEST_ASSERT(socketpair(AF_UNIX,SOCK_STREAM,0,pair)==0);unsigned char invalid[]={ 'A','T','N','2',0,0,0,2,0xC0,0xAF };ATENA_TEST_ASSERT(write(pair[1],invalid,sizeof(invalid))==(ssize_t)sizeof(invalid));ATENA_TEST_ASSERT(atena_ipc_read_frame(pair[0],&out,1024)==ATENA_ERR_PROTOCOL);close(pair[0]);close(pair[1]);puts("test_protocol: PASS");return 0;}
