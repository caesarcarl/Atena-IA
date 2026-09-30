#include "protocol.h"
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static AtenaStatus write_all(AtenaNativeHandle native,const void *buf,size_t n){
    HANDLE h=(HANDLE)(intptr_t)native;
    const unsigned char*p=(const unsigned char*)buf;
    while(n){
        DWORD chunk=n>0x7fffffffU?0x7fffffffU:(DWORD)n,w=0;
        if(!WriteFile(h,p,chunk,&w,NULL)||w==0)return ATENA_ERR_NETWORK;
        p+=w;n-=w;
    }
    return ATENA_OK;
}
static AtenaStatus read_all(AtenaNativeHandle native,void *buf,size_t n){
    HANDLE h=(HANDLE)(intptr_t)native;
    unsigned char*p=(unsigned char*)buf;
    while(n){
        DWORD chunk=n>0x7fffffffU?0x7fffffffU:(DWORD)n,r=0;
        if(!ReadFile(h,p,chunk,&r,NULL)||r==0)return ATENA_ERR_NETWORK;
        p+=r;n-=r;
    }
    return ATENA_OK;
}
#else
#include <sys/socket.h>
static AtenaStatus write_all(AtenaNativeHandle native,const void*buf,size_t n){
    int fd=(int)native;const unsigned char*p=(const unsigned char*)buf;
    while(n){
        ssize_t w=send(fd,p,n,MSG_NOSIGNAL);
        if(w<0){if(errno==EINTR)continue;return ATENA_ERR_NETWORK;}
        if(w==0)return ATENA_ERR_NETWORK;
        p+=w;n-=(size_t)w;
    }
    return ATENA_OK;
}
static AtenaStatus read_all(AtenaNativeHandle native,void*buf,size_t n){
    int fd=(int)native;unsigned char*p=(unsigned char*)buf;
    while(n){
        ssize_t r=recv(fd,p,n,0);
        if(r<0){if(errno==EINTR)continue;return ATENA_ERR_NETWORK;}
        if(r==0)return ATENA_ERR_NETWORK;
        p+=r;n-=(size_t)r;
    }
    return ATENA_OK;
}
#endif

static int valid_utf8(const unsigned char *s,size_t n){
    size_t i=0;
    while(i<n){
        unsigned char c=s[i++];
        if(c<0x80)continue;
        size_t need=0;unsigned int cp=0;
        if((c&0xE0)==0xC0){need=1;cp=c&0x1F;if(cp<2)return 0;}
        else if((c&0xF0)==0xE0){need=2;cp=c&0x0F;}
        else if((c&0xF8)==0xF0){need=3;cp=c&0x07;}
        else return 0;
        if(i+need>n)return 0;
        for(size_t j=0;j<need;j++){
            unsigned char d=s[i++];
            if((d&0xC0)!=0x80)return 0;
            cp=(cp<<6)|(d&0x3F);
        }
        if(cp>0x10FFFF||(cp>=0xD800&&cp<=0xDFFF)||(need==2&&cp<0x800)||(need==3&&cp<0x10000))return 0;
    }
    return 1;
}
static void store_be32(unsigned char out[4],uint32_t v){
    out[0]=(unsigned char)(v>>24);out[1]=(unsigned char)(v>>16);out[2]=(unsigned char)(v>>8);out[3]=(unsigned char)v;
}
static uint32_t load_be32(const unsigned char in[4]){
    return ((uint32_t)in[0]<<24)|((uint32_t)in[1]<<16)|((uint32_t)in[2]<<8)|(uint32_t)in[3];
}

AtenaStatus atena_ipc_write_frame(AtenaNativeHandle handle,const char*json){
    if(handle==(AtenaNativeHandle)-1||!json)return ATENA_ERR_INVALID_ARGUMENT;
    size_t len=strlen(json);
    if(len>4U*1024U*1024U)return ATENA_ERR_PROTOCOL;
    unsigned char hdr[8]={'A','T','N','2',0,0,0,0};
    store_be32(hdr+4,(uint32_t)len);
    AtenaStatus st=write_all(handle,hdr,sizeof(hdr));
    if(st!=ATENA_OK)return st;
    return write_all(handle,json,len);
}
AtenaStatus atena_ipc_read_frame(AtenaNativeHandle handle,char**out_json,size_t max_size){
    if(handle==(AtenaNativeHandle)-1||!out_json)return ATENA_ERR_INVALID_ARGUMENT;
    *out_json=NULL;
    unsigned char hdr[8];
    AtenaStatus st=read_all(handle,hdr,sizeof(hdr));
    if(st!=ATENA_OK)return st;
    if(memcmp(hdr,"ATN2",4)!=0)return ATENA_ERR_PROTOCOL;
    size_t len=(size_t)load_be32(hdr+4);
    if(len==0||len>max_size)return ATENA_ERR_PROTOCOL;
    char*buf=(char*)malloc(len+1);
    if(!buf)return ATENA_ERR_NO_MEMORY;
    st=read_all(handle,buf,len);
    if(st!=ATENA_OK){free(buf);return st;}
    if(!valid_utf8((const unsigned char*)buf,len)){free(buf);return ATENA_ERR_PROTOCOL;}
    buf[len]='\0';*out_json=buf;return ATENA_OK;
}
