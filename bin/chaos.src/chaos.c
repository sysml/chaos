#include "config.h"
#include "cmdline.h"


int main(int argc, char** argv)
{
    int ret;

    cmdline cmd;

    h2_ctx* ctx;
    h2_guest* guest;
    h2_hyp_cfg hyp_cfg;


    cmdline_parse(argc, argv, &cmd);

    if (cmd.error) {
        cmdline_usage(argv[0]);
        ret = EINVAL;
        goto out;
    }

    hyp_cfg.xen.xs.domid = 0;
    hyp_cfg.xen.xs.active = true;
    hyp_cfg.xen.xlib = h2_xen_xlib_t_xc;

    ret = h2_open(&ctx, h2_hyp_t_xen, &hyp_cfg);
    if (ret) {
        goto out_h2;
    }

    switch(cmd.op) {
        case op_none:
            break;

        case op_create:
            ret = config_parse(cmd.kernel, h2_hyp_t_xen, &guest);
            if (ret) {
                goto out_h2;
            }

            ret = h2_guest_create(ctx, guest);
            if (ret) {
                goto out_guest;
            }

            h2_guest_free(&guest);
            break;

        case op_destroy:
            ret = h2_guest_destroy(ctx, cmd.gid);
            if (ret) {
                goto out_h2;
            }
            break;
    }

    h2_close(&ctx);

    return 0;

out_guest:
    h2_guest_free(&guest);

out_h2:
    h2_close(&ctx);

out:
    return -ret;
}
