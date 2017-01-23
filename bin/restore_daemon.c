#include <restore_daemon/cmdline.h>
#include <h2/stream.h>


int main(int argc, char** argv)
{
    int ret;

    cmdline cmd;

    h2_ctx* ctx;
    h2_guest* guest;
    h2_hyp_cfg hyp_cfg;

    h2_guest_ctrl_create gcc;


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

    gcc.sd.type = stream_type_net;
    gcc.sd.net.mode = stream_net_server;
    gcc.sd.net.endp.server.listen_endp.port = cmd.port;

    ret = h2_guest_ctrl_create_init(&gcc, true);
    if (ret) {
        goto out;
    }

    while (1) {
        ret = h2_open(&ctx, h2_hyp_t_xen, &hyp_cfg);
        if (ret) {
            goto out_h2;
        }

        ret = h2_guest_ctrl_create_open(&gcc);
        if (ret) {
            goto out_h2;
        }

        ret = h2_guest_deserialize(ctx, &gcc, &guest);
        if (ret) {
            goto out_h2;
        }

        ret = h2_guest_create(ctx, guest);
        if (ret) {//TODO remove
            goto out_ctx;
        }

        h2_guest_free(&guest);
out_ctx:
        h2_guest_ctrl_create_close(&gcc);
out_h2:
        h2_close(&ctx);
    }

    h2_guest_ctrl_create_destroy(&gcc);

out:
    return -ret;
}
