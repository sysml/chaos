#include <h2/stream.h>

#include <stdlib.h>
#include <unistd.h>


int stream_init(stream_desc* sd, stream_cfg* cfg)
{
    int ret;

    if (cfg->type == stream_type_file)
        ret = stream_file_init(&cfg->file);

    else if (cfg->type == stream_type_net)
        ret = stream_net_init(&cfg->net);

    if (ret != 0)
        goto out_ret;

    sd->cfg = cfg;
    sd->fd = -1;
    sd->bytes = 0;

out_ret:
    return ret;
}

bool stream_is_initialized(stream_desc* sd)
{
    return (sd->cfg != NULL);
}

int stream_open(stream_desc *sd)
{
    int fd;

    stream_cfg* cfg = sd->cfg;

    if (cfg->type == stream_type_file)
        fd = stream_file_open(&cfg->file);

    else if (cfg->type == stream_type_net)
        fd = stream_net_open(&cfg->net);

    if (fd < 0)
        return -1;

    sd->cfg = cfg;
    sd->fd = fd;
    sd->bytes = 0;

    return 0;
}

void stream_close(struct stream_desc *sd)
{
    if (sd->fd >= 0)
        close(sd->fd);
}

int stream_read(struct stream_desc *sd, void *buffer, size_t size)
{
    int bytes;

    bytes = read(sd->fd, buffer, size);
    if (bytes > 0)
        sd->bytes += bytes;

    return bytes;
}

int stream_write(struct stream_desc *sd, void *buffer, size_t size)
{
    int bytes;

    bytes = write(sd->fd, buffer, size);
    if (bytes > 0)
        sd->bytes += bytes;

    return bytes;
}

int stream_align(stream_desc *sd, size_t align)
{
    int not_aligned, extra;

    not_aligned = sd->bytes % align;
    if (not_aligned) {
        extra = align - not_aligned;
        stream_file_move(sd->fd, extra);
        sd->bytes += extra;
    }

    return 0;
}

size_t stream_size(stream_desc *sd)
{
    if (sd->cfg->type == stream_type_file)
        return stream_file_size(sd->fd);
    else
        return 0;
}
