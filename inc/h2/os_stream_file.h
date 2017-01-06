#ifndef __H2__OS_STREAM_FILE__H__
#define __H2__OS_STREAM_FILE__H__

#include <stddef.h>


enum stream_file_op {
    stream_file_op_none,
    stream_file_op_read,
    stream_file_op_write,
};
typedef enum stream_file_op stream_file_op;

struct stream_file_cfg {
    stream_file_op op;
    const char* filename;
};
typedef struct stream_file_cfg stream_file_cfg;


int stream_file_init(stream_file_cfg* cfg);
int stream_file_open(stream_file_cfg* cfg);
int stream_file_close(int fd);

size_t stream_file_move(int fd, int bytes);
size_t stream_file_size(int fd);

#endif /* __H2__OS_STREAM_FILE__H__ */
