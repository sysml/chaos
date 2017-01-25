#include <h2/stream.h>

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>


int stream_init(stream_desc* sd)
{
    int ret;

    if (sd == NULL) {
        ret = EINVAL;
        goto out_ret;
    }

    switch (sd->type) {
        case stream_type_file:
            ret = stream_file_init(&sd->file);
            break;
        case stream_type_net:
            ret = stream_net_init(&sd->net);
            break;
        default:
            ret = EINVAL;
            break;
    }
    if (ret) {
        goto out_ret;
    }

    sd->fd = -1;
    sd->bytes = 0;

out_ret:
    return ret;
}

int stream_destroy(stream_desc* sd)
{
    int ret;

    if (sd == NULL) {
        ret = EINVAL;
        goto out_ret;
    }

    ret = 0;

    switch (sd->type) {
        case stream_type_file:
            break;
        case stream_type_net:
            stream_net_destroy(&sd->net);
            break;
        default:
            ret = EINVAL;
            break;
    }
    if (ret) {
        goto out_ret;
    }

out_ret:
    return ret;
}

int stream_open(stream_desc* sd)
{
    int ret;
    int fd;

    if (sd == NULL) {
        ret = EINVAL;
        goto out_ret;
    }

    switch (sd->type) {
        case stream_type_file:
            ret = stream_file_open(&sd->file, &fd);
            break;
        case stream_type_net:
            ret = stream_net_open(&sd->net, &fd);
            break;
        default:
            ret = EINVAL;
            break;
    }
    if (ret) {
        goto out_ret;
    }

    sd->fd = fd;
    sd->bytes = 0;

out_ret:
    return ret;
}

int stream_close(stream_desc *sd)
{
    int ret;

    if (sd == NULL) {
        ret = EINVAL;
        goto out_ret;
    }

    switch (sd->type) {
        case stream_type_file:
            ret = stream_file_close(sd->fd);
            break;
        case stream_type_net:
            ret = stream_net_close(sd->fd);
            break;
        default:
            ret = EINVAL;
            break;
    }

out_ret:
    return ret;
}

int stream_read(stream_desc* sd, void* buffer, size_t size)
{
    int ret;
    int bytes;

    if (sd == NULL || buffer == NULL) {
        ret = EINVAL;
        goto out_ret;
    }

    switch (sd->type) {
        case stream_type_file:
            ret = stream_file_read(sd->fd, buffer, size, &bytes);
            break;
        case stream_type_net:
            ret = stream_net_read(sd->fd, buffer, size, &bytes);
            break;
        default:
            ret = EINVAL;
            break;
    }
    if (ret) {
        goto out_ret;
    }

    sd->bytes += bytes;

out_ret:
    return ret;
}

int stream_write(stream_desc* sd, void* buffer, size_t size)
{
    int ret;
    int bytes;

    if (sd == NULL || buffer == NULL) {
        ret = EINVAL;
        goto out_ret;
    }

    switch (sd->type) {
        case stream_type_file:
            ret = stream_file_write(sd->fd, buffer, size, &bytes);
            break;
        case stream_type_net:
            ret = stream_net_write(sd->fd, buffer, size, &bytes);
            break;
        default:
            ret = EINVAL;
            break;
    }
    if (ret) {
        goto out_ret;
    }

    sd->bytes += bytes;

out_ret:
    return ret;
}

int stream_align(stream_desc* sd, size_t align)
{
    int ret;
    int not_aligned, extra;

    if (sd == NULL) {
        ret = EINVAL;
        goto out_ret;
    }

    ret = 0;

    switch (sd->type) {
        case stream_type_file:
            not_aligned = sd->bytes % align;
            if (not_aligned) {
                extra = align - not_aligned;
                ret = stream_file_move(sd->fd, extra);
                if (ret) {
                    goto out_ret;
                }
                sd->bytes += extra;
            }
            break;
        case stream_type_net:
            break;
        default:
            ret = EINVAL;
            break;
    }

out_ret:
    return ret;
}

int stream_size(stream_desc* sd, size_t* size)
{
    int ret;

    if (sd == NULL || size == NULL) {
        ret = EINVAL;
        goto out_ret;
    }

    switch (sd->type) {
        case stream_type_file:
            ret = stream_file_size(sd->fd, size);
            break;
        case stream_type_net:
            ret = stream_net_size(sd->fd, size);
            break;
        default:
            ret = EINVAL;
            break;
    }

out_ret:
    return ret;
}
