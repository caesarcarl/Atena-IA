#include "atena/provider.h"

#include <curl/curl.h>
#include <json-c/json.h>
#include "atena/sync.h"
#include "atena/runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OLLAMA_DEFAULT_URL "http://127.0.0.1:11434"
#define OLLAMA_DEFAULT_MODEL "qwen3:0.6b"

typedef struct OllamaImpl {
    char base_url[512];
    char bearer_token[1024];
    AtenaMutex lock;
    char active_operation[37];
    int cancelled;
} OllamaImpl;

typedef struct StreamContext {
    OllamaImpl *impl;
    const char *operation_id;
    AtenaEventCallback callback;
    void *userdata;
    char *pending;
    size_t pending_size;
    uint64_t sequence;
    size_t text_bytes;
    size_t reasoning_bytes;
    int done;
    AtenaStatus status;
} StreamContext;

typedef struct HttpBuffer {
    char *data;
    size_t size;
} HttpBuffer;

static AtenaOnce curl_once = ATENA_ONCE_INIT;
static void initialize_curl(void) { (void)curl_global_init(CURL_GLOBAL_DEFAULT); }

static const char *role_name(AtenaRole role) {
    switch(role){case ATENA_ROLE_SYSTEM:return "system";case ATENA_ROLE_USER:return "user";case ATENA_ROLE_ASSISTANT:return "assistant";case ATENA_ROLE_TOOL:return "tool";}return "user";
}

static int is_cancelled(OllamaImpl *impl) {
    int value; atena_mutex_lock(&impl->lock); value=impl->cancelled; atena_mutex_unlock(&impl->lock); return value;
}

static struct curl_slist *make_headers(const OllamaImpl *impl, int json_body) {
    struct curl_slist *headers = NULL;
    if (json_body) headers = curl_slist_append(headers, "Content-Type: application/json");
    if (impl && impl->bearer_token[0]) {
        char authorization[1200];
        int n = snprintf(authorization, sizeof(authorization), "Authorization: Bearer %s", impl->bearer_token);
        if (n > 0 && (size_t)n < sizeof(authorization)) headers = curl_slist_append(headers, authorization);
    }
    return headers;
}

static size_t collect_http(char *data, size_t size, size_t count, void *userdata) {
    HttpBuffer *buffer = userdata;
    size_t bytes = size * count;
    if (!bytes) return 0;
    char *next = realloc(buffer->data, buffer->size + bytes + 1);
    if (!next) return 0;
    buffer->data = next;
    memcpy(buffer->data + buffer->size, data, bytes);
    buffer->size += bytes;
    buffer->data[buffer->size] = '\0';
    return bytes;
}

static AtenaStatus http_json(OllamaImpl *impl, const char *method, const char *path,
                             const char *body, long timeout_ms,
                             char **out_body, long *out_http) {
    if (!impl || !method || !path) return ATENA_ERR_INVALID_ARGUMENT;
    if (out_body) *out_body = NULL;
    if (out_http) *out_http = 0;
    atena_once(&curl_once, initialize_curl);

    char url[768];
    int n = snprintf(url, sizeof(url), "%s%s", impl->base_url, path);
    if (n < 0 || (size_t)n >= sizeof(url)) return ATENA_ERR_INVALID_ARGUMENT;

    CURL *curl = curl_easy_init();
    if (!curl) return ATENA_ERR_INTERNAL;
    HttpBuffer buffer = {0};
    struct curl_slist *headers = make_headers(impl, body != NULL);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, collect_http);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 5000L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    if (strcmp(method, "GET") != 0) curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
    if (body) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(body));
    }

    CURLcode result = curl_easy_perform(curl);
    long http = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    if (out_http) *out_http = http;

    if (result == CURLE_OPERATION_TIMEDOUT) { free(buffer.data); return ATENA_ERR_TIMEOUT; }
    if (result != CURLE_OK) { free(buffer.data); return ATENA_ERR_NETWORK; }
    if (http < 200 || http >= 300) { free(buffer.data); return ATENA_ERR_PROVIDER_INVALID; }
    if (out_body) {
        if (!buffer.data) {
            buffer.data = malloc(1);
            if (!buffer.data) return ATENA_ERR_NO_MEMORY;
            buffer.data[0] = '\0';
        }
        *out_body = buffer.data;
    } else free(buffer.data);
    return ATENA_OK;
}

static int expose_reasoning(void) {
    const char *v=getenv("ATENA_EXPOSE_MODEL_THINKING");
    return v && !strcmp(v,"1");
}

static int emit_text_event(StreamContext *ctx, AtenaEventType type, const char *text) {
    if(!text||!*text)return 1;
    AtenaStreamEvent event={0}; event.type=type; event.operation_id=ctx->operation_id;
    event.seq=++ctx->sequence; event.text=text;
    return ctx->callback(&event,ctx->userdata)==0;
}

static int emit_line(StreamContext *ctx, const char *line, size_t length) {
    if (!length) return 1;
    char *copy=malloc(length+1);if(!copy){ctx->status=ATENA_ERR_NO_MEMORY;return 0;}memcpy(copy,line,length);copy[length]='\0';
    json_object *root=json_tokener_parse(copy);free(copy);if(!root){ctx->status=ATENA_ERR_JSON;return 0;}
    json_object *value=NULL,*message=NULL;
    if(json_object_object_get_ex(root,"error",&value)){ctx->status=ATENA_ERR_PROVIDER_INVALID;json_object_put(root);return 0;}
    if(json_object_object_get_ex(root,"message",&message) && json_object_is_type(message,json_type_object)){
        if(json_object_object_get_ex(message,"thinking",&value) && json_object_is_type(value,json_type_string)){
            const char *thinking=json_object_get_string(value); if(thinking)ctx->reasoning_bytes+=strlen(thinking);
            if(thinking&&*thinking&&expose_reasoning()&&!emit_text_event(ctx,ATENA_EVENT_REASONING_DELTA,thinking)){
                ctx->status=ATENA_ERR_CANCELLED;json_object_put(root);return 0;
            }
        }
        if(json_object_object_get_ex(message,"content",&value) && json_object_is_type(value,json_type_string)){
            const char *text=json_object_get_string(value); if(text&&*text){ctx->text_bytes+=strlen(text);if(!emit_text_event(ctx,ATENA_EVENT_TEXT_DELTA,text)){ctx->status=ATENA_ERR_CANCELLED;json_object_put(root);return 0;}}
        }
    }
    int done=0;if(json_object_object_get_ex(root,"done",&value))done=json_object_get_boolean(value);
    if(done){ctx->done=1;AtenaStreamEvent usage={0};usage.type=ATENA_EVENT_USAGE;usage.operation_id=ctx->operation_id;usage.seq=++ctx->sequence;if(json_object_object_get_ex(root,"prompt_eval_count",&value))usage.metrics.prompt_tokens=(uint64_t)json_object_get_int64(value);if(json_object_object_get_ex(root,"eval_count",&value))usage.metrics.completion_tokens=(uint64_t)json_object_get_int64(value);(void)ctx->callback(&usage,ctx->userdata);}
    json_object_put(root);return 1;
}

static size_t receive_data(char *data,size_t size,size_t count,void *userdata){
    StreamContext *ctx=userdata;size_t bytes=size*count;if(!bytes)return 0;if(is_cancelled(ctx->impl)){ctx->status=ATENA_ERR_CANCELLED;return 0;}
    char *next=realloc(ctx->pending,ctx->pending_size+bytes+1);if(!next){ctx->status=ATENA_ERR_NO_MEMORY;return 0;}ctx->pending=next;memcpy(ctx->pending+ctx->pending_size,data,bytes);ctx->pending_size+=bytes;ctx->pending[ctx->pending_size]='\0';
    size_t consumed=0;for(size_t i=0;i<ctx->pending_size;i++){if(ctx->pending[i]!='\n')continue;if(!emit_line(ctx,ctx->pending+consumed,i-consumed))return 0;consumed=i+1;}
    if(consumed){memmove(ctx->pending,ctx->pending+consumed,ctx->pending_size-consumed);ctx->pending_size-=consumed;ctx->pending[ctx->pending_size]='\0';}
    return bytes;
}

static int progress_callback(void *userdata,curl_off_t a,curl_off_t b,curl_off_t c,curl_off_t d){(void)a;(void)b;(void)c;(void)d;StreamContext*ctx=userdata;return is_cancelled(ctx->impl)?1:0;}

static void add_ollama_thinking(json_object *root, const AtenaProviderRequest *request,
                                const AtenaRuntimePlan *plan, int have_plan) {
    const char *env=getenv("ATENA_OLLAMA_THINK");
    if(env&&*env&&strcmp(env,"auto")){
        if(!strcmp(env,"0")||!strcmp(env,"false")||!strcmp(env,"off")||!strcmp(env,"none"))json_object_object_add(root,"think",json_object_new_boolean(0));
        else if(!strcmp(env,"1")||!strcmp(env,"true")||!strcmp(env,"on"))json_object_object_add(root,"think",json_object_new_boolean(1));
        else if(!strcmp(env,"low")||!strcmp(env,"medium")||!strcmp(env,"high")||!strcmp(env,"max"))json_object_object_add(root,"think",json_object_new_string(env));
        return;
    }
    switch(request->reasoning){
        case ATENA_REASONING_DISABLED: json_object_object_add(root,"think",json_object_new_boolean(0)); return;
        case ATENA_REASONING_LOW: json_object_object_add(root,"think",json_object_new_string("low")); return;
        case ATENA_REASONING_MEDIUM: json_object_object_add(root,"think",json_object_new_string("medium")); return;
        case ATENA_REASONING_HIGH: json_object_object_add(root,"think",json_object_new_string("high")); return;
        default: break;
    }
    /* AUTO: local reasoning is constrained by local resources. */
    if(!have_plan || !plan){ return; }
    if(!strcmp(plan->profile,"emergency") || !strcmp(plan->profile,"constrained"))
        json_object_object_add(root,"think",json_object_new_boolean(0));
    else if(!strcmp(plan->profile,"balanced"))
        json_object_object_add(root,"think",json_object_new_string("low"));
    else
        json_object_object_add(root,"think",json_object_new_string("medium"));
}

static AtenaStatus emit_nonstream_fallback(OllamaImpl *impl, json_object *root, StreamContext *ctx) {
    json_object_object_add(root,"stream",json_object_new_boolean(0));
    if(ctx->reasoning_bytes>0) json_object_object_add(root,"think",json_object_new_boolean(0));
    const char *body=json_object_to_json_string_ext(root,JSON_C_TO_STRING_PLAIN);
    char *raw=NULL; long http=0; AtenaStatus st=http_json(impl,"POST","/api/chat",body,600000L,&raw,&http);
    (void)http; if(st!=ATENA_OK)return st;
    json_object *response=json_tokener_parse(raw); free(raw); if(!response)return ATENA_ERR_JSON;
    json_object *message=NULL,*value=NULL; const char *text=NULL;
    if(json_object_object_get_ex(response,"message",&message)&&json_object_is_type(message,json_type_object)&&
       json_object_object_get_ex(message,"content",&value)&&json_object_is_type(value,json_type_string))text=json_object_get_string(value);
    if(text&&*text){ctx->text_bytes+=strlen(text);if(!emit_text_event(ctx,ATENA_EVENT_TEXT_DELTA,text)){json_object_put(response);return ATENA_ERR_CANCELLED;}}
    AtenaStreamEvent usage={0}; usage.type=ATENA_EVENT_USAGE; usage.operation_id=ctx->operation_id; usage.seq=++ctx->sequence;
    if(json_object_object_get_ex(response,"prompt_eval_count",&value))usage.metrics.prompt_tokens=(uint64_t)json_object_get_int64(value);
    if(json_object_object_get_ex(response,"eval_count",&value))usage.metrics.completion_tokens=(uint64_t)json_object_get_int64(value);
    (void)ctx->callback(&usage,ctx->userdata); json_object_put(response);
    return ctx->text_bytes?ATENA_OK:ATENA_ERR_PROVIDER_INVALID;
}

static AtenaStatus ollama_generate(AtenaProvider *provider,const AtenaProviderRequest *request,AtenaEventCallback callback,void *userdata){
    if(!provider||!request||!callback||!request->messages||!request->message_count)return ATENA_ERR_INVALID_ARGUMENT;
    OllamaImpl *impl=provider->impl;atena_once(&curl_once,initialize_curl);atena_mutex_lock(&impl->lock);impl->cancelled=0;snprintf(impl->active_operation,sizeof(impl->active_operation),"%s",request->operation_id?request->operation_id:"");atena_mutex_unlock(&impl->lock);
    json_object *root=json_object_new_object(),*messages=json_object_new_array(),*options=json_object_new_object();
    json_object_object_add(root,"model",json_object_new_string(request->model&&*request->model?request->model:provider->model));
    json_object_object_add(root,"stream",json_object_new_boolean(1));
    AtenaResourceSnapshot runtime_snapshot; AtenaRuntimePlan runtime_plan;
    int have_runtime_plan = !getenv("ATENA_DISABLE_RUNTIME_TUNING") &&
        atena_runtime_snapshot(&runtime_snapshot) == ATENA_OK &&
        atena_runtime_plan(&runtime_snapshot,&runtime_plan) == ATENA_OK;
    add_ollama_thinking(root,request,have_runtime_plan?&runtime_plan:NULL,have_runtime_plan);
    for(size_t i=0;i<request->message_count;i++){
        json_object*m=json_object_new_object();
        json_object_object_add(m,"role",json_object_new_string(role_name(request->messages[i].role)));
        json_object_object_add(m,"content",json_object_new_string(request->messages[i].content?request->messages[i].content:""));
        json_object_array_add(messages,m);
    }
    json_object_object_add(root,"messages",messages);
    json_object_object_add(options,"temperature",json_object_new_double(request->temperature));
    json_object_object_add(options,"top_p",json_object_new_double(request->top_p));
    json_object_object_add(options,"num_predict",json_object_new_int64((int64_t)request->max_output_tokens));

    /* CyberCore -> Atena Runtime integration: adapt Ollama execution to the
       resources available now. Environment variables remain an escape hatch
       for benchmarking and manual tuning. */
    if (have_runtime_plan) {
        long num_ctx = (long)runtime_plan.recommended_context_tokens;
        long num_thread = (long)runtime_plan.recommended_threads;
        long num_batch = (long)runtime_plan.recommended_batch_tokens;
        long keep_alive = (long)runtime_plan.recommended_keep_alive_seconds;
        const char *ctx_override = getenv("ATENA_OLLAMA_NUM_CTX");
        const char *thread_override = getenv("ATENA_OLLAMA_NUM_THREAD");
        const char *batch_override = getenv("ATENA_OLLAMA_NUM_BATCH");
        const char *keep_override = getenv("ATENA_OLLAMA_KEEP_ALIVE");
        if (ctx_override && *ctx_override) {
            long value = strtol(ctx_override, NULL, 10);
            if (value > 0) num_ctx = value;
        }
        if (thread_override && *thread_override) {
            long value = strtol(thread_override, NULL, 10);
            if (value > 0) num_thread = value;
        }
        if (batch_override && *batch_override) {
            long value = strtol(batch_override, NULL, 10);
            if (value >= 32) num_batch = value;
        }
        json_object_object_add(options,"num_ctx",json_object_new_int64((int64_t)num_ctx));
        json_object_object_add(options,"num_thread",json_object_new_int64((int64_t)num_thread));
        json_object_object_add(options,"num_batch",json_object_new_int64((int64_t)num_batch));
        if (keep_override && *keep_override) {
            json_object_object_add(root,"keep_alive",json_object_new_string(keep_override));
        } else if (runtime_plan.keep_model_resident && keep_alive > 0) {
            json_object_object_add(root,"keep_alive",json_object_new_int64((int64_t)keep_alive));
        } else {
            json_object_object_add(root,"keep_alive",json_object_new_int(0));
        }
    }
    json_object_object_add(root,"options",options);
    const char *body=json_object_to_json_string_ext(root,JSON_C_TO_STRING_PLAIN);char url[640];int n=snprintf(url,sizeof(url),"%s/api/chat",impl->base_url);if(n<0||(size_t)n>=sizeof(url)){json_object_put(root);return ATENA_ERR_INVALID_ARGUMENT;}
    CURL *curl=curl_easy_init();if(!curl){json_object_put(root);return ATENA_ERR_INTERNAL;}struct curl_slist *headers=make_headers(impl,1);StreamContext ctx={impl,request->operation_id,callback,userdata,NULL,0,0,0,0,0,ATENA_OK};
    curl_easy_setopt(curl,CURLOPT_URL,url);curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers);curl_easy_setopt(curl,CURLOPT_POSTFIELDS,body);curl_easy_setopt(curl,CURLOPT_POSTFIELDSIZE,(long)strlen(body));curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,receive_data);curl_easy_setopt(curl,CURLOPT_WRITEDATA,&ctx);curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT_MS,5000L);curl_easy_setopt(curl,CURLOPT_TIMEOUT_MS,600000L);curl_easy_setopt(curl,CURLOPT_LOW_SPEED_LIMIT,1L);curl_easy_setopt(curl,CURLOPT_LOW_SPEED_TIME,180L);curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L);curl_easy_setopt(curl,CURLOPT_NOPROGRESS,0L);curl_easy_setopt(curl,CURLOPT_XFERINFOFUNCTION,progress_callback);curl_easy_setopt(curl,CURLOPT_XFERINFODATA,&ctx);
    CURLcode result=curl_easy_perform(curl);long http=0;curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&http);if(ctx.pending_size&&ctx.status==ATENA_OK)(void)emit_line(&ctx,ctx.pending,ctx.pending_size);
    free(ctx.pending);curl_slist_free_all(headers);curl_easy_cleanup(curl);
    AtenaStatus final_status=ATENA_OK;
    if(ctx.status!=ATENA_OK)final_status=ctx.status;
    else if(result==CURLE_OPERATION_TIMEDOUT)final_status=ATENA_ERR_TIMEOUT;
    else if(result==CURLE_ABORTED_BY_CALLBACK)final_status=ATENA_ERR_CANCELLED;
    else if(result!=CURLE_OK)final_status=ATENA_ERR_NETWORK;
    else if(http<200||http>=300)final_status=ATENA_ERR_PROVIDER_INVALID;
    else if(ctx.text_bytes==0)final_status=emit_nonstream_fallback(impl,root,&ctx);
    json_object_put(root);
    atena_mutex_lock(&impl->lock);impl->active_operation[0]='\0';atena_mutex_unlock(&impl->lock);
    return final_status;
}

static AtenaStatus ollama_cancel(AtenaProvider*p,const char*operation){if(!p||!operation)return ATENA_ERR_INVALID_ARGUMENT;OllamaImpl*i=p->impl;atena_mutex_lock(&i->lock);if(!strcmp(i->active_operation,operation))i->cancelled=1;atena_mutex_unlock(&i->lock);return ATENA_OK;}
static void ollama_destroy(AtenaProvider*p){if(!p)return;OllamaImpl*i=p->impl;if(i)atena_mutex_destroy(&i->lock);free(i);free(p);}
static const AtenaProviderVTable OLLAMA_VTABLE={ollama_generate,ollama_cancel,ollama_destroy};

AtenaProvider *atena_ollama_provider_create(const char *id,const char *base_url,const char *model){
    if(!id||!*id)return NULL;
    AtenaProvider*p=calloc(1,sizeof(*p));OllamaImpl*i=calloc(1,sizeof(*i));
    if(!p||!i){free(p);free(i);return NULL;}
    snprintf(p->id,sizeof(p->id),"%s",id);snprintf(p->type,sizeof(p->type),"ollama");snprintf(p->model,sizeof(p->model),"%s",model&&*model?model:OLLAMA_DEFAULT_MODEL);snprintf(i->base_url,sizeof(i->base_url),"%s",base_url&&*base_url?base_url:OLLAMA_DEFAULT_URL);
    size_t len=strlen(i->base_url);while(len>0&&i->base_url[len-1]=='/'){i->base_url[len-1]='\0';len--;}
    atena_mutex_init(&i->lock);p->capabilities=ATENA_CAP_TEXT|ATENA_CAP_STREAMING|ATENA_CAP_USAGE|ATENA_CAP_CANCELLATION|ATENA_CAP_MODEL_LIST|ATENA_CAP_REASONING_CONTROL;p->capabilities_known=ATENA_CAP_TEXT|ATENA_CAP_STREAMING|ATENA_CAP_TOOLS|ATENA_CAP_MODEL_LIST|ATENA_CAP_USAGE|ATENA_CAP_CANCELLATION|ATENA_CAP_REASONING_CONTROL;p->vtable=&OLLAMA_VTABLE;p->impl=i;return p;
}

AtenaStatus atena_ollama_provider_set_bearer_token(AtenaProvider *provider,const char *token){
    if(!provider||strcmp(provider->type,"ollama")!=0||!provider->impl)return ATENA_ERR_INVALID_ARGUMENT;
    OllamaImpl *impl=provider->impl;atena_mutex_lock(&impl->lock);snprintf(impl->bearer_token,sizeof(impl->bearer_token),"%s",token?token:"");atena_mutex_unlock(&impl->lock);return ATENA_OK;
}

AtenaStatus atena_ollama_provider_test_json(AtenaProvider *provider,char **out_json){
    if(!provider||!out_json||strcmp(provider->type,"ollama")!=0)return ATENA_ERR_INVALID_ARGUMENT;
    *out_json=NULL;char *raw=NULL;long http=0;AtenaStatus st=http_json(provider->impl,"GET","/api/version",NULL,15000L,&raw,&http);(void)http;if(st!=ATENA_OK)return st;
    json_object *remote=json_tokener_parse(raw);free(raw);json_object *result=json_object_new_object();json_object *v=NULL;const char *version="desconhecida";
    if(remote&&json_object_object_get_ex(remote,"version",&v)&&json_object_is_type(v,json_type_string))version=json_object_get_string(v);
    json_object_object_add(result,"ok",json_object_new_boolean(1));json_object_object_add(result,"provider_id",json_object_new_string(provider->id));json_object_object_add(result,"version",json_object_new_string(version));
    char message[256];snprintf(message,sizeof(message),"Ollama conectado · versão %s",version);json_object_object_add(result,"message",json_object_new_string(message));
    *out_json=strdup(json_object_to_json_string_ext(result,JSON_C_TO_STRING_PLAIN));if(remote)json_object_put(remote);json_object_put(result);return *out_json?ATENA_OK:ATENA_ERR_NO_MEMORY;
}

static void human_size(int64_t bytes,char out[64]){
    static const char *units[]={"B","KiB","MiB","GiB","TiB"};double value=(double)bytes;size_t unit=0;while(value>=1024.0&&unit<4){value/=1024.0;unit++;}snprintf(out,64,unit==0?"%.0f %s":"%.1f %s",value,units[unit]);
}

AtenaStatus atena_ollama_provider_models_json(AtenaProvider *provider,char **out_json){
    if(!provider||!out_json||strcmp(provider->type,"ollama")!=0)return ATENA_ERR_INVALID_ARGUMENT;
    *out_json=NULL;char *raw=NULL;AtenaStatus st=http_json(provider->impl,"GET","/api/tags",NULL,30000L,&raw,NULL);if(st!=ATENA_OK)return st;
    json_object *remote=json_tokener_parse(raw);free(raw);if(!remote)return ATENA_ERR_JSON;json_object *models=NULL;if(!json_object_object_get_ex(remote,"models",&models)||!json_object_is_type(models,json_type_array)){json_object_put(remote);return ATENA_ERR_PROTOCOL;}
    json_object *items=json_object_new_array();size_t count=json_object_array_length(models);for(size_t i=0;i<count;i++){json_object *src=json_object_array_get_idx(models,i),*v=NULL;if(!src)continue;const char *name=NULL;int64_t size=0;if(json_object_object_get_ex(src,"name",&v))name=json_object_get_string(v);if(!name||!*name)continue;if(json_object_object_get_ex(src,"size",&v))size=json_object_get_int64(v);char size_text[64];human_size(size,size_text);json_object *m=json_object_new_object();json_object_object_add(m,"id",json_object_new_string(name));json_object_object_add(m,"name",json_object_new_string(name));json_object_object_add(m,"provider_id",json_object_new_string(provider->id));json_object_object_add(m,"runtime",json_object_new_string("Ollama"));json_object_object_add(m,"size",json_object_new_string(size_text));json_object_object_add(m,"size_bytes",json_object_new_int64(size));json_object_object_add(m,"active",json_object_new_boolean(strcmp(name,provider->model)==0));json_object_array_add(items,m);}
    *out_json=strdup(json_object_to_json_string_ext(items,JSON_C_TO_STRING_PLAIN));json_object_put(items);json_object_put(remote);return *out_json?ATENA_OK:ATENA_ERR_NO_MEMORY;
}

AtenaStatus atena_ollama_provider_pull_model(AtenaProvider *provider, const char *model) {
    if (!provider || !model || !*model || strcmp(provider->type, "ollama") != 0)
        return ATENA_ERR_INVALID_ARGUMENT;
    json_object *root = json_object_new_object();
    if (!root) return ATENA_ERR_NO_MEMORY;
    json_object_object_add(root, "name", json_object_new_string(model));
    json_object_object_add(root, "stream", json_object_new_boolean(0));
    const char *body = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    AtenaStatus st = http_json(provider->impl, "POST", "/api/pull", body, 1800000L, NULL, NULL);
    json_object_put(root);
    return st;
}

AtenaStatus atena_ollama_provider_remove_model(AtenaProvider *provider, const char *model) {
    if (!provider || !model || !*model || strcmp(provider->type, "ollama") != 0)
        return ATENA_ERR_INVALID_ARGUMENT;
    json_object *root = json_object_new_object();
    if (!root) return ATENA_ERR_NO_MEMORY;
    json_object_object_add(root, "name", json_object_new_string(model));
    const char *body = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    AtenaStatus st = http_json(provider->impl, "DELETE", "/api/delete", body, 120000L, NULL, NULL);
    json_object_put(root);
    return st;
}
