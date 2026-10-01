#include "atena/core.h"
#include "context.h"
#include "util.h"
#include "../memory/store.h"
#include "../tools/tools.h"
#include <json-c/json.h>
#include "atena/sync.h"
#include "atena/secret.h"
#include "atena/runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ATENA_MAX_PROVIDERS 16
#define ATENA_MAX_ACTIVE_OPS 16
#define ATENA_MAX_TOOL_CALLS 4

struct AtenaActiveOperation {
    char id[37];
    AtenaProvider *provider;
    int cancelled;
};

struct AtenaCore {
    AtenaStore *store;
    char *identity_dir;
    size_t context_budget;
    size_t rag_results;
    int offline_mode;
    AtenaProvider *providers[ATENA_MAX_PROVIDERS];
    size_t provider_count;
    char selected_provider[64];
    struct AtenaActiveOperation active[ATENA_MAX_ACTIVE_OPS];
    AtenaMutex lock;
};

typedef struct StreamCollector {
    AtenaCore *core;
    const char *operation_id;
    AtenaEventCallback downstream;
    void *downstream_ud;
    char *text;
    size_t text_len;
    size_t text_cap;
    char tool_name[128];
    char *tool_json;
    AtenaMetrics metrics;
    uint64_t first_delta_ms;
    uint64_t start_ms;
    uint64_t seq;
} StreamCollector;

static AtenaProvider *find_provider(AtenaCore *core, const char *id) {
    if (!core || !id) return NULL;
    for (size_t i=0;i<core->provider_count;i++) if (strcmp(core->providers[i]->id,id)==0) return core->providers[i];
    return NULL;
}

static int active_add(AtenaCore *core, const char *id, AtenaProvider *provider) {
    atena_mutex_lock(&core->lock);
    for(size_t i=0;i<ATENA_MAX_ACTIVE_OPS;i++) {
        if(!core->active[i].id[0]) {
            snprintf(core->active[i].id,37,"%s",id);core->active[i].provider=provider;core->active[i].cancelled=0;
            atena_mutex_unlock(&core->lock);return 1;
        }
    }
    atena_mutex_unlock(&core->lock);return 0;
}

static void active_remove(AtenaCore *core,const char *id){atena_mutex_lock(&core->lock);for(size_t i=0;i<ATENA_MAX_ACTIVE_OPS;i++)if(strcmp(core->active[i].id,id)==0){memset(&core->active[i],0,sizeof(core->active[i]));break;}atena_mutex_unlock(&core->lock);}

static int active_cancelled(AtenaCore *core,const char *id){int c=0;atena_mutex_lock(&core->lock);for(size_t i=0;i<ATENA_MAX_ACTIVE_OPS;i++)if(strcmp(core->active[i].id,id)==0){c=core->active[i].cancelled;break;}atena_mutex_unlock(&core->lock);return c;}

static int append_text(StreamCollector *c, const char *text) {
    if (!text) return 1;
    size_t n = strlen(text);
    if (c->text_len + n + 1 > c->text_cap) {
        size_t next = c->text_cap ? c->text_cap * 2 : 512;
        while (next < c->text_len+n+1) next *= 2;
        char *tmp = realloc(c->text, next);
        if (!tmp) return 0;
        c->text = tmp; c->text_cap = next;
    }
    memcpy(c->text+c->text_len,text,n+1);c->text_len+=n;return 1;
}

static int collector_cb(const AtenaStreamEvent *event, void *userdata) {
    StreamCollector *c = userdata;
    if (active_cancelled(c->core,c->operation_id)) return 1;
    if (event->type == ATENA_EVENT_TEXT_DELTA) {
        if (!c->first_delta_ms) c->first_delta_ms = atena_now_monotonic_ms();
        if (!append_text(c,event->text)) return 1;
    } else if (event->type == ATENA_EVENT_TOOL_CALL) {
        snprintf(c->tool_name,sizeof(c->tool_name),"%s",event->tool_name?event->tool_name:"");
        free(c->tool_json); c->tool_json=atena_strdup(event->tool_json?event->tool_json:"{}");
        if(!c->tool_json)return 1;
    } else if (event->type == ATENA_EVENT_USAGE) {
        c->metrics = event->metrics;
    }
    if (c->downstream) {
        AtenaStreamEvent copy=*event; copy.seq=++c->seq;
        return c->downstream(&copy,c->downstream_ud);
    }
    return 0;
}

static AtenaStatus persist_message(AtenaCore *core,const char *session_id,AtenaRole role,AtenaMessageState state,const char *content,char out_id[37]) {
    AtenaMessage m={0};
    if(!atena_uuid4(m.id))return ATENA_ERR_IO;
    snprintf(m.session_id,sizeof(m.session_id),"%s",session_id);m.role=role;m.state=state;m.content=(char*)(content?content:"");atena_now_iso8601(m.created_at);
    AtenaStatus st=atena_store_message_add(core->store,&m);if(out_id)snprintf(out_id,37,"%s",m.id);return st;
}

static AtenaStatus messages_copy_with_tool(const AtenaBuiltContext *ctx,const char *session_id,const char *tool_json,AtenaMessage **out,size_t *out_count) {
    size_t n=ctx->message_count+1;AtenaMessage*m=calloc(n,sizeof(*m));if(!m)return ATENA_ERR_NO_MEMORY;
    for(size_t i=0;i<ctx->message_count;i++){m[i]=ctx->messages[i];m[i].content=atena_strdup(ctx->messages[i].content);if(!m[i].content){for(size_t j=0;j<i;j++)free(m[j].content);free(m);return ATENA_ERR_NO_MEMORY;}}
    AtenaMessage *t=&m[n-1];atena_uuid4(t->id);snprintf(t->session_id,37,"%s",session_id);t->role=ATENA_ROLE_TOOL;t->state=ATENA_MSG_COMPLETE;t->content=atena_strdup(tool_json?tool_json:"{}");atena_now_iso8601(t->created_at);if(!t->content){for(size_t j=0;j<n-1;j++)free(m[j].content);free(m);return ATENA_ERR_NO_MEMORY;}*out=m;*out_count=n;return ATENA_OK;
}

static void free_messages(AtenaMessage *m,size_t n){if(!m)return;for(size_t i=0;i<n;i++)free(m[i].content);free(m);}

AtenaStatus atena_core_create(const AtenaCoreConfig *config, AtenaCore **out_core) {
    if(!config||!config->database_path||!config->identity_dir||!out_core) return ATENA_ERR_INVALID_ARGUMENT;
    *out_core=NULL;
    AtenaCore*c=calloc(1,sizeof(*c));if(!c)return ATENA_ERR_NO_MEMORY;
    c->identity_dir=atena_strdup(config->identity_dir);if(!c->identity_dir){free(c);return ATENA_ERR_NO_MEMORY;}
    c->context_budget=config->default_context_budget_chars?config->default_context_budget_chars:12000;c->rag_results=config->default_rag_results?config->default_rag_results:3;c->offline_mode=config->offline_mode;
    atena_mutex_init(&c->lock);
    AtenaStatus st=atena_store_open(config->database_path,&c->store);if(st!=ATENA_OK){atena_mutex_destroy(&c->lock);free(c->identity_dir);free(c);return st;}
    *out_core=c;return ATENA_OK;
}

void atena_core_destroy(AtenaCore *core){if(!core)return;for(size_t i=0;i<core->provider_count;i++)if(core->providers[i]&&core->providers[i]->vtable&&core->providers[i]->vtable->destroy)core->providers[i]->vtable->destroy(core->providers[i]);atena_store_close(core->store);atena_mutex_destroy(&core->lock);free(core->identity_dir);free(core);}

static void warn_persistence(const char *operation, AtenaStatus st) {
    if (st != ATENA_OK)
        fprintf(stderr, "Atena storage: aviso: %s não persistido (%s); runtime continuará em memória.\n",
                operation ? operation : "estado", atena_status_string(st));
}

AtenaStatus atena_core_register_provider(AtenaCore*core,AtenaProvider*provider){
    if(!core||!provider||!provider->id[0]||!provider->vtable||!provider->vtable->generate)return ATENA_ERR_INVALID_ARGUMENT;
    atena_mutex_lock(&core->lock);
    if(core->provider_count>=ATENA_MAX_PROVIDERS){atena_mutex_unlock(&core->lock);return ATENA_ERR_BUSY;}
    for(size_t i=0;i<core->provider_count;i++)if(strcmp(core->providers[i]->id,provider->id)==0){atena_mutex_unlock(&core->lock);return ATENA_ERR_CONFLICT;}
    core->providers[core->provider_count++]=provider;
    if(!core->selected_provider[0]) snprintf(core->selected_provider,sizeof(core->selected_provider),"%s",provider->id);
    atena_mutex_unlock(&core->lock);
    AtenaStatus pst=atena_store_provider_upsert(core->store,provider->id,provider->type,provider->model,provider->capabilities,provider->capabilities_known,1);
    warn_persistence("provider registry",pst);
    return ATENA_OK;
}


static int provider_is_active_locked(AtenaCore *core, AtenaProvider *provider) {
    for (size_t i=0;i<ATENA_MAX_ACTIVE_OPS;i++)
        if (core->active[i].provider == provider && core->active[i].id[0]) return 1;
    return 0;
}

static char *preference_or_null(AtenaCore *core, const char *key) {
    char *value = NULL;
    if (atena_store_preference_get(core->store, key, &value) != ATENA_OK) return NULL;
    return value;
}

static int provider_pref_key(char out[160], const char *provider_id, const char *field) {
    int n = snprintf(out, 160, "provider.%s.%s", provider_id, field);
    return n >= 0 && n < 160;
}

static const char *cloud_default_endpoint(const char *provider_id) {
    if (!strcmp(provider_id, "openai")) return "https://api.openai.com/v1";
    if (!strcmp(provider_id, "groq")) return "https://api.groq.com/openai/v1";
    if (!strcmp(provider_id, "deepseek")) return "https://api.deepseek.com/v1";
    if (!strcmp(provider_id, "xai")) return "https://api.x.ai/v1";
    if (!strcmp(provider_id, "gemini")) return "https://generativelanguage.googleapis.com/v1beta/openai";
    return NULL;
}

static int is_openai_compatible(const char *provider_id, const char *type) {
    if (type && !strcmp(type, "openai_compatible")) return 1;
    return provider_id && (!strcmp(provider_id, "openai") ||
                           !strcmp(provider_id, "openai_compatible") ||
                           !strcmp(provider_id, "groq") ||
                           !strcmp(provider_id, "deepseek") ||
                           !strcmp(provider_id, "xai") ||
                           !strcmp(provider_id, "gemini"));
}

static AtenaStatus replace_provider(AtenaCore *core, AtenaProvider *replacement) {
    AtenaProvider *old = NULL;
    atena_mutex_lock(&core->lock);
    size_t index = core->provider_count;
    for (size_t i=0;i<core->provider_count;i++) {
        if (!strcmp(core->providers[i]->id, replacement->id)) { index=i; break; }
    }
    if (index < core->provider_count) {
        old = core->providers[index];
        if (provider_is_active_locked(core, old)) {
            atena_mutex_unlock(&core->lock);
            replacement->vtable->destroy(replacement);
            return ATENA_ERR_BUSY;
        }
        core->providers[index] = replacement;
    } else {
        if (core->provider_count >= ATENA_MAX_PROVIDERS) {
            atena_mutex_unlock(&core->lock);
            replacement->vtable->destroy(replacement);
            return ATENA_ERR_BUSY;
        }
        core->providers[core->provider_count++] = replacement;
    }
    atena_mutex_unlock(&core->lock);
    if (old && old->vtable && old->vtable->destroy) old->vtable->destroy(old);
    return ATENA_OK;
}

AtenaStatus atena_core_provider_configure(AtenaCore *core,
                                          const char *provider_id,
                                          const char *provider_type,
                                          const char *endpoint,
                                          const char *model,
                                          const char *session_secret) {
    if (!core || !provider_id || !*provider_id) return ATENA_ERR_INVALID_ARGUMENT;
    const char *type = provider_type && *provider_type ? provider_type : provider_id;

    char endpoint_key[160], model_key[160];
    if (!provider_pref_key(endpoint_key, provider_id, "endpoint") ||
        !provider_pref_key(model_key, provider_id, "model")) return ATENA_ERR_INVALID_ARGUMENT;

    char *stored_endpoint = NULL, *stored_model = NULL, *stored_secret = NULL;
    if (!endpoint || !*endpoint) stored_endpoint = preference_or_null(core, endpoint_key);
    if (!model || !*model) stored_model = preference_or_null(core, model_key);
    const char *effective_endpoint = endpoint && *endpoint ? endpoint : (stored_endpoint && *stored_endpoint ? stored_endpoint : NULL);
    const char *effective_model = model && *model ? model : (stored_model && *stored_model ? stored_model : NULL);
    const char *effective_secret = session_secret && *session_secret ? session_secret : NULL;
    int should_lookup_secret = strcmp(provider_id, "ollama") != 0;
    if (!should_lookup_secret && stored_endpoint && *stored_endpoint &&
        !strstr(stored_endpoint, "127.0.0.1") && !strstr(stored_endpoint, "localhost"))
        should_lookup_secret = 1;
    if (!effective_secret && should_lookup_secret && atena_secret_lookup(provider_id, &stored_secret) == ATENA_OK)
        effective_secret = stored_secret;

    AtenaProvider *replacement = NULL;
    if (!strcmp(provider_id, "ollama") && !strcmp(type, "ollama")) {
        if (!effective_endpoint) effective_endpoint = "http://127.0.0.1:11434";
        if (!effective_model) effective_model = "qwen3:0.6b";
        replacement = atena_ollama_provider_create("ollama", effective_endpoint, effective_model);
        if (replacement && effective_secret && *effective_secret)
            (void)atena_ollama_provider_set_bearer_token(replacement, effective_secret);
    } else if (is_openai_compatible(provider_id, type)) {
        if (!effective_endpoint) effective_endpoint = cloud_default_endpoint(provider_id);
        if (!effective_endpoint || !*effective_endpoint) {
            atena_secret_free(stored_secret); free(stored_endpoint); free(stored_model);
            return ATENA_ERR_INVALID_ARGUMENT;
        }
        /* Named cloud presets require a credential. Custom OpenAI-compatible endpoints
         * may intentionally be keyless (for example a local gateway). */
        if (strcmp(provider_id, "openai_compatible") && (!effective_secret || !*effective_secret)) {
            atena_secret_free(stored_secret); free(stored_endpoint); free(stored_model);
            return ATENA_ERR_PROVIDER_UNAVAILABLE;
        }
        replacement = atena_openai_compatible_provider_create(provider_id, effective_endpoint,
                                                               effective_model ? effective_model : "",
                                                               effective_secret ? effective_secret : "");
    } else {
        atena_secret_free(stored_secret); free(stored_endpoint); free(stored_model);
        return ATENA_ERR_UNSUPPORTED;
    }

    if (!replacement) {
        atena_secret_free(stored_secret); free(stored_endpoint); free(stored_model);
        return ATENA_ERR_NO_MEMORY;
    }

    AtenaStatus st = replace_provider(core, replacement);
    if (st != ATENA_OK) {
        atena_secret_free(stored_secret); free(stored_endpoint); free(stored_model);
        return st;
    }

    /* Runtime availability must not depend on persistence. A provider is already
     * usable in memory at this point. Persist each field best-effort and keep
     * running if a legacy/corrupted store rejects the write. */
    AtenaStatus pst = atena_store_provider_upsert(core->store, replacement->id, replacement->type, replacement->model,
                                                  replacement->capabilities, replacement->capabilities_known, 1);
    warn_persistence("provider metadata", pst);
    if (effective_endpoint && *effective_endpoint) {
        pst = atena_store_preference_put(core->store, endpoint_key, effective_endpoint);
        warn_persistence("provider endpoint", pst);
    }
    if (effective_model && *effective_model) {
        pst = atena_store_preference_put(core->store, model_key, effective_model);
        warn_persistence("provider model", pst);
    }
    if (session_secret && *session_secret && atena_secret_persistence_available())
        (void)atena_secret_store(provider_id, session_secret);

    atena_mutex_lock(&core->lock);
    if (!core->selected_provider[0] || !strcmp(provider_id, "ollama"))
        snprintf(core->selected_provider, sizeof(core->selected_provider), "%s", provider_id);
    atena_mutex_unlock(&core->lock);

    atena_secret_free(stored_secret);
    free(stored_endpoint);
    free(stored_model);
    return ATENA_OK;
}

AtenaStatus atena_core_provider_select(AtenaCore *core, const char *provider_id) {
    if (!core || !provider_id || !*provider_id) return ATENA_ERR_INVALID_ARGUMENT;
    if (!find_provider(core, provider_id)) return ATENA_ERR_PROVIDER_UNAVAILABLE;
    atena_mutex_lock(&core->lock);
    snprintf(core->selected_provider, sizeof(core->selected_provider), "%s", provider_id);
    atena_mutex_unlock(&core->lock);
    AtenaStatus pst = atena_store_preference_put(core->store, "provider.default", provider_id);
    warn_persistence("default provider", pst);
    return ATENA_OK;
}

AtenaStatus atena_core_restore_default_provider(AtenaCore *core) {
    if (!core) return ATENA_ERR_INVALID_ARGUMENT;
    char *provider_id = NULL;
    AtenaStatus st = atena_store_preference_get(core->store, "provider.default", &provider_id);
    if (st == ATENA_ERR_NOT_FOUND) return ATENA_OK;
    if (st != ATENA_OK) {
        warn_persistence("restore default provider", st);
        return ATENA_OK;
    }
    if (!provider_id || !*provider_id) { free(provider_id); return ATENA_OK; }
    if (!strcmp(provider_id, "ollama") || find_provider(core, provider_id)) {
        atena_mutex_lock(&core->lock);
        snprintf(core->selected_provider, sizeof(core->selected_provider), "%s", provider_id);
        atena_mutex_unlock(&core->lock);
        free(provider_id);
        return ATENA_OK;
    }
    st = atena_core_provider_configure(core, provider_id, "openai_compatible", NULL, NULL, NULL);
    if (st != ATENA_OK) {
        fprintf(stderr, "Atena provider: aviso: não foi possível restaurar '%s' (%s); usando runtime disponível.\n",
                provider_id, atena_status_string(st));
        st = ATENA_OK;
    }
    free(provider_id);
    return st;
}

AtenaStatus atena_core_provider_test_json(AtenaCore *core, const char *provider_id, char **out_json) {
    if (!core || !provider_id || !out_json) return ATENA_ERR_INVALID_ARGUMENT;
    AtenaProvider *provider = find_provider(core, provider_id);
    if (!provider) return ATENA_ERR_PROVIDER_UNAVAILABLE;
    if (!strcmp(provider->type, "ollama")) return atena_ollama_provider_test_json(provider, out_json);
    if (!strcmp(provider->type, "openai_compatible")) return atena_openai_compatible_provider_test_json(provider, out_json);
    return ATENA_ERR_UNSUPPORTED;
}

AtenaStatus atena_core_models_list_json(AtenaCore *core, const char *provider_id, char **out_json) {
    if (!core || !provider_id || !out_json) return ATENA_ERR_INVALID_ARGUMENT;
    AtenaProvider *provider = find_provider(core, provider_id);
    if (!provider) return ATENA_ERR_PROVIDER_UNAVAILABLE;
    if (!strcmp(provider->type, "ollama")) return atena_ollama_provider_models_json(provider, out_json);
    if (!strcmp(provider->type, "openai_compatible")) return atena_openai_compatible_provider_models_json(provider, out_json);
    return ATENA_ERR_UNSUPPORTED;
}

AtenaStatus atena_core_model_select(AtenaCore *core, const char *provider_id, const char *model) {
    if (!core || !provider_id || !model || !*model) return ATENA_ERR_INVALID_ARGUMENT;
    AtenaProvider *provider = find_provider(core, provider_id);
    if (!provider) return ATENA_ERR_PROVIDER_UNAVAILABLE;
    if (strlen(model) >= sizeof(provider->model)) return ATENA_ERR_INVALID_ARGUMENT;
    atena_mutex_lock(&core->lock);
    snprintf(provider->model, sizeof(provider->model), "%s", model);
    atena_mutex_unlock(&core->lock);
    AtenaStatus pst = atena_store_provider_upsert(core->store, provider->id, provider->type, provider->model,
                                                  provider->capabilities, provider->capabilities_known, 1);
    warn_persistence("selected model metadata", pst);
    char model_key[160];
    if (provider_pref_key(model_key, provider_id, "model")) {
        pst = atena_store_preference_put(core->store, model_key, model);
        warn_persistence("selected model", pst);
    }
    (void)atena_core_provider_select(core, provider_id);
    return ATENA_OK;
}

AtenaStatus atena_core_model_pull(AtenaCore *core, const char *provider_id, const char *model) {
    if (!core || !provider_id || !model || !*model) return ATENA_ERR_INVALID_ARGUMENT;
    AtenaProvider *provider = find_provider(core, provider_id);
    if (!provider) return ATENA_ERR_PROVIDER_UNAVAILABLE;
    if (strcmp(provider->type, "ollama")) return ATENA_ERR_UNSUPPORTED;
    return atena_ollama_provider_pull_model(provider, model);
}

AtenaStatus atena_core_model_remove(AtenaCore *core, const char *provider_id, const char *model) {
    if (!core || !provider_id || !model || !*model) return ATENA_ERR_INVALID_ARGUMENT;
    AtenaProvider *provider = find_provider(core, provider_id);
    if (!provider) return ATENA_ERR_PROVIDER_UNAVAILABLE;
    if (!strcmp(provider->model, model)) return ATENA_ERR_CONFLICT;
    if (strcmp(provider->type, "ollama")) return ATENA_ERR_UNSUPPORTED;
    return atena_ollama_provider_remove_model(provider, model);
}

AtenaStatus atena_core_session_create(AtenaCore*core,const char*title,char out_session_id[37]){if(!core)return ATENA_ERR_INVALID_ARGUMENT;AtenaStatus st=atena_store_session_create(core->store,title,out_session_id);if(st==ATENA_OK)atena_store_audit(core->store,"sessions.created",out_session_id,NULL,"{}");return st;}
AtenaStatus atena_core_session_history(AtenaCore*core,const char*session_id,AtenaMessage**out_messages,size_t*out_count){if(!core)return ATENA_ERR_INVALID_ARGUMENT;return atena_store_history(core->store,session_id,out_messages,out_count);}
void atena_core_messages_free(AtenaMessage*m,size_t count){free_messages(m,count);}
AtenaStatus atena_core_rag_import_text(AtenaCore*core,const char*title,const char*locator,const char*text,char out_document_id[37]){if(!core)return ATENA_ERR_INVALID_ARGUMENT;return atena_store_rag_import_text(core->store,title,locator,text,out_document_id);}
AtenaStatus atena_core_documents_json(AtenaCore*core,char**out_json){if(!core||!out_json)return ATENA_ERR_INVALID_ARGUMENT;return atena_store_documents_json(core->store,out_json);}
AtenaStatus atena_core_identity_json(AtenaCore*core,char**out_json){if(!core||!out_json)return ATENA_ERR_INVALID_ARGUMENT;*out_json=NULL;char*effective=NULL;AtenaStatus st=atena_identity_effective_text(core->identity_dir,&effective);if(st!=ATENA_OK)return st;json_object*o=json_object_new_object();if(!o){free(effective);return ATENA_ERR_NO_MEMORY;}json_object_object_add(o,"identity_dir",json_object_new_string(core->identity_dir));json_object_object_add(o,"effective_prompt",json_object_new_string(effective));free(effective);*out_json=atena_strdup(json_object_to_json_string_ext(o,JSON_C_TO_STRING_PLAIN));json_object_put(o);return *out_json?ATENA_OK:ATENA_ERR_NO_MEMORY;}
AtenaStatus atena_core_memory_put(AtenaCore*core,const char*key,const char*value){if(!core)return ATENA_ERR_INVALID_ARGUMENT;return atena_store_preference_put(core->store,key,value);}

AtenaStatus atena_core_chat_send(AtenaCore *core,const AtenaChatRequest *request,AtenaEventCallback callback,void *userdata,char out_operation_id[37]) {
    if(!core||!request||!request->session_id||!request->user_text||!out_operation_id)return ATENA_ERR_INVALID_ARGUMENT;
    int exists=0;AtenaStatus st=atena_store_session_exists(core->store,request->session_id,&exists);if(st!=ATENA_OK)return st;if(!exists)return ATENA_ERR_NOT_FOUND;
    char *default_provider=NULL;
    const char *provider_id=request->provider_id&&*request->provider_id?request->provider_id:NULL;
    char selected_provider[64]={0};
    if(!provider_id){
        atena_mutex_lock(&core->lock);
        snprintf(selected_provider,sizeof(selected_provider),"%s",core->selected_provider);
        atena_mutex_unlock(&core->lock);
        if(selected_provider[0]) provider_id=selected_provider;
    }
    if(!provider_id&&atena_store_preference_get(core->store,"provider.default",&default_provider)==ATENA_OK&&default_provider&&*default_provider)provider_id=default_provider;
    AtenaProvider*provider=provider_id?find_provider(core,provider_id):NULL;
    if(!provider)provider=find_provider(core,"ollama");
    if(!provider&&core->provider_count)provider=core->providers[0];
    free(default_provider);
    if(!provider)return ATENA_ERR_PROVIDER_UNAVAILABLE;
    if(!(provider->capabilities&ATENA_CAP_TEXT))return ATENA_ERR_PROVIDER_INVALID;
    if(request->idempotency_key&&*request->idempotency_key){char prior[37],state[24];st=atena_store_operation_by_key(core->store,request->session_id,request->idempotency_key,prior,state);if(st==ATENA_OK){snprintf(out_operation_id,37,"%s",prior);return ATENA_ERR_CONFLICT;}if(st!=ATENA_ERR_NOT_FOUND)return st;}
    if(!atena_uuid4(out_operation_id))return ATENA_ERR_IO;
    st=atena_store_operation_begin(core->store,out_operation_id,request->session_id,request->idempotency_key);if(st!=ATENA_OK)return st;
    if(!active_add(core,out_operation_id,provider)){atena_store_operation_finish(core->store,out_operation_id,"failed",ATENA_ERR_BUSY);return ATENA_ERR_BUSY;}
    atena_store_audit(core->store,"request.started",request->session_id,out_operation_id,"{}");

    AtenaStreamEvent start={0};start.type=ATENA_EVENT_START;start.operation_id=out_operation_id;start.seq=1;if(callback&&callback(&start,userdata)!=0){st=ATENA_ERR_CANCELLED;goto finish_early;}

    /* Context must fit the effective local runtime. A fixed 16k-character prompt
       can exceed a 2k-token Ollama window before the model gets any room to answer. */
    size_t context_budget = core->context_budget;
    size_t rag_limit = core->rag_results;
    size_t effective_max_output_tokens =
        request->max_output_tokens ? request->max_output_tokens : 256U;
    int effective_use_rag = request->use_rag;
    AtenaResourceSnapshot chat_rs;
    AtenaRuntimePlan chat_rp;
    if (!getenv("ATENA_DISABLE_RUNTIME_TUNING") &&
        atena_runtime_snapshot(&chat_rs) == ATENA_OK &&
        atena_runtime_plan(&chat_rs, &chat_rp) == ATENA_OK) {
        long ctx_tokens = (long)chat_rp.recommended_context_tokens;
        const char *ctx_override = getenv("ATENA_OLLAMA_NUM_CTX");
        if (ctx_override && *ctx_override) {
            long v = strtol(ctx_override, NULL, 10);
            if (v > 0) ctx_tokens = v;
        }
        if (chat_rp.recommended_max_output_tokens > 0 &&
            effective_max_output_tokens > chat_rp.recommended_max_output_tokens) {
            effective_max_output_tokens = chat_rp.recommended_max_output_tokens;
        }
        size_t reserve_tokens = effective_max_output_tokens + 128U;
        size_t input_tokens = ctx_tokens > (long)reserve_tokens
            ? (size_t)ctx_tokens - reserve_tokens
            : 384U;
        size_t adaptive_chars = input_tokens * 3U; /* conservative UTF-8/token estimate */
        if (adaptive_chars >= 1024U && adaptive_chars < context_budget)
            context_budget = adaptive_chars;
        if (chat_rp.rag_level == 0) effective_use_rag = 0;
        else if ((size_t)chat_rp.rag_level < rag_limit) rag_limit = (size_t)chat_rp.rag_level;
    }
    AtenaContextConfig cc={core->identity_dir,context_budget,rag_limit};AtenaBuiltContext ctx={0};
    uint64_t retrieval_start=atena_now_monotonic_ms();st=atena_context_build(core->store,&cc,request->session_id,request->user_text,effective_use_rag,&ctx);uint64_t retrieval_end=atena_now_monotonic_ms();if(st!=ATENA_OK)goto finish_early;
    char user_msg_id[37];st=persist_message(core,request->session_id,ATENA_ROLE_USER,ATENA_MSG_COMPLETE,request->user_text,user_msg_id);if(st!=ATENA_OK){atena_context_free(&ctx);goto finish_early;}
    if(ctx.rag_count)atena_store_audit(core->store,"rag.retrieved",request->session_id,out_operation_id,"{\"source\":\"fts5\"}");

    StreamCollector col={0};col.core=core;col.operation_id=out_operation_id;col.downstream=callback;col.downstream_ud=userdata;col.start_ms=atena_now_monotonic_ms();col.seq=1;col.metrics.retrieval_ms=(double)(retrieval_end-retrieval_start);
    AtenaProviderRequest preq={out_operation_id,request->model&&*request->model?request->model:provider->model,ctx.messages,ctx.message_count,effective_max_output_tokens,0.7,1.0,request->reasoning};
    st=provider->vtable->generate(provider,&preq,collector_cb,&col);
    int tool_calls=0;
    while(st==ATENA_OK&&col.tool_name[0]&&tool_calls<ATENA_MAX_TOOL_CALLS){
        tool_calls++;atena_store_audit(core->store,"tool.proposed",request->session_id,out_operation_id,col.tool_name);
        uint64_t tool_start=atena_now_monotonic_ms();char*tool_result=NULL;AtenaStatus tst=atena_tool_execute(col.tool_name,col.tool_json?col.tool_json:"{}",&tool_result);uint64_t tool_end=atena_now_monotonic_ms();col.metrics.tool_ms+=(double)(tool_end-tool_start);
        if(tst!=ATENA_OK){atena_store_audit(core->store,"tool.denied",request->session_id,out_operation_id,col.tool_name);st=tst;free(tool_result);break;}
        atena_store_audit(core->store,"tool.executed",request->session_id,out_operation_id,col.tool_name);
        persist_message(core,request->session_id,ATENA_ROLE_TOOL,ATENA_MSG_COMPLETE,tool_result,NULL);
        AtenaStreamEvent tr={0};tr.type=ATENA_EVENT_TOOL_RESULT;tr.operation_id=out_operation_id;tr.seq=++col.seq;tr.tool_name=col.tool_name;tr.tool_json=tool_result;if(callback&&callback(&tr,userdata)!=0){free(tool_result);st=ATENA_ERR_CANCELLED;break;}
        AtenaMessage*cont=NULL;size_t cont_count=0;AtenaStatus cst=messages_copy_with_tool(&ctx,request->session_id,tool_result,&cont,&cont_count);free(tool_result);if(cst!=ATENA_OK){st=cst;break;}
        free(col.text);col.text=NULL;col.text_len=col.text_cap=0;col.tool_name[0]='\0';free(col.tool_json);col.tool_json=NULL;
        preq.messages=cont;preq.message_count=cont_count;st=provider->vtable->generate(provider,&preq,collector_cb,&col);free_messages(cont,cont_count);
    }

    if(st==ATENA_OK&&tool_calls>=ATENA_MAX_TOOL_CALLS&&col.tool_name[0])st=ATENA_ERR_POLICY_DENIED;
    if(st==ATENA_OK && (!col.text || !*col.text)) {
        /* HTTP 200 with no assistant text is not a successful chat turn. */
        st = ATENA_ERR_PROVIDER_INVALID;
    }
    if(st==ATENA_OK){
        char assistant_id[37];
        AtenaStatus pst=persist_message(core,request->session_id,ATENA_ROLE_ASSISTANT,ATENA_MSG_COMPLETE,col.text?col.text:"",assistant_id);
        if(pst!=ATENA_OK) st=pst;
    } else if(col.text&&*col.text){persist_message(core,request->session_id,ATENA_ROLE_ASSISTANT,st==ATENA_ERR_CANCELLED?ATENA_MSG_CANCELLED:ATENA_MSG_PARTIAL,col.text,NULL);}
    col.metrics.total_generation_ms=(double)(atena_now_monotonic_ms()-col.start_ms);if(col.first_delta_ms)col.metrics.time_to_first_token_ms=(double)(col.first_delta_ms-col.start_ms);
    if(st==ATENA_OK){
        for(size_t i=0;i<ctx.rag_count;i++){if(col.text&&strstr(col.text,ctx.rag_hits[i].content)){AtenaStreamEvent ce={0};ce.type=ATENA_EVENT_CITATION;ce.operation_id=out_operation_id;ce.seq=++col.seq;ce.citation_id=ctx.rag_hits[i].chunk_id;ce.citation_document_id=ctx.rag_hits[i].document_id;ce.citation_title=ctx.rag_hits[i].title;ce.citation_locator=ctx.rag_hits[i].locator;ce.citation_content_sha256=ctx.rag_hits[i].content_sha256;if(callback)callback(&ce,userdata);}}
        AtenaStreamEvent done={0};done.type=ATENA_EVENT_DONE;done.operation_id=out_operation_id;done.seq=++col.seq;done.metrics=col.metrics;if(callback)callback(&done,userdata);
        atena_store_operation_finish(core->store,out_operation_id,"completed",ATENA_OK);
    } else {
        AtenaStreamEvent err={0};err.type=ATENA_EVENT_ERROR;err.operation_id=out_operation_id;err.seq=++col.seq;err.error_code=st;err.text=atena_status_string(st);if(callback)callback(&err,userdata);
        atena_store_operation_finish(core->store,out_operation_id,st==ATENA_ERR_CANCELLED?"cancelled":"failed",st);atena_store_audit(core->store,st==ATENA_ERR_CANCELLED?"request.cancelled":"request.failed",request->session_id,out_operation_id,atena_status_string(st));
    }
    free(col.text);free(col.tool_json);atena_context_free(&ctx);active_remove(core,out_operation_id);return st;

finish_early:
    atena_store_operation_finish(core->store,out_operation_id,st==ATENA_ERR_CANCELLED?"cancelled":"failed",st);active_remove(core,out_operation_id);return st;
}

AtenaStatus atena_core_chat_cancel(AtenaCore*core,const char*operation_id){if(!core||!operation_id)return ATENA_ERR_INVALID_ARGUMENT;AtenaProvider*p=NULL;atena_mutex_lock(&core->lock);for(size_t i=0;i<ATENA_MAX_ACTIVE_OPS;i++)if(strcmp(core->active[i].id,operation_id)==0){core->active[i].cancelled=1;p=core->active[i].provider;break;}atena_mutex_unlock(&core->lock);if(!p)return ATENA_ERR_NOT_FOUND;if(p->vtable->cancel)p->vtable->cancel(p,operation_id);return ATENA_OK;}

AtenaStatus atena_core_status_json(AtenaCore *core, char **out_json) {
    if (!core || !out_json) return ATENA_ERR_INVALID_ARGUMENT;
    *out_json = NULL;
    json_object *o = json_object_new_object();
    if (!o) return ATENA_ERR_NO_MEMORY;
    json_object_object_add(o, "name", json_object_new_string("Atena Core"));
    json_object_object_add(o, "version", json_object_new_string("0.5.0-base"));
    json_object_object_add(o, "ipc", json_object_new_string("atena.ipc/2"));
    json_object_object_add(o, "offline_mode", json_object_new_boolean(core->offline_mode));
    json_object_object_add(o, "provider_count", json_object_new_int64((int64_t)core->provider_count));

    /* Runtime truth lives in memory. Persistence is only a cache/history layer. */
    json_object *providers = json_object_new_array();
    if (providers) {
        atena_mutex_lock(&core->lock);
        for (size_t i = 0; i < core->provider_count; i++) {
            AtenaProvider *p = core->providers[i];
            if (!p) continue;
            json_object *item = json_object_new_object();
            if (!item) continue;
            json_object_object_add(item, "id", json_object_new_string(p->id));
            json_object_object_add(item, "type", json_object_new_string(p->type));
            json_object_object_add(item, "model", json_object_new_string(p->model));
            json_object_object_add(item, "capabilities_supported", json_object_new_int64((int64_t)p->capabilities));
            json_object_object_add(item, "capabilities_known", json_object_new_int64((int64_t)p->capabilities_known));
            json_object_object_add(item, "configured", json_object_new_boolean(1));
            json_object_object_add(item, "selected", json_object_new_boolean(!strcmp(core->selected_provider,p->id)));
            json_object_array_add(providers,item);
        }
        atena_mutex_unlock(&core->lock);
        json_object_object_add(o, "providers", providers);
    }

    AtenaResourceSnapshot rs;
    AtenaRuntimePlan rp;
    if (atena_runtime_snapshot(&rs) == ATENA_OK && atena_runtime_plan(&rs, &rp) == ATENA_OK) {
        json_object *runtime = json_object_new_object();
        json_object *memory = json_object_new_object();
        json_object *cpu = json_object_new_object();
        json_object *plan = json_object_new_object();

        json_object_object_add(memory, "total_bytes", json_object_new_int64((int64_t)rs.ram_total_bytes));
        json_object_object_add(memory, "available_bytes", json_object_new_int64((int64_t)rs.ram_available_bytes));
        json_object_object_add(memory, "free_bytes", json_object_new_int64((int64_t)rs.ram_free_bytes));
        json_object_object_add(memory, "cached_bytes", json_object_new_int64((int64_t)rs.ram_cached_bytes));
        json_object_object_add(memory, "swap_total_bytes", json_object_new_int64((int64_t)rs.swap_total_bytes));
        json_object_object_add(memory, "swap_free_bytes", json_object_new_int64((int64_t)rs.swap_free_bytes));
        json_object_object_add(memory, "pressure", json_object_new_string(atena_memory_pressure_string(rs.memory_pressure)));

        json_object_object_add(cpu, "architecture", json_object_new_string(rs.architecture));
        json_object_object_add(cpu, "logical_cpus", json_object_new_int((int)rs.logical_cpus));
        json_object_object_add(cpu, "load_1m", json_object_new_double(rs.load_1m));
        json_object_object_add(cpu, "load_5m", json_object_new_double(rs.load_5m));
        json_object_object_add(cpu, "load_15m", json_object_new_double(rs.load_15m));

        json_object_object_add(plan, "profile", json_object_new_string(rp.profile));
        json_object_object_add(plan, "threads", json_object_new_int((int)rp.recommended_threads));
        json_object_object_add(plan, "context_tokens", json_object_new_int((int)rp.recommended_context_tokens));
        json_object_object_add(plan, "max_output_tokens", json_object_new_int((int)rp.recommended_max_output_tokens));
        json_object_object_add(plan, "batch_tokens", json_object_new_int((int)rp.recommended_batch_tokens));
        long effective_threads = (long)rp.recommended_threads;
        long effective_context = (long)rp.recommended_context_tokens;
        long effective_batch = (long)rp.recommended_batch_tokens;
        const char *thread_override = getenv("ATENA_OLLAMA_NUM_THREAD");
        const char *context_override = getenv("ATENA_OLLAMA_NUM_CTX");
        const char *batch_override = getenv("ATENA_OLLAMA_NUM_BATCH");
        if (thread_override && *thread_override) { long v=strtol(thread_override,NULL,10); if(v>0) effective_threads=v; }
        if (context_override && *context_override) { long v=strtol(context_override,NULL,10); if(v>0) effective_context=v; }
        if (batch_override && *batch_override) { long v=strtol(batch_override,NULL,10); if(v>0) effective_batch=v; }
        json_object_object_add(plan, "effective_threads", json_object_new_int64((int64_t)effective_threads));
        json_object_object_add(plan, "effective_context_tokens", json_object_new_int64((int64_t)effective_context));
        json_object_object_add(plan, "effective_batch_tokens", json_object_new_int64((int64_t)effective_batch));
        json_object_object_add(plan, "override_active", json_object_new_boolean(
            (thread_override&&*thread_override)||(context_override&&*context_override)||(batch_override&&*batch_override)));
        json_object_object_add(plan, "keep_alive_seconds", json_object_new_int((int)rp.recommended_keep_alive_seconds));
        json_object_object_add(plan, "rag_level", json_object_new_int((int)rp.rag_level));
        json_object_object_add(plan, "keep_model_resident", json_object_new_boolean(rp.keep_model_resident));
        json_object_object_add(plan, "allow_python_worker", json_object_new_boolean(rp.allow_python_worker));

        json_object_object_add(runtime, "platform", json_object_new_string(rs.platform));
        json_object_object_add(runtime, "memory", memory);
        json_object_object_add(runtime, "cpu", cpu);
        json_object_object_add(runtime, "plan", plan);
        json_object_object_add(o, "runtime", runtime);
    }

    *out_json = atena_strdup(json_object_to_json_string_ext(o, JSON_C_TO_STRING_PLAIN));
    json_object_put(o);
    return *out_json ? ATENA_OK : ATENA_ERR_NO_MEMORY;
}
void atena_core_free_string(char*value){free(value);}
