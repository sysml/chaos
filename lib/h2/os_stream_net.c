#include <h2/os_stream_net.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>


static int server_connection_open(stream_net_cfg* cfg)
{
    int ret;
    struct sockaddr_in addr;

    cfg->endp.server.listen_fd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(cfg->endp.server.listen_endp.port);

    ret = bind(cfg->endp.server.listen_fd, (struct sockaddr*) &addr, sizeof(addr));
    if (ret) {
        goto out_err;
    }

    ret = listen(cfg->endp.server.listen_fd, 10);
    if (ret) {
        goto out_err;
    }

    return 0;

out_err:
    close(cfg->endp.server.listen_fd);

    return ret;
}

static int server_connection_wait(int listen_fd)
{
    return accept(listen_fd, (struct sockaddr*) NULL, NULL);
}

static int client_stream_open(tcp_endpoint* server)
{
    int ret;
    int fd;
    struct sockaddr_in serv_addr;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        goto out_err;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server->port);
    serv_addr.sin_addr.s_addr = server->ip.s_addr;

    ret = connect(fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    if (ret) {
        goto out_err;
    }

    return fd;

out_err:
    close(fd);

    return -1;
}

int stream_net_init(stream_net_cfg* cfg)
{
    int ret = 0;

    if (cfg->mode == stream_net_server)
        ret = server_connection_open(cfg);

    else if (cfg->mode != stream_net_client)
        ret = -1;

    return ret;
}

int stream_net_open(stream_net_cfg* cfg)
{
    int fd;

    if (cfg->mode == stream_net_server) {
        fd = server_connection_wait(cfg->endp.server.listen_fd);
    }

    else if (cfg->mode == stream_net_client) {
        fd = client_stream_open(&cfg->endp.client.server_endp);
    }

    return fd;
}
