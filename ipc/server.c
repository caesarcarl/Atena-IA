#ifndef _WIN32
#include "atena/ipc.h"
#include "protocol.h"
#include "server_dispatch.h"

#include <json-c/json.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

struct AtenaIpcServer {
    AtenaCore *core;
    int listen_fd;
    char endpoint[sizeof(((struct sockaddr_un*)0)->sun_path)];
    pthread_t accept_thread;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    size_t active_clients;
    int stopping;
};

typedef struct ClientCtx { AtenaIpcServer *server; int fd; } ClientCtx;

static void *client_thread(void *arg) {
    ClientCtx *c = (ClientCtx *)arg;
    char *raw = NULL;
    AtenaStatus s = atena_ipc_read_frame(c->fd, &raw, 4U * 1024U * 1024U);
    if (s == ATENA_OK) {
        json_object *r = json_tokener_parse(raw);
        free(raw);
        if (r && json_object_is_type(r, json_type_object))
            (void)atena_ipc_dispatch_request(c->server->core, c->fd, r);
        if (r) json_object_put(r);
    }
    shutdown(c->fd, SHUT_RDWR);
    close(c->fd);
    AtenaIpcServer *server = c->server;
    free(c);
    pthread_mutex_lock(&server->lock);
    if (server->active_clients) server->active_clients--;
    pthread_cond_broadcast(&server->cond);
    pthread_mutex_unlock(&server->lock);
    return NULL;
}

static void *accept_thread(void *arg) {
    AtenaIpcServer *s = (AtenaIpcServer *)arg;
    for (;;) {
        int fd = accept(s->listen_fd, NULL, NULL);
        if (fd < 0) {
            pthread_mutex_lock(&s->lock); int stop = s->stopping; pthread_mutex_unlock(&s->lock);
            if (stop) break;
            continue;
        }
        ClientCtx *c = (ClientCtx *)calloc(1, sizeof(*c));
        if (!c) { close(fd); continue; }
        c->server = s; c->fd = fd;
        pthread_mutex_lock(&s->lock); s->active_clients++; pthread_mutex_unlock(&s->lock);
        pthread_t t;
        if (pthread_create(&t, NULL, client_thread, c) == 0) pthread_detach(t);
        else { close(fd); free(c); pthread_mutex_lock(&s->lock); s->active_clients--; pthread_mutex_unlock(&s->lock); }
    }
    return NULL;
}

AtenaStatus atena_ipc_server_start(AtenaCore *core, const char *endpoint, AtenaIpcServer **out) {
    if (!core || !endpoint || !out) return ATENA_ERR_INVALID_ARGUMENT;
    *out = NULL;
    AtenaIpcServer *s = (AtenaIpcServer *)calloc(1, sizeof(*s));
    if (!s) return ATENA_ERR_NO_MEMORY;
    if (strlen(endpoint) >= sizeof(s->endpoint)) { free(s); return ATENA_ERR_INVALID_ARGUMENT; }
    s->core = core; s->listen_fd = -1; snprintf(s->endpoint, sizeof(s->endpoint), "%s", endpoint);
    pthread_mutex_init(&s->lock, NULL); pthread_cond_init(&s->cond, NULL);
    s->listen_fd = socket(AF_UNIX, SOCK_STREAM, 0); if (s->listen_fd < 0) goto fail;
    unlink(endpoint);
    struct sockaddr_un a; memset(&a, 0, sizeof(a)); a.sun_family = AF_UNIX; snprintf(a.sun_path, sizeof(a.sun_path), "%s", endpoint);
    if (bind(s->listen_fd, (struct sockaddr *)&a, sizeof(a)) != 0 || chmod(endpoint, 0600) != 0 || listen(s->listen_fd, 16) != 0) goto fail;
    if (pthread_create(&s->accept_thread, NULL, accept_thread, s) != 0) goto fail;
    *out = s; return ATENA_OK;
fail:
    if (s->listen_fd >= 0) close(s->listen_fd);
    unlink(endpoint); pthread_cond_destroy(&s->cond); pthread_mutex_destroy(&s->lock); free(s); return ATENA_ERR_IO;
}

void atena_ipc_server_stop(AtenaIpcServer *s) {
    if (!s) return;
    pthread_mutex_lock(&s->lock); s->stopping = 1; pthread_mutex_unlock(&s->lock);
    shutdown(s->listen_fd, SHUT_RDWR); close(s->listen_fd); pthread_join(s->accept_thread, NULL);
    pthread_mutex_lock(&s->lock); while (s->active_clients) pthread_cond_wait(&s->cond, &s->lock); pthread_mutex_unlock(&s->lock);
    unlink(s->endpoint); pthread_cond_destroy(&s->cond); pthread_mutex_destroy(&s->lock); free(s);
}
#endif
