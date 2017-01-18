#ifndef __H2__STREAM__H__
#define __H2__STREAM__H__

#include <h2/os_stream_file.h>
#include <h2/os_stream_net.h>

#include <stdbool.h>


enum stream_type {
    stream_type_none,
    stream_type_file,
    stream_type_net,
};
typedef enum stream_type stream_type;

struct stream_desc {
    stream_type type;

    union {
        stream_file_cfg file;
        stream_net_cfg net;
    };

    int fd;
    size_t bytes;
};
typedef struct stream_desc stream_desc;


int stream_init(stream_desc* sd);
int stream_open(stream_desc* sd);
int stream_close(stream_desc* sd);

int stream_read(stream_desc* sd, void* buffer, size_t size);
int stream_write(stream_desc* sd, void* buffer, size_t size);

int stream_align(stream_desc* sd, size_t align);

int stream_size(stream_desc* sd, size_t* size);

#endif /* __H2__STREAM__H__ */
