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

#include <h2/xen/dev.h>
#ifdef CONFIG_H2_XEN_NOXS
#include <h2/xen/noxs.h>
#endif
#include <h2/xen/vif.h>
#include <h2/xen/xs.h>


void h2_xen_dev_free(h2_xen_dev* dev)
{
    switch (dev->type) {
        case h2_xen_dev_t_none:
            break;

        case h2_xen_dev_t_vif:
            h2_xen_vif_free(&(dev->dev.vif));
            break;
    }

    dev->type = h2_xen_dev_t_none;
}


h2_xen_dev* h2_xen_dev_get_next(h2_guest* guest, h2_xen_dev_t type, int* idx)
{
    int i;
    h2_xen_dev* dev;

    dev = NULL;
    for (i = idx ? (*idx) : 0; i < H2_XEN_DEV_COUNT_MAX; i++) {
        if (guest->hyp.info.xen->devs[i].type == type) {
            dev = &(guest->hyp.info.xen->devs[i]);
            break;
        }
    }

    if (idx) {
        (*idx) = i;
    }

    return dev;
}


int h2_xen_dev_enumerate(h2_xen_ctx* ctx, h2_guest* guest)
{
    int ret;

    if (ctx->xs.active && guest->hyp.info.xen->xs.active) {
        ret = h2_xen_xs_dev_enumerate(ctx, guest);
        if (ret) {
            goto out_err;
        }
    }

#ifdef CONFIG_H2_XEN_NOXS
    if (ctx->noxs.active && guest->hyp.info.xen->noxs.active) {
        ret = h2_xen_noxs_dev_enumerate(ctx, guest);
        if (ret) {
            goto out_err;
        }
    }
#endif

    return 0;

out_err:
    return ret;
}

int h2_xen_dev_create(h2_xen_ctx* ctx, h2_guest* guest, h2_xen_dev* dev)
{
    int ret;

    ret = 0;
    switch (dev->type) {
        case h2_xen_dev_t_none:
            break;

        case h2_xen_dev_t_vif:
            ret = h2_xen_vif_create(ctx, guest, &(dev->dev.vif));
            break;
    }

    return ret;
}

int h2_xen_dev_destroy(h2_xen_ctx* ctx, h2_guest* guest, h2_xen_dev* dev)
{
    int ret;

    ret = 0;
    switch (dev->type) {
        case h2_xen_dev_t_none:
            break;

        case h2_xen_dev_t_vif:
            ret = h2_xen_vif_destroy(ctx, guest, &(dev->dev.vif));
            break;
    }

    return ret;
}
