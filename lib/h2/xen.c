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

#include <h2/xen.h>
#include <h2/xen/dev.h>
#include <h2/xen/xc.h>
#include <h2/xen/xs.h>


int h2_xen_open(h2_xen_ctx** ctx, h2_xen_cfg* cfg)
{
    int ret;

    if (ctx == NULL) {
        ret = EINVAL;
        goto out_err;
    }

    (*ctx) = (h2_xen_ctx*) malloc(sizeof(h2_xen_ctx));
    if ((*ctx) == NULL) {
        ret = errno;
        goto out_err;
    }

    /* FIXME: log level should be configurable. Keep debug while developing. */
    (*ctx)->xc.xtl = (xentoollog_logger*) xtl_createlogger_stdiostream(stderr, XTL_DEBUG, 0);
    if ((*ctx)->xc.xtl == NULL) {
        ret = errno;
        goto out_mem;
    }

    (*ctx)->xc.xci = xc_interface_open((*ctx)->xc.xtl, NULL, 0);
    if ((*ctx)->xc.xci == NULL) {
        ret = errno;
        goto out_xtl;
    }

    if (cfg->xs.active) {
        (*ctx)->xs.active = true;
        (*ctx)->xs.domid = cfg->xs.domid;

        (*ctx)->xs.xsh = xs_open(0);
        if ((*ctx)->xs.xsh == NULL) {
            ret = errno;
            goto out_xci;
        }
    }

    return 0;

out_xci:
    xc_interface_close((*ctx)->xc.xci);

out_xtl:
    xtl_logger_destroy((*ctx)->xc.xtl);

out_mem:
    free(*ctx);
    (*ctx) = NULL;

out_err:
    return ret;
}

void h2_xen_close(h2_xen_ctx** ctx)
{
    if (ctx == NULL || (*ctx) == NULL) {
        return;
    }

    if ((*ctx)->xs.active && (*ctx)->xs.xsh) {
        xs_close((*ctx)->xs.xsh);
    }

    if ((*ctx)->xc.xci) {
        xc_interface_close((*ctx)->xc.xci);
    }

    if ((*ctx)->xc.xtl) {
        xtl_logger_destroy((*ctx)->xc.xtl);
    }

    free(*ctx);
    (*ctx) = NULL;
}


int h2_xen_guest_alloc(h2_xen_guest** guest)
{
    int ret;

    if (guest == NULL) {
        ret = EINVAL;
        goto out_err;
    }

    (*guest) = (h2_xen_guest*) calloc(1, sizeof(h2_xen_guest));
    if ((*guest) == NULL) {
        ret = errno;
        goto out_err;
    }

    return 0;

out_err:
    return ret;
}

void h2_xen_guest_free(h2_xen_guest** guest)
{
    if (guest == NULL || (*guest) == NULL) {
        return;
    }

    if ((*guest)->xs.dom_path) {
        free((*guest)->xs.dom_path);
        (*guest)->xs.dom_path = NULL;
    }

    for (int i; i < H2_XEN_DEV_COUNT_MAX; i++) {
        h2_xen_dev_free(&((*guest)->devs[i]));
    }

    free(*guest);
    (*guest) = NULL;
}


int h2_xen_domain_create(h2_xen_ctx* ctx, h2_guest* guest)
{
    int ret;

    bool xs_active;
    unsigned int xs_mfn;
    evtchn_port_t xs_evtchn;

    h2_xen_dev* dev;
    h2_xen_dev_console* console;


    xs_evtchn = 0;
    xs_active = ctx->xs.active && guest->hyp.info.xen->xs.active;

    ret = h2_xen_xc_domain_create(ctx, guest);
    if (ret) {
        goto out_err;
    }

    if (xs_active) {
        ret = h2_xen_xc_evtchn_alloc_unbound(ctx, guest->id, ctx->xs.domid, &xs_evtchn);
        if (ret) {
            goto out_dom;
        }
    }

    console = NULL;
    dev = h2_xen_dev_get_next(guest, h2_xen_dev_t_console, NULL);
    if (dev != NULL) {
        console = &(dev->dev.console);

        ret = h2_xen_xc_evtchn_alloc_unbound(ctx, guest->id, console->backend_id,
                &(console->evtchn));
        if (ret) {
            goto out_dom;
        }
    }

    ret = h2_xen_xc_domain_init(ctx, guest, xs_active, ctx->xs.domid, xs_evtchn, &xs_mfn, console);
    if (ret) {
        goto out_dom;
    }

    if (xs_active) {
        ret = h2_xen_xs_domain_create(ctx, guest);
        if (ret) {
            goto out_dom;
        }
    }

    ret = 0;
    for (int i = 0; i < H2_XEN_DEV_COUNT_MAX && !ret; i++) {
        ret = h2_xen_dev_create(ctx, guest, &(guest->hyp.info.xen->devs[i]));
    }
    if (ret) {
        goto out_dev;
    }

    if (xs_active) {
        ret = h2_xen_xs_domain_intro(ctx, guest, xs_evtchn, xs_mfn);
        if (ret) {
            goto out_xs;
        }
    }

    ret = h2_xen_xc_domain_unpause(ctx, guest);
    if (ret) {
        goto out_xs;
    }

    return 0;

out_dev:
    for (int i = 0; i < H2_XEN_DEV_COUNT_MAX; i++) {
        h2_xen_dev_destroy(ctx, guest, &(guest->hyp.info.xen->devs[i]));
    }

out_xs:
    if (xs_active) {
        h2_xen_xs_domain_destroy(ctx, guest);
    }

out_dom:
    h2_xen_xc_domain_destroy(ctx, guest);

out_err:
    return ret;
}

int h2_xen_domain_destroy(h2_xen_ctx* ctx, h2_guest_id id)
{
    int ret;
    int _ret;

    bool xs_active;

    h2_guest* guest;

    ret = h2_guest_alloc(&guest, h2_hyp_t_xen);
    if (ret) {
        goto out;
    }

    guest->id = id;

    if (ctx->xs.active) {
        ret = h2_xen_xs_probe_guest(ctx, guest);
        if (ret) {
            goto out;
        }
        xs_active = guest->hyp.info.xen->xs.active;
    } else {
        xs_active = false;
    }

    ret = h2_xen_dev_enumerate(ctx, guest);
    if (ret) {
        goto out;
    }


    for (int i = 0; i < H2_XEN_DEV_COUNT_MAX; i++) {
        _ret = h2_xen_dev_destroy(ctx, guest, &(guest->hyp.info.xen->devs[i]));
        if (_ret && !ret) {
            ret = _ret;
        }
    }

    if (xs_active) {
        _ret = h2_xen_xs_domain_destroy(ctx, guest);
        if (_ret && !ret) {
            ret = _ret;
        }
    }

    _ret = h2_xen_xc_domain_destroy(ctx, guest);
    if (_ret && !ret) {
        ret = _ret;
    }


    h2_guest_free(&guest);

out:
    return ret;
}
