#include "atena/platform.h"

#ifndef _WIN32
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

AtenaStatus atena_transport_connect(const char *endpoint, int timeout_ms, AtenaTransport *out) {
    if (!endpoint || !out || strlen(endpoint) >= sizeof(((struct sockaddr_un*)0)->sun_path))
        return ATENA_ERR_INVALID_ARGUMENT;
    out->fd = -1;
    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (fd < 0) return ATENA_ERR_IO;
    struct sockaddr_un address;
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    snprintf(address.sun_path, sizeof(address.sun_path), "%s", endpoint);
    int rc = connect(fd, (struct sockaddr*)&address, sizeof(address));
    if (rc != 0 && errno != EINPROGRESS) { close(fd); return ATENA_ERR_NETWORK; }
    if (rc != 0) {
        struct pollfd pollfd = {fd, POLLOUT, 0};
        rc = poll(&pollfd, 1, timeout_ms > 0 ? timeout_ms : 2000);
        if (rc <= 0) { close(fd); return rc == 0 ? ATENA_ERR_TIMEOUT : ATENA_ERR_NETWORK; }
        int error = 0; socklen_t length = sizeof(error);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length) != 0 || error != 0) {
            close(fd); return ATENA_ERR_NETWORK;
        }
    }
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) (void)fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
    out->fd = fd;
    return ATENA_OK;
}

void atena_transport_close(AtenaTransport *transport) {
    if (transport && transport->fd >= 0) { close(transport->fd); transport->fd = -1; }
}

AtenaNativeHandle atena_transport_native_handle(const AtenaTransport *transport) {
    return transport ? transport->fd : -1;
}
#endif
