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
 */

#ifdef CONFIG_H2_XEN_NOXS
#include <h2/xen/noxs.h>
#endif
#include <h2/xen/vif.h>
#include <h2/xen/xs.h>


void h2_xen_vif_reuse(h2_xen_dev_vif* vif)
{
    vif->valid = false;
}

void h2_xen_vif_free(h2_xen_dev_vif* vif)
{
    if (vif->bridge) {
        free(vif->bridge);
        vif->bridge = NULL;
    }

    if (vif->script) {
        free(vif->script);
        vif->script = NULL;
    }
}


int h2_xen_vif_create(h2_xen_ctx* ctx, h2_guest* guest, h2_xen_dev_vif* vif)
{
    int ret;

    if (vif->valid) {
        ret = EINVAL;
        goto out;
    }

    switch (vif->meth) {
        case h2_xen_dev_meth_t_xs:
            if (ctx->xs.active && guest->hyp.guest.xen->xs.active) {
                ret = h2_xen_xs_vif_create(ctx, guest, vif);
                if (!ret) {
                    vif->valid = true;
                }
            } else {
                ret = EINVAL;
            }
            break;

#ifdef CONFIG_H2_XEN_NOXS
        case h2_xen_dev_meth_t_noxs:
            if (ctx->noxs.active && guest->hyp.guest.xen->noxs.active) {
                ret = h2_xen_noxs_vif_create(ctx, guest, vif);
                if (!ret) {
                    vif->valid = true;
                }
            } else {
                ret = EINVAL;
            }
            break;
#endif
    }

out:
    return ret;
}

int h2_xen_vif_destroy(h2_xen_ctx* ctx, h2_guest* guest, h2_xen_dev_vif* vif)
{
    int ret;

    if (!vif->valid) {
        ret = EINVAL;
        goto out;
    }

    switch (vif->meth) {
        case h2_xen_dev_meth_t_xs:
            if (ctx->xs.active && guest->hyp.guest.xen->xs.active) {
                ret = h2_xen_xs_vif_destroy(ctx, guest, vif);
                if (!ret) {
                    vif->valid = false;
                }
            } else {
                ret = EINVAL;
            }
            break;

#ifdef CONFIG_H2_XEN_NOXS
        case h2_xen_dev_meth_t_noxs:
            if (ctx->noxs.active && guest->hyp.guest.xen->noxs.active) {
                ret = h2_xen_noxs_vif_destroy(ctx, guest, vif);
                if (!ret) {
                    vif->valid = false;
                }
            } else {
                ret = EINVAL;
            }
            break;
#endif
    }

out:
    return ret;
}
