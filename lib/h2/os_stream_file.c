#include <h2/os_stream_file.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>


int stream_file_init(stream_file_cfg* cfg)
{
    return 0;
}

int stream_file_open(stream_file_cfg* cfg, int* fd)
{
    int ret;
    int flags;

    ret = 0;

    switch (cfg->op) {
        case stream_file_op_read:
            flags = O_RDONLY;
            break;
        case stream_file_op_write:
            flags = O_WRONLY | O_CREAT | O_TRUNC;
            break;
        default:
            ret = EINVAL;
            goto out_ret;
            break;
    }

    *fd = open(cfg->filename, flags, 0644);
    if (*fd < 0) {
        ret = errno;
    }

out_ret:
    return ret;
}

int stream_file_close(int fd)
{
    int ret;

    ret = close(fd);
    if (ret) {
        ret = errno;
    }

    return ret;
}

int stream_file_read(int fd, void* buffer, size_t size, int* out_read)
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

int stream_file_write(int fd, void* buffer, size_t size, int* out_written)
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

int stream_file_move(int fd, int bytes)
{
    int ret;
    int here, there;

    here = lseek(fd, 0, SEEK_CUR);
    if (here < 0) {
        ret = errno;
        goto out_ret;
    }

    there = lseek(fd, bytes, SEEK_CUR);
    if (there < 0) {
        ret = errno;
        goto out_ret;
    }

    ret = !((there - here) == bytes);

out_ret:
    return ret;
}

int stream_file_size(int fd, size_t* size)
{
    int ret;
    struct stat stats;

    ret = fstat(fd, &stats);
    if (ret) {
        ret = errno;
        goto out_ret;
    }

    *size = stats.st_size;

out_ret:
    return ret;
}
