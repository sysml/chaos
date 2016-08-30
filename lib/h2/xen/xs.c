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
#include <h2/xen/dev.h>

#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xenstore.h>


static int __write_kv(h2_xen_ctx* ctx, xs_transaction_t th, char* path, char* key, char* value)
{
    int ret;
    char* fpath;

    ret = 0;
    asprintf(&fpath, "%s/%s", path, key);

    if (!xs_write(ctx->xsh, th, fpath, value, strlen(value))) {
        ret = errno;
    }

    free(fpath);

    return ret;
}

static int __read_kv(h2_xen_ctx* ctx, xs_transaction_t th, char* path, char* key, char** value)
{
    int ret;
    char* fpath;
    unsigned int len;

    ret = 0;
    asprintf(&fpath, "%s/%s", path, key);

    (*value) = xs_read(ctx->xsh, th, fpath, &len);
    if ((*value)) {
        ret = errno;
    }

    free(fpath);

    return ret;
}

static int __enumerate_xenstore(h2_xen_ctx* ctx, h2_guest* guest)
{
    int ret;

    char* dom_path;
    char* xs_val;
    unsigned int xs_val_len;
    int idx;
    h2_xen_dev* dev;


    ret = 0;
    dom_path = guest->hyp.info.xen->xs_dom_path;

    /* Check if the domain has xenstore by reading domain path. The value read
     * is not important, is discarded immediately.
     */
    xs_val = xs_read(ctx->xsh, XBT_NULL, dom_path, &xs_val_len);
    if (xs_val == NULL) {
        free(xs_val);
        ret = EINVAL;
        goto out;
    }
    free(xs_val);

    /* Domain has a xenstore so lets add that device to the list. */

    idx = 0;
    dev = h2_xen_dev_get_next(guest, h2_xen_dev_t_none, &idx);
    if (!dev) {
        ret = ENOMEM;
        goto out;
    }

    dev->type = h2_xen_dev_t_xenstore;
    /* There's no way to retrieve these. */
    dev->dev.xenstore.evtchn = 0;
    dev->dev.xenstore.mfn = 0;

out:
    return ret;
}

static int __enumerate_console(h2_xen_ctx* ctx, h2_guest* guest)
{
    int ret;

    char* dom_path;
    char* xs_val;
    int idx;
    h2_xen_dev* dev;


    ret = 0;
    dom_path = guest->hyp.info.xen->xs_dom_path;

    /* Check if the domain has xenstore by reading console path. The value read
     * is not important, is discarded immediately.
     */
    ret = __read_kv(ctx, XBT_NULL, dom_path, "console", &xs_val);
    if (ret) {
        /* Not having a console is not an error. */
        ret = 0;
        goto out;
    }
    free(xs_val);

    /* Domain has a console so lets add that device to the list. */

    idx = 0;
    dev = h2_xen_dev_get_next(guest, h2_xen_dev_t_none, &idx);
    if (!dev) {
        ret = ENOMEM;
        goto out;
    }

    dev->type = h2_xen_dev_t_console;

    /* FIXME: Need to retrieve these */
    dev->dev.console.backend_id = 0;
    dev->dev.console.evtchn = 0;
    dev->dev.console.mfn = 0;

out:
    return ret;
}

int h2_xen_xs_domain_create(h2_xen_ctx* ctx, h2_guest* guest)
{
    int ret;

    xs_transaction_t th;

    char* domid_str = NULL;
    char* dom_path = NULL;
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

    ret = __write_kv(ctx, th, dom_path, "name", guest->name);
    if (ret) {
        goto th_end;
    }

    ret = __write_kv(ctx, th, dom_path, "domid", domid_str);
    if (ret) {
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

int h2_xen_xs_dev_enumerate(h2_xen_ctx* ctx, h2_guest* guest)
{
    int ret;

    ret = __enumerate_xenstore(ctx, guest);
    if (ret) {
        if (ret == EINVAL) {
            ret = 0;
        }
        goto out;
    }

    ret = __enumerate_console(ctx, guest);
    if (ret) {
        goto out;
    }

out:
    return ret;
}

int h2_xen_xs_console_create(h2_xen_ctx* ctx, h2_guest* guest, h2_xen_dev_console* console)
{
    int ret;

    xs_transaction_t th;

    char* dom_path;
    char* console_path;
    char* type_val;
    char* mfn_val;
    char* evtchn_val;

    struct xs_permissions dom_rw[1];

    dom_rw[0].id = guest->id;
    dom_rw[0].perms = XS_PERM_NONE;

    dom_path = guest->hyp.info.xen->xs_dom_path;

    asprintf(&console_path, "%s/console", dom_path);
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

    ret = __write_kv(ctx, th, console_path, "type", type_val);
    if (ret) {
        goto th_end;
    }

    ret = __write_kv(ctx, th, console_path, "ring-ref", mfn_val);
    if (ret) {
        goto th_end;
    }

    ret = __write_kv(ctx, th, console_path, "port", evtchn_val);
    if (ret) {
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
    free(mfn_val);
    free(evtchn_val);

    return ret;
}
