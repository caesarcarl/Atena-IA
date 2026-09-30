#include "atena/provider.h"
#include "../core/util.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct MockImpl {
    volatile int cancelled;
    char active_operation[37];
} MockImpl;

static AtenaStatus mock_generate(AtenaProvider *provider,
                                 const AtenaProviderRequest *request,
                                 AtenaEventCallback callback,
                                 void *userdata) {
    if (!provider || !request || !callback || !request->messages || request->message_count == 0) return ATENA_ERR_INVALID_ARGUMENT;
    MockImpl *impl = provider->impl;
    impl->cancelled = 0;
    snprintf(impl->active_operation,sizeof(impl->active_operation),"%s",request->operation_id?request->operation_id:"");
    const AtenaMessage *last = &request->messages[request->message_count - 1];
    if (last->role != ATENA_ROLE_USER && last->role != ATENA_ROLE_TOOL) return ATENA_ERR_PROVIDER_INVALID;

    uint64_t seq=1;
    if (last->role == ATENA_ROLE_USER && strncmp(last->content,"tool:",5)==0) {
        const char *tool = last->content + 5;
        AtenaStreamEvent tev={0};tev.type=ATENA_EVENT_TOOL_CALL;tev.operation_id=request->operation_id;tev.seq=seq++;tev.tool_name=tool;tev.tool_json="{}";
        if(callback(&tev,userdata)!=0)return ATENA_ERR_CANCELLED;
        return ATENA_OK;
    }

    char reply[4096];
    if (last->role == ATENA_ROLE_TOOL) {
        snprintf(reply,sizeof(reply),"Tool observation received: %s", last->content);
    } else {
        int identity_seen = 0, rag_seen = 0;
        for(size_t i=0;i<request->message_count;i++) {
            if(strstr(request->messages[i].content,"[ATENA IDENTITY]")) identity_seen=1;
            if(strstr(request->messages[i].content,"[UNTRUSTED RAG DATA")) rag_seen=1;
        }
        snprintf(reply,sizeof(reply),"Atena mock response: %s%s%s",
                 last->content,
                 identity_seen ? " | identity=present" : " | identity=missing",
                 rag_seen ? " | rag=present" : "");
    }

    size_t len=strlen(reply), pos=0;
    while(pos<len) {
        if(impl->cancelled) return ATENA_ERR_CANCELLED;
        size_t end=pos+24; if(end>len)end=len;
        /* Never split an UTF-8 code point between chat.delta frames. */
        while(end<len && end>pos && (((unsigned char)reply[end] & 0xC0U)==0x80U)) --end;
        if(end==pos) end=pos+1;
        size_t n=end-pos;
        char chunk[32]; memcpy(chunk,reply+pos,n);chunk[n]='\0';
        AtenaStreamEvent ev={0}; ev.type=ATENA_EVENT_TEXT_DELTA; ev.operation_id=request->operation_id;ev.seq=seq++;ev.text=chunk;
        if(callback(&ev,userdata)!=0)return ATENA_ERR_CANCELLED;
        pos+=n;
    }
    AtenaStreamEvent usage={0};usage.type=ATENA_EVENT_USAGE;usage.operation_id=request->operation_id;usage.seq=seq++;usage.metrics.prompt_tokens=request->message_count*16;usage.metrics.completion_tokens=(len+3)/4;
    callback(&usage,userdata);
    return ATENA_OK;
}

static AtenaStatus mock_cancel(AtenaProvider *provider,const char *operation_id){if(!provider||!operation_id)return ATENA_ERR_INVALID_ARGUMENT;MockImpl*impl=provider->impl;if(strcmp(impl->active_operation,operation_id)==0)impl->cancelled=1;return ATENA_OK;}
static void mock_destroy(AtenaProvider *provider){if(!provider)return;free(provider->impl);free(provider);}
static const AtenaProviderVTable MOCK_VTABLE={mock_generate,mock_cancel,mock_destroy};

AtenaProvider *atena_mock_provider_create(const char *id,const char *model){if(!id||!*id)return NULL;AtenaProvider*p=calloc(1,sizeof(*p));MockImpl*impl=calloc(1,sizeof(*impl));if(!p||!impl){free(p);free(impl);return NULL;}snprintf(p->id,sizeof(p->id),"%s",id);snprintf(p->type,sizeof(p->type),"mock");snprintf(p->model,sizeof(p->model),"%s",model?model:"atena-mock");p->capabilities=ATENA_CAP_TEXT|ATENA_CAP_STREAMING|ATENA_CAP_TOOLS|ATENA_CAP_USAGE|ATENA_CAP_CANCELLATION;p->capabilities_known=~0ULL;p->vtable=&MOCK_VTABLE;p->impl=impl;return p;}

AtenaCapabilityState atena_provider_capability(const AtenaProvider *provider,uint64_t capability){if(!provider||!capability||!(provider->capabilities_known&capability))return ATENA_CAPABILITY_UNKNOWN;return (provider->capabilities&capability)?ATENA_CAPABILITY_SUPPORTED:ATENA_CAPABILITY_UNSUPPORTED;}
