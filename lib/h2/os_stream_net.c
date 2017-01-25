#include <h2/os_stream_net.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>


static int server_connection_open(stream_net_cfg* cfg)
{
    int ret;
    int listen_fd;
    struct sockaddr_in addr;

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        ret = errno;
        goto out_ret;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(cfg->endp.server.listen_endp.port);

    ret = bind(listen_fd, (struct sockaddr*) &addr, sizeof(addr));
    if (ret) {
        ret = errno;
        goto out_close;
    }

    ret = listen(listen_fd, 10);
    if (ret) {
        ret = errno;
        goto out_close;
    }

    cfg->endp.server.listen_fd = listen_fd;

    return 0;

out_close:
    close(listen_fd);

out_ret:
    return ret;
}

static void server_connection_close(stream_net_cfg* cfg)
{
    close(cfg->endp.server.listen_fd);
}

static int server_connection_wait(int listen_fd, int* conn_fd)
{
    int ret;

    ret = 0;

    *conn_fd = accept(listen_fd, (struct sockaddr*) NULL, NULL);
    if (*conn_fd < 0) {
        ret = errno;
    }

    return ret;
}

static int client_stream_open(tcp_endpoint* server, int* fd)
{
    int ret;
    struct sockaddr_in serv_addr;

    *fd = socket(AF_INET, SOCK_STREAM, 0);
    if (*fd < 0) {
        goto out_err;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server->port);
    serv_addr.sin_addr.s_addr = server->ip.s_addr;

    ret = connect(*fd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
    if (ret) {
        ret = errno;
        goto out_close;
    }

    return 0;

out_close:
    close(*fd);
    *fd = -1;

out_err:
    return ret;
}

int stream_net_init(stream_net_cfg* cfg)
{
    int ret;

    if (cfg == NULL) {
        ret = EINVAL;
        goto out_ret;
    }

    ret = 0;

    switch (cfg->mode) {
        case stream_net_server:
            ret = server_connection_open(cfg);
            break;
        case stream_net_client:
            break;
        default:
            ret = EINVAL;
            break;
    }

out_ret:
    return ret;
}

int stream_net_destroy(stream_net_cfg* cfg)
{
    int ret;

    if (cfg == NULL) {
        ret = EINVAL;
        goto out_ret;
    }

    ret = 0;

    switch (cfg->mode) {
        case stream_net_server:
            server_connection_close(cfg);
            break;
        case stream_net_client:
            break;
        default:
            ret = EINVAL;
            break;
    }

out_ret:
    return ret;
}

int stream_net_open(stream_net_cfg* cfg, int* fd)
{
    int ret;

    if (cfg == NULL || fd == NULL) {
        ret = EINVAL;
        goto out_ret;
    }

    ret = 0;

    switch (cfg->mode) {
        case stream_net_server:
            ret = server_connection_wait(cfg->endp.server.listen_fd, fd);
            break;
        case stream_net_client:
            ret = client_stream_open(&cfg->endp.client.server_endp, fd);
            break;
        default:
            ret = EINVAL;
            break;
    }

out_ret:
    return ret;
}

int stream_net_close(int fd)
{
    int ret;

    ret = close(fd);
    if (ret) {
        ret = errno;
    }

    return ret;
}

int stream_net_read(int fd, void* buffer, size_t size, int* out_read)
{
    int ret;
    int bytes;

    if (fd < 0 || buffer == NULL || out_read == NULL) {
        ret = EINVAL;
        goto out_ret;
    }

    bytes = read(fd, buffer, size);
    if (bytes < 0) {
        ret = errno;
    } else {
        *out_read = bytes;
        ret = 0;
    }

out_ret:
    return ret;
}

int stream_net_write(int fd, void* buffer, size_t size, int* out_written)
{
    int ret;
    int bytes;

    if (fd < 0 || buffer == NULL || out_written == NULL) {
        ret = EINVAL;
        goto out_ret;
    }

    bytes = write(fd, buffer, size);
    if (bytes < 0) {
        ret = errno;
    } else {
        *out_written = bytes;
        ret = 0;
    }

out_ret:
    return ret;
}

int stream_net_size(int fd, size_t* size)
{
    return 0;
}
