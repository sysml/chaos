/*
 * chaos
 *
 * Authors: Filipe Manco <filipe.manco@neclab.eu>
 *
 *
 * Copyright (c) 2016, NEC Europe Ltd., NEC Corporation All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * THIS HEADER MAY NOT BE EXTRACTED OR MODIFIED IN ANY WAY.
 */

#include <chaos/cmdline.h>
#include <h2/config.h>


static int __guest_ctrl_create_open(h2_guest_ctrl_create* gcc, bool restore)
{
    int ret;

    ret = h2_guest_ctrl_create_init(gcc, restore);
    if (ret) {
        goto out_err;
    }

    ret = h2_guest_ctrl_create_open(gcc);
    if (ret) {
        goto out_destroy;
    }

    return 0;

out_destroy:
    h2_guest_ctrl_create_destroy(gcc);
out_err:
    return ret;
}

static void __guest_ctrl_create_close(h2_guest_ctrl_create* gcc)
{
    h2_guest_ctrl_create_close(gcc);
    h2_guest_ctrl_create_destroy(gcc);
}

static int __guest_ctrl_save_open(h2_guest_ctrl_save* gcs)
{
    int ret;

    ret = h2_guest_ctrl_save_init(gcs);
    if (ret) {
        goto out_err;
    }

    ret = h2_guest_ctrl_save_open(gcs);
    if (ret) {
        goto out_destroy;
    }

    return 0;

out_destroy:
    h2_guest_ctrl_save_destroy(gcs);
out_err:
    return ret;
}

static void __guest_ctrl_save_close(h2_guest_ctrl_save* gcs)
{
    h2_guest_ctrl_save_close(gcs);
    h2_guest_ctrl_save_destroy(gcs);
}



int main(int argc, char** argv)
{
    int ret;

    cmdline cmd;

    h2_ctx* ctx;
    h2_guest* guest;
    h2_hyp_cfg hyp_cfg;

    h2_guest_ctrl_create gcc;
    h2_guest_ctrl_save gcs;


    cmdline_parse(argc, argv, &cmd);

    if (cmd.error || cmd.help) {
        cmdline_usage(argv[0]);
        ret = cmd.error ? EINVAL : 0;
        goto out;
    }

    hyp_cfg.xen.xs.domid = 0;
    hyp_cfg.xen.xs.active = cmd.enable_xs;
#ifdef CONFIG_H2_XEN_NOXS
    hyp_cfg.xen.noxs.active = cmd.enable_noxs;
#endif
    hyp_cfg.xen.xlib = h2_xen_xlib_t_xc;

    ret = h2_open(&ctx, h2_hyp_t_xen, &hyp_cfg);
    if (ret) {
        goto out_h2;
    }

    switch(cmd.op) {
        case op_none:
            break;

        case op_create:
            gcc.sd.type = stream_type_file;
            gcc.sd.file.op = stream_file_op_read;
            gcc.sd.file.filename = cmd.kernel;

            ret = __guest_ctrl_create_open(&gcc, false);
            if (ret) {
                goto out_h2;
            }

            for (int i = 0; i < cmd.nr_doms; i++) {
                ret = h2_guest_deserialize(ctx, &gcc, &guest);
                if (ret) {
                    goto out_h2;
                }

                ret = h2_guest_create(ctx, guest);
                if (ret) {
                    goto out_guest;
                }
                h2_guest_reuse(guest);
            }

            h2_guest_free(&guest);
            __guest_ctrl_create_close(&gcc);
            break;

        case op_destroy:
            ret = h2_guest_query(ctx, cmd.gid, &guest);
            if (ret) {
                goto out_h2;
            }

            ret = h2_guest_destroy(ctx, guest);
            if (ret) {
                goto out_guest;
            }

            h2_guest_free(&guest);
            break;

        case op_shutdown:
            ret = h2_guest_query(ctx, cmd.gid, &guest);
            if (ret) {
                goto out_h2;
            }

            ret = h2_guest_shutdown(ctx, guest, cmd.wait);
            if (ret) {
                goto out_guest;
            }

            if (cmd.keep == false) {
                ret = h2_guest_destroy(ctx, guest);
                if (ret) {
                    goto out_guest;
                }
            }

            h2_guest_free(&guest);
            break;

        case op_save:
            gcs.sd.type = stream_type_file;
            gcs.sd.file.op = stream_file_op_write;
            gcs.sd.file.filename = cmd.filename;

            ret = __guest_ctrl_save_open(&gcs);
            if (ret) {
                goto out_h2;
            }

            ret = h2_guest_query(ctx, cmd.gid, &guest);
            if (ret) {
                goto out_h2;
            }

            ret = h2_guest_serialize(ctx, &gcs, guest);
            if (ret) {
                goto out_h2;
            }

            ret = h2_guest_save(ctx, guest, cmd.wait);
            if (ret) {
                goto out_guest;
            }

            if (cmd.keep) {
                ret = h2_guest_resume(ctx, guest);
            } else {
                ret = h2_guest_destroy(ctx, guest);
            }
            if (ret) {
                goto out_guest;
            }

            h2_guest_free(&guest);
            __guest_ctrl_save_close(&gcs);
            break;

        case op_restore:
            gcc.sd.type = stream_type_file;
            gcc.sd.file.op = stream_file_op_read;
            gcc.sd.file.filename = cmd.filename;

            ret = __guest_ctrl_create_open(&gcc, true);
            if (ret) {
                goto out_h2;
            }

            ret = h2_guest_deserialize(ctx, &gcc, &guest);
            if (ret) {
                goto out_h2;
            }

            ret = h2_guest_create(ctx, guest);
            if (ret) {
                goto out_guest;
            }

            h2_guest_free(&guest);
            __guest_ctrl_create_close(&gcc);
            break;

        case op_migrate:
            gcs.sd.type = stream_type_net;
            gcs.sd.net.mode = stream_net_client;
            gcs.sd.net.endp.client.server_endp = cmd.destination;

            ret = __guest_ctrl_save_open(&gcs);
            if (ret) {
                goto out_h2;
            }

            ret = h2_guest_query(ctx, cmd.gid, &guest);
            if (ret) {
                goto out_h2;
            }

            ret = h2_guest_serialize(ctx, &gcs, guest);
            if (ret) {
                goto out_h2;
            }

            ret = h2_guest_save(ctx, guest, cmd.wait);
            if (ret) {
                goto out_guest;
            }

            ret = h2_guest_destroy(ctx, guest);
            if (ret) {
                goto out_guest;
            }

            h2_guest_free(&guest);
            __guest_ctrl_save_close(&gcs);
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
