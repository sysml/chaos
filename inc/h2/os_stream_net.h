#ifndef __H2__OS_STREAM_NET_H__
#define __H2__OS_STREAM_NET_H__

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


struct tcp_endpoint {
    int port;
    struct in_addr ip;
};
typedef struct tcp_endpoint tcp_endpoint;


enum stream_net_mode {
    stream_net_none,
    stream_net_server,
    stream_net_client,
};
typedef enum stream_net_mode stream_net_mode;


struct stream_net_cfg {
    stream_net_mode mode;

    union {
        struct {
            tcp_endpoint listen_endp;
            int listen_fd;
        } server;

        struct {
            tcp_endpoint server_endp;
        } client;
    } endp;
};
typedef struct stream_net_cfg stream_net_cfg;

int stream_net_init(stream_net_cfg* cfg);
int stream_net_open(stream_net_cfg* cfg, int* fd);
int stream_net_close(int fd);

int stream_net_read(int fd, void* buffer, size_t size, int* out_read);
int stream_net_write(int fd, void* buffer, size_t size, int* out_written);

int stream_net_size(int fd, size_t* size);

#endif /* __H2__OS_STREAM_NET__H__ */
