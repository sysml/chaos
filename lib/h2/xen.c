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


int h2_xen_open(h2_xen_ctx** ctx)
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
    (*ctx)->xtl = (xentoollog_logger*) xtl_createlogger_stdiostream(stderr, XTL_DEBUG, 0);
    if ((*ctx)->xtl == NULL) {
        ret = errno;
        goto out_mem;
    }

    (*ctx)->xci = xc_interface_open((*ctx)->xtl, NULL, 0);
    if ((*ctx)->xci == NULL) {
        ret = errno;
        goto out_xtl;
    }

    (*ctx)->xsh = xs_open(0);
    if ((*ctx)->xsh == NULL) {
        ret = errno;
        goto out_xci;
    }

    return 0;

out_xci:
    xc_interface_close((*ctx)->xci);

out_xtl:
    xtl_logger_destroy((*ctx)->xtl);

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

    if ((*ctx)->xsh) {
        xs_close((*ctx)->xsh);
    }

    if ((*ctx)->xci) {
        xc_interface_close((*ctx)->xci);
    }

    if ((*ctx)->xtl) {
        xtl_logger_destroy((*ctx)->xtl);
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

    if ((*guest)->xs_dom_path) {
        free((*guest)->xs_dom_path);
        (*guest)->xs_dom_path = NULL;
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

    int idx;
    h2_xen_dev* dev;
    h2_xen_dev_xenstore* xenstore;
    h2_xen_dev_console* console;
    xc_evtchn_port_or_error_t ec_ret;

    ret = h2_xen_xc_domain_create(ctx, guest);
    if (ret) {
        goto out_err;
    }

    idx = 0;
    dev = NULL;
    xenstore = NULL;
    dev = h2_xen_dev_get_next(guest, h2_xen_dev_t_xenstore, &idx);
    if (dev != NULL) {
        xenstore = &(dev->dev.xenstore);
        ec_ret = xc_evtchn_alloc_unbound(ctx->xci, guest->id, xenstore->backend_id);
        if (ec_ret == -1) {
            ret = errno;
            goto out_dom;
        }
        xenstore->evtchn = ec_ret;
    }

    idx = 0;
    dev = NULL;
    console = NULL;
    dev = h2_xen_dev_get_next(guest, h2_xen_dev_t_console, &idx);
    if (dev != NULL) {
        console = &(dev->dev.console);
        ec_ret = xc_evtchn_alloc_unbound(ctx->xci, guest->id, console->backend_id);
        if (ec_ret == -1) {
            ret = errno;
            goto out_dom;
        }
        console->evtchn = ec_ret;
    }

    ret = h2_xen_xc_domain_init(ctx, guest, xenstore, console);
    if (ret) {
        goto out_dom;
    }

    ret = h2_xen_xs_domain_create(ctx, guest);
    if (ret) {
        goto out_dom;
    }

    if (console != NULL) {
        ret = h2_xen_xs_console_create(ctx, guest, console);
        if (ret) {
            goto out_xs;
        }
    }

    ret = 0;
    for (int i = 0; i < H2_XEN_DEV_COUNT_MAX && !ret; i++) {
        ret = h2_xen_dev_create(ctx, guest, &(guest->hyp.info.xen->devs[i]));
    }
    if (ret) {
        goto out_dev;
    }

    if (xenstore != NULL) {
        ret = h2_xen_xs_domain_intro(ctx, guest, xenstore);
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
    h2_xen_xs_domain_destroy(ctx, guest);

out_dom:
    h2_xen_xc_domain_destroy(ctx, guest);

out_err:
    return ret;
}

int h2_xen_domain_destroy(h2_xen_ctx* ctx, h2_guest_id id)
{
    int ret;
    int _ret;

    h2_guest* guest;

    ret = h2_guest_alloc(&guest, h2_hyp_t_xen);
    if (ret) {
        ret = errno;
        goto out;
    }

    guest->id = id;
    guest->hyp.info.xen->xs_dom_path = xs_get_domain_path(ctx->xsh, guest->id);


    /* FIXME: Need to enumerate devices, so that they're destroyed */

    _ret = h2_xen_xc_domain_destroy(ctx, guest);
    if (_ret && !ret) {
        ret = _ret;
    }

    _ret = h2_xen_xs_domain_destroy(ctx, guest);
    if (_ret && !ret) {
        ret = _ret;
    }

    for (int i = 0; i < H2_XEN_DEV_COUNT_MAX; i++) {
        _ret = h2_xen_dev_destroy(ctx, guest, &(guest->hyp.info.xen->devs[i]));
        if (_ret && !ret) {
            ret = _ret;
        }
    }


    h2_guest_free(&guest);

out:
    return ret;
}
