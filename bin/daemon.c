#include <daemon/cmdline.h>
#include <h2/stream.h>


int main(int argc, char** argv)
{
    int ret;

    cmdline cmd;

    h2_ctx* ctx;
    h2_guest* guest;
    h2_hyp_cfg hyp_cfg;
    stream_desc* sd;


    cmdline_parse(argc, argv, &cmd);

    if (cmd.error) {
        cmdline_usage(argv[0]);
        ret = EINVAL;
        goto out;
    }

    hyp_cfg.xen.xs.domid = 0;
    hyp_cfg.xen.xs.active = true;
#ifdef CONFIG_H2_XEN_NOXS
    hyp_cfg.xen.noxs.active = true;
#endif
    hyp_cfg.xen.xlib = h2_xen_xlib_t_xc;


    while (1) {
        ret = h2_open(&ctx, h2_hyp_t_xen, &hyp_cfg);
        if (ret) {
            goto out_h2;
        }

        sd = &ctx->ctrl.create.sd;
        sd->type = stream_type_net;
        sd->net.mode = stream_net_server;
        sd->net.endp.server.listen_endp.port = cmd.port;

        ret = h2_guest_ctrl_create_init(&ctx->ctrl.create, true);
        if (ret) {
            goto out_h2;
        }

        ret = h2_guest_create(ctx, &guest);
        if (ret) {
            goto out_ctx;
        }

        h2_guest_free(&guest);
out_ctx:
        h2_guest_ctrl_create_destroy(&ctx->ctrl.create);
out_h2:
        h2_close(&ctx);
    }

out:
    return -ret;
}
