#include <h2/guest_ctrl.h>
#include <h2/stream.h>
#include <h2/config.h>


#define CHAOS_IMG_MAGIC 0xDEADBEEF

struct save_file_header {
    uint32_t magic;
    uint32_t flags;
    uint32_t optional_data_len;
};

int h2_serialized_cfg_alloc(h2_serialized_cfg* cfg, size_t size)
{
    int ret;

    cfg->data = malloc(size * sizeof(char));
    if (cfg->data == NULL) {
        ret = -ENOMEM;
        goto out;
    }

    cfg->size = size;

    ret = 0;

out:
    return ret;
}

void h2_serialized_cfg_free(h2_serialized_cfg* cfg)
{
    if (cfg->data) {
        free(cfg->data);
        cfg->data = NULL;
    }
    cfg->size = 0;
}

static int save_header(h2_guest_ctrl_save* gs)
{
    int ret;

    struct save_file_header hdr;

    hdr.magic = CHAOS_IMG_MAGIC;
    hdr.flags = 0;
    hdr.optional_data_len = gs->serialized_cfg.size;

    ret = stream_write(&gs->sd, &hdr, sizeof(hdr));
    if (ret < 0) {
        goto out_ret;
    }

    ret = 0;

out_ret:
    return ret;
}

static int restore_header(h2_guest_ctrl_create* gc)
{
    int ret;

    struct save_file_header hdr;

    ret = stream_read(&gc->sd, &hdr, sizeof(hdr));
    if (ret < 0) {
        goto out_ret;
    }

    if (hdr.magic != CHAOS_IMG_MAGIC) {
        goto out_ret;
    }

    ret = h2_serialized_cfg_alloc(&gc->serialized_cfg, hdr.optional_data_len);

out_ret:
    return ret;
}

static int create_read_config(struct h2_guest_ctrl_create* gc)
{
    int ret;
    size_t cfg_size;

    ret = stream_size(&gc->sd, &cfg_size);
    if (ret) {
        goto out_ret;
    }

    ret = h2_serialized_cfg_alloc(&gc->serialized_cfg, cfg_size);
    if (ret) {
        goto out_ret;
    }

    ret = config_read(&gc->serialized_cfg, &gc->sd);

out_ret:
    return ret;
}

static int save_config(h2_guest_ctrl_save* gs)
{
    int ret;

    ret = config_write(&gs->serialized_cfg, &gs->sd);
    if (ret) {
        goto out_ret;
    }

    ret = stream_align(&gs->sd, 64);

out_ret:
    return ret;
}

static int restore_config(h2_guest_ctrl_create* gc)
{
    int ret;

    ret = config_read(&gc->serialized_cfg, &gc->sd);
    if (ret) {
        goto out_ret;
    }

    ret = stream_align(&gc->sd, 64);

out_ret:
    return ret;
}

int h2_guest_ctrl_create_init(h2_guest_ctrl_create* gc, bool restore)
{
    int ret;

    ret = stream_init(&gc->sd);
    if (ret) {
        goto out_ret;
    }

    ret = stream_open(&gc->sd);
    if (ret) {
        goto out_ret;
    }

    if (restore) {
        gc->cb_read_header = restore_header;
        gc->cb_read_config = restore_config;
        gc->cb_do_config = config_parse;
        gc->restore = true;

    } else {
        gc->cb_read_header = NULL;
        gc->cb_read_config = create_read_config;
        gc->cb_do_config = config_parse;
        gc->restore = false;
    }

out_ret:
    return ret;
}

void h2_guest_ctrl_create_destroy(h2_guest_ctrl_create* gc)
{
    stream_close(&gc->sd);
}

int h2_guest_ctrl_save_init(h2_guest_ctrl_save* gs)
{
    int ret;

    ret = stream_init(&gs->sd);
    if (ret) {
        goto out_ret;
    }

    ret = stream_open(&gs->sd);
    if (ret) {
        goto out_ret;
    }

    gs->cb_do_config = config_dump;
    gs->cb_write_header = save_header;
    gs->cb_write_config = save_config;

out_ret:
    return ret;
}

void h2_guest_ctrl_save_destroy(h2_guest_ctrl_save* gs)
{
    stream_close(&gs->sd);
}
