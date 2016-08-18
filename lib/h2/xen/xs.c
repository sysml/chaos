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

#include <h2/xen/xs.h>

#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <xenstore.h>


int h2_xen_xs_domain_create(h2_xen_ctx* ctx, h2_guest* guest)
{
    int ret;

    xs_transaction_t th;

    char* dom_path = NULL;
    char* domid_str = NULL;
    char* name_path = NULL;
    char* domid_path = NULL;
    char* data_path = NULL;
    char* shutdown_path = NULL;

    struct xs_permissions dom_rw[1];
    struct xs_permissions dom_ro[2];

    dom_rw[0].id = guest->id;
    dom_rw[0].perms = XS_PERM_NONE;

    dom_ro[0].id = 0;
    dom_ro[0].perms = XS_PERM_NONE;
    dom_ro[1].id = guest->id;
    dom_ro[1].perms = XS_PERM_READ;

    dom_path = xs_get_domain_path(ctx->xsh, guest->id);

    asprintf(&domid_str, "%u", (unsigned int) guest->id);
    asprintf(&name_path, "%s/name", dom_path);
    asprintf(&domid_path, "%s/domid", dom_path);
    asprintf(&data_path, "%s/data", dom_path);
    asprintf(&shutdown_path, "%s/control/shutdown", dom_path);

th_start:
    ret = 0;
    th = xs_transaction_start(ctx->xsh);

    if (!xs_mkdir(ctx->xsh, th, dom_path)) {
        ret = errno;
        goto th_end;
    }
    if (!xs_set_permissions(ctx->xsh, th, dom_path, dom_ro, 2)) {
        ret = errno;
        goto th_end;
    }

    if (!xs_mkdir(ctx->xsh, th, data_path)) {
        ret = errno;
        goto th_end;
    }
    if (!xs_set_permissions(ctx->xsh, th, data_path, dom_rw, 1)) {
        ret = errno;
        goto th_end;
    }

    if (!xs_mkdir(ctx->xsh, th, shutdown_path)) {
        ret = errno;
        goto th_end;
    }
    if (!xs_set_permissions(ctx->xsh, th, shutdown_path, dom_rw, 1)) {
        ret = errno;
        goto th_end;
    }

    if (!xs_write(ctx->xsh, th, name_path, guest->name, strlen(guest->name))) {
        ret = errno;
        goto th_end;
    }

    if (xs_write(ctx->xsh, th, domid_path, domid_str, strlen(domid_str))) {
        ret = errno;
        goto th_end;
    }

th_end:
    if (ret) {
        xs_transaction_end(ctx->xsh, th, true);
    } else {
        if (!xs_transaction_end(ctx->xsh, th, false)) {
            if (errno == EAGAIN) {
                goto th_start;
            } else {
                ret = errno;
            }
        }
    }

    guest->hyp.info.xen->xs_dom_path = dom_path;

    free(domid_str);
    free(name_path);
    free(domid_path);
    free(data_path);
    free(shutdown_path);

    return ret;
}

int h2_xen_xs_domain_destroy(h2_xen_ctx* ctx, h2_guest* guest)
{
    if (!xs_rm(ctx->xsh, XBT_NULL, guest->hyp.info.xen->xs_dom_path)) {
        return errno;
    }

    return 0;
}

int h2_xen_xs_domain_intro(h2_xen_ctx* ctx, h2_guest* guest, h2_xen_dev_xenstore* xenstore)
{
    if (!xs_introduce_domain(ctx->xsh, guest->id, xenstore->mfn, xenstore->evtchn)) {
        return errno;
    }

    return 0;
}

int h2_xen_xs_console_create(h2_xen_ctx* ctx, h2_guest* guest, h2_xen_dev_console* console)
{
    int ret;

    xs_transaction_t th;

    char* dom_path;
    char* console_path;
    char* type_path;
    char* mfn_path;
    char* evtchn_path;
    char* type_val;
    char* mfn_val;
    char* evtchn_val;

    struct xs_permissions dom_rw[1];

    dom_rw[0].id = guest->id;
    dom_rw[0].perms = XS_PERM_NONE;

    dom_path = guest->hyp.info.xen->xs_dom_path;

    asprintf(&console_path, "%s/console", dom_path);
    asprintf(&type_path, "%s/type", console_path);
    asprintf(&mfn_path, "%s/ring-ref", console_path);
    asprintf(&evtchn_path, "%s/port", console_path);
    asprintf(&mfn_val, "%lu", console->mfn);
    asprintf(&evtchn_val, "%u", console->evtchn);
    type_val = "xenconsoled";

th_start:
    ret = 0;
    th = xs_transaction_start(ctx->xsh);

    if (!xs_mkdir(ctx->xsh, th, console_path)) {
        ret = errno;
        goto th_end;
    }

    if (!xs_set_permissions(ctx->xsh, th, console_path, dom_rw, 1)) {
        ret = errno;
        goto th_end;
    }

    if (!xs_write(ctx->xsh, th, type_path, type_val, strlen(type_val))) {
        ret = errno;
        goto th_end;
    }

    if (!xs_write(ctx->xsh, th, mfn_path, mfn_val, strlen(mfn_val))) {
        ret = errno;
        goto th_end;
    }

    if (!xs_write(ctx->xsh, th, evtchn_path, evtchn_val, strlen(evtchn_val))) {
        ret = errno;
        goto th_end;
    }

th_end:
    if (ret) {
        xs_transaction_end(ctx->xsh, th, true);
    } else {
        if (!xs_transaction_end(ctx->xsh, th, false)) {
            if (errno == EAGAIN) {
                goto th_start;
            } else {
                ret = errno;
            }
        }
    }

    free(console_path);
    free(type_path);
    free(mfn_path);
    free(evtchn_path);
    free(mfn_val);
    free(evtchn_val);

    return ret;
}
