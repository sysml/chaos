#include <h2/os_stream_file.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


int stream_file_init(stream_file_cfg* cfg)
{
    return 0;
}

int stream_file_open(stream_file_cfg* cfg)
{
    int flags;

    if (cfg->op == stream_file_op_read)
        flags = O_RDONLY;

    else if (cfg->op == stream_file_op_write)
        flags = O_WRONLY | O_CREAT | O_TRUNC;

    else
        return -1;

    return open(cfg->filename, flags, 0644);
}

int stream_file_close(int fd)
{
    return close(fd);
}

size_t stream_file_move(int fd, int bytes)
{
    return lseek(fd, bytes, SEEK_CUR);
}

size_t stream_file_size(int fd)
{
    struct stat stats;

    fstat(fd, &stats);

    return stats.st_size;
}
