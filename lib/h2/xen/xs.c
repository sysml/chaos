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
#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <xenstore.h>


static int __guest_pre(h2_xen_ctx* ctx, h2_guest* guest)
{
    int ret;

    char* dom_path;

    ret = 0;

    if (!guest->hyp.info.xen->xs.active) {
        ret = EINVAL;
        goto out;
    }

    if (guest->hyp.info.xen->xs.dom_path == NULL) {
        dom_path = xs_get_domain_path(ctx->xs.xsh, guest->id);
        if (dom_path == NULL) {
            ret = errno;
            goto out;
        }

        guest->hyp.info.xen->xs.dom_path = dom_path;
    }

out:
    return ret;
}

static int __write_kv(h2_xen_ctx* ctx, xs_transaction_t th, char* path, char* key, char* value)
{
    int ret;

    char* fpath;

    ret = 0;

    asprintf(&fpath, "%s/%s", path, key);

    if (!xs_write(ctx->xs.xsh, th, fpath, value, strlen(value))) {
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

    (*value) = xs_read(ctx->xs.xsh, th, fpath, &len);
    if ((*value)) {
        ret = errno;
    }

    free(fpath);

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

    dom_path = guest->hyp.info.xen->xs.dom_path;

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

    dev->dev.console.meth = h2_xen_dev_meth_t_xs;
    /* FIXME: Need to retrieve these */
    dev->dev.console.backend_id = 0;
    dev->dev.console.evtchn = 0;
    dev->dev.console.mfn = 0;

out:
    return ret;
}

static int __enumerate_vif(h2_xen_ctx* ctx, h2_guest* guest)
{
    int ret;

    char** xs_list;
    unsigned int xs_list_num;

    char* fe_dev_path;
    char* fe_path;

    int idx;
    h2_xen_dev* dev;

    ret = 0;

    asprintf(&fe_path, "%s/device/%s", guest->hyp.info.xen->xs.dom_path, "vif");

    xs_list = xs_directory(ctx->xs.xsh, XBT_NULL, fe_path, &xs_list_num);
    if (xs_list == NULL) {
        /* List is empty. */
        goto out_path;
    }

    idx = 0;
    for (int i = 0; i < xs_list_num; i++) {
        char* be_id_str;

        dev = h2_xen_dev_get_next(guest, h2_xen_dev_t_none, &idx);
        if (!dev) {
            ret = ENOMEM;
            break;
        }

        asprintf(&fe_dev_path, "%s/%s", fe_path, xs_list[i]);

        ret = __read_kv(ctx, XBT_NULL, fe_dev_path, "backend-id", &be_id_str);
        if (ret) {
            free(fe_dev_path);
            continue;
        }

        dev->type = h2_xen_dev_t_vif;
        dev->dev.vif.id = atoi(xs_list[i]);
        dev->dev.vif.valid = true;
        dev->dev.vif.meth = h2_xen_dev_meth_t_xs;
        dev->dev.vif.backend_id = atoi(be_id_str);
        /* FIXME: Fill dev->dev.vif.ip */
        /* FIXME: Fill dev->dev.vif.mac */
        dev->dev.vif.bridge = NULL;
        dev->dev.vif.script = NULL;

        free(be_id_str);
        free(fe_dev_path);

    }

    free(xs_list);
out_path:
    free(fe_path);

    return ret;
}


int h2_xen_xs_open(h2_xen_ctx* ctx)
{
    int ret;

    if (ctx == NULL) {
        ret = EINVAL;
        goto out_err;
    }

    ctx->xs.xsh = xs_open(0);
    if (ctx->xs.xsh == NULL) {
        ret = errno;
        goto out_err;
    }

    return 0;

out_err:
    return ret;
}

int h2_xen_xs_close(h2_xen_ctx* ctx)
{
    int ret;

    if (ctx == NULL) {
        ret = EINVAL;
        goto out_err;
    }

    if (ctx->xs.xsh == NULL) {
        ret = EINVAL;
        goto out_err;
    }

    xs_close(ctx->xs.xsh);
    ctx->xs.xsh = NULL;

    return 0;

out_err:
    return ret;
}

int h2_xen_xs_domain_create(h2_xen_ctx* ctx, h2_guest* guest)
{
    int ret;

    xs_transaction_t th;

    char* domid_str;
    char* dom_path;
    char* data_path;
    char* shutdown_path;

    struct xs_permissions dom_rw[1];
    struct xs_permissions dom_ro[2];

    ret = __guest_pre(ctx, guest);
    if (ret) {
        goto out;
    }

    dom_rw[0].id = guest->id;
    dom_rw[0].perms = XS_PERM_NONE;

    dom_ro[0].id = 0;
    dom_ro[0].perms = XS_PERM_NONE;
    dom_ro[1].id = guest->id;
    dom_ro[1].perms = XS_PERM_READ;

    dom_path = guest->hyp.info.xen->xs.dom_path;

    asprintf(&domid_str, "%u", (unsigned int) guest->id);
    asprintf(&data_path, "%s/data", dom_path);
    asprintf(&shutdown_path, "%s/control/shutdown", dom_path);

th_start:
    ret = 0;
    th = xs_transaction_start(ctx->xs.xsh);

    if (!xs_mkdir(ctx->xs.xsh, th, dom_path)) {
        ret = errno;
        goto th_end;
    }
    if (!xs_set_permissions(ctx->xs.xsh, th, dom_path, dom_ro, 2)) {
        ret = errno;
        goto th_end;
    }

    if (!xs_mkdir(ctx->xs.xsh, th, data_path)) {
        ret = errno;
        goto th_end;
    }
    if (!xs_set_permissions(ctx->xs.xsh, th, data_path, dom_rw, 1)) {
        ret = errno;
        goto th_end;
    }

    if (!xs_mkdir(ctx->xs.xsh, th, shutdown_path)) {
        ret = errno;
        goto th_end;
    }
    if (!xs_set_permissions(ctx->xs.xsh, th, shutdown_path, dom_rw, 1)) {
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
        xs_transaction_end(ctx->xs.xsh, th, true);
    } else {
        if (!xs_transaction_end(ctx->xs.xsh, th, false)) {
            if (errno == EAGAIN) {
                goto th_start;
            } else {
                ret = errno;
            }
        }
    }

    free(domid_str);
    free(data_path);
    free(shutdown_path);
out:
    return ret;
}

int h2_xen_xs_domain_destroy(h2_xen_ctx* ctx, h2_guest* guest)
{
    int ret;

    ret = __guest_pre(ctx, guest);
    if (ret) {
        goto out;
    }

    if (!xs_rm(ctx->xs.xsh, XBT_NULL, guest->hyp.info.xen->xs.dom_path)) {
        ret = errno;
    }

out:
    return ret;
}

int h2_xen_xs_domain_intro(h2_xen_ctx* ctx, h2_guest* guest,
        evtchn_port_t evtchn, unsigned int mfn)
{
    int ret;

    ret = __guest_pre(ctx, guest);
    if (ret) {
        goto out;
    }

    if (!xs_introduce_domain(ctx->xs.xsh, guest->id, mfn, evtchn)) {
        ret = errno;
    }

out:
    return ret;
}

int h2_xen_xs_probe_guest(h2_xen_ctx* ctx, h2_guest* guest)
{
    int ret;

    char* dom_path;
    char* xs_val;
    unsigned int xs_val_len;

    ret = 0;

    guest->hyp.info.xen->xs.active = false;
    if (guest->hyp.info.xen->xs.dom_path) {
        free(guest->hyp.info.xen->xs.dom_path);
        guest->hyp.info.xen->xs.dom_path = NULL;
    }

    dom_path = xs_get_domain_path(ctx->xs.xsh, guest->id);
    if (!dom_path) {
        ret = errno;
        goto out;
    }

    /* Check if the domain has xenstore by reading domain path. The value read
     * is not important, is discarded immediately.
     */
    xs_val = xs_read(ctx->xs.xsh, XBT_NULL, dom_path, &xs_val_len);
    if (xs_val == NULL) {
        guest->hyp.info.xen->xs.active = false;
        free(dom_path);
    } else {
        guest->hyp.info.xen->xs.active = true;
        guest->hyp.info.xen->xs.dom_path = dom_path;
        free(xs_val);
    }

out:
    return ret;
}

int h2_xen_xs_dev_enumerate(h2_xen_ctx* ctx, h2_guest* guest)
{
    int ret;

    ret = __guest_pre(ctx, guest);
    if (ret) {
        goto out;
    }

    ret = __enumerate_console(ctx, guest);
    if (ret) {
        goto out;
    }

    ret = __enumerate_vif(ctx, guest);
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

    char* console_path;
    char* type_val;
    char* mfn_val;
    char* evtchn_val;

    struct xs_permissions dom_rw[1];

    ret = __guest_pre(ctx, guest);
    if (ret) {
        goto out;
    }

    dom_rw[0].id = guest->id;
    dom_rw[0].perms = XS_PERM_NONE;

    asprintf(&console_path, "%s/console", guest->hyp.info.xen->xs.dom_path);
    asprintf(&mfn_val, "%lu", console->mfn);
    asprintf(&evtchn_val, "%u", console->evtchn);
    type_val = "xenconsoled";

th_start:
    ret = 0;
    th = xs_transaction_start(ctx->xs.xsh);

    if (!xs_mkdir(ctx->xs.xsh, th, console_path)) {
        ret = errno;
        goto th_end;
    }
    if (!xs_set_permissions(ctx->xs.xsh, th, console_path, dom_rw, 1)) {
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
        xs_transaction_end(ctx->xs.xsh, th, true);
    } else {
        if (!xs_transaction_end(ctx->xs.xsh, th, false)) {
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
out:
    return ret;
}

int h2_xen_xs_console_destroy(h2_xen_ctx* ctx, h2_guest* guest, h2_xen_dev_console* console)
{
    int ret;

    char* console_path;

    ret = __guest_pre(ctx, guest);
    if (ret) {
        goto out;
    }

    asprintf(&console_path, "%s/console", guest->hyp.info.xen->xs.dom_path);

    ret = 0;
    if (!xs_rm(ctx->xs.xsh, XBT_NULL, console_path)) {
        ret = errno;
    }

    free(console_path);

out:
    return ret;
}

int h2_xen_xs_vif_create(h2_xen_ctx* ctx, h2_guest* guest, h2_xen_dev_vif* vif)
{
    int ret;

    xs_transaction_t th;

    char* dev_id_str;
    char* mac_str;
    char* fe_dom_path;
    char* fe_path;
    char* fe_id_str;
    char* be_dom_path;
    char* be_path;
    char* be_id_str;

    struct xs_permissions fe_perms[2];
    struct xs_permissions be_perms[2];

    ret = __guest_pre(ctx, guest);
    if (ret) {
        goto out;
    }

    fe_perms[0].id = guest->id;
    fe_perms[0].perms = XS_PERM_NONE;
    fe_perms[1].id = vif->backend_id;
    fe_perms[1].perms = XS_PERM_READ;

    be_perms[0].id = vif->backend_id;
    be_perms[0].perms = XS_PERM_NONE;
    be_perms[1].id = guest->id;
    be_perms[1].perms = XS_PERM_READ;

    asprintf(&dev_id_str, "%d", vif->id);
    asprintf(&mac_str, "%2"SCNx8":%2"SCNx8":%2"SCNx8":%2"SCNx8":%2"SCNx8":%2"SCNx8,
            vif->mac[0], vif->mac[1], vif->mac[2], vif->mac[3], vif->mac[4], vif->mac[5]);

    fe_dom_path = guest->hyp.info.xen->xs.dom_path;
    asprintf(&fe_path, "%s/device/%s/%s", fe_dom_path, "vif", dev_id_str);
    asprintf(&fe_id_str, "%u", (domid_t) guest->id);

    be_dom_path = xs_get_domain_path(ctx->xs.xsh, vif->backend_id);
    asprintf(&be_path, "%s/backend/%s/%u/%d", be_dom_path, "vif", (domid_t) guest->id, vif->id);
    asprintf(&be_id_str, "%d", vif->backend_id);

th_start:
    ret = 0;
    th = xs_transaction_start(ctx->xs.xsh);

    if (!xs_mkdir(ctx->xs.xsh, th, fe_path)) {
        ret = errno;
        goto th_end;
    }

    if (!xs_set_permissions(ctx->xs.xsh, th, fe_path, fe_perms, 2)) {
        ret = errno;
        goto th_end;
    }

    ret = __write_kv(ctx, th, fe_path, "backend", be_path);
    if (ret) {
        goto th_end;
    }

    ret = __write_kv(ctx, th, fe_path, "backend-id", be_id_str);
    if (ret) {
        goto th_end;
    }

    ret = __write_kv(ctx, th, fe_path, "state", "1");
    if (ret) {
        goto th_end;
    }

    ret = __write_kv(ctx, th, fe_path, "handle", dev_id_str);
    if (ret) {
        goto th_end;
    }

    ret = __write_kv(ctx, th, fe_path, "mac", mac_str);
    if (ret) {
        goto th_end;
    }

    if (!xs_mkdir(ctx->xs.xsh, th, be_path)) {
        ret = errno;
        goto th_end;
    }

    if (!xs_set_permissions(ctx->xs.xsh, th, be_path, be_perms, 2)) {
        ret = errno;
        goto th_end;
    }

    ret = __write_kv(ctx, th, be_path, "frontend", fe_path);
    if (ret) {
        goto th_end;
    }

    ret = __write_kv(ctx, th, be_path, "frontend-id", fe_id_str);
    if (ret) {
        goto th_end;
    }

    ret = __write_kv(ctx, th, be_path, "online", "1");
    if (ret) {
        goto th_end;
    }

    if (vif->bridge) {
        ret = __write_kv(ctx, th, be_path, "bridge", vif->bridge);
        if (ret) {
            goto th_end;
        }
    }

    ret = __write_kv(ctx, th, be_path, "ip", inet_ntoa(vif->ip));
    if (ret) {
        goto th_end;
    }

    ret = __write_kv(ctx, th, be_path, "handle", dev_id_str);
    if (ret) {
        goto th_end;
    }

    ret = __write_kv(ctx, th, be_path, "mac", mac_str);
    if (ret) {
        goto th_end;
    }

    ret = __write_kv(ctx, th, be_path, "state", "1");
    if (ret) {
        goto th_end;
    }

    ret = __write_kv(ctx, th, be_path, "type", "vif");
    if (ret) {
        goto th_end;
    }

    ret = __write_kv(ctx, th, be_path, "hotplug-status", "");
    if (ret) {
        goto th_end;
    }

    ret = __write_kv(ctx, th, be_path, "script", vif->script ? vif->script : "");
    if (ret) {
        goto th_end;
    }

th_end:
    if (ret) {
        xs_transaction_end(ctx->xs.xsh, th, true);
    } else {
        if (!xs_transaction_end(ctx->xs.xsh, th, false)) {
            if (errno == EAGAIN) {
                goto th_start;
            } else {
                ret = errno;
            }
        }
    }

    free(dev_id_str);
    free(mac_str);
    free(fe_path);
    free(fe_id_str);
    free(be_dom_path);
    free(be_path);
    free(be_id_str);
out:
    return ret;
}

int h2_xen_xs_vif_destroy(h2_xen_ctx* ctx, h2_guest* guest, h2_xen_dev_vif* vif)
{
    int ret;

    xs_transaction_t th;

    char* fe_dev_path;
    char* be_dev_path;

    ret = __guest_pre(ctx, guest);
    if (ret) {
        goto out;
    }

    asprintf(&fe_dev_path, "%s/device/%s/%d", guest->hyp.info.xen->xs.dom_path, "vif", vif->id);
    ret = __read_kv(ctx, XBT_NULL, fe_dev_path, "backend", &be_dev_path);
    if (ret) {
        goto out_fe;
    }

th_start:
    ret = 0;
    th = xs_transaction_start(ctx->xs.xsh);

    if (!xs_rm(ctx->xs.xsh, th, fe_dev_path)) {
        ret = errno;
        goto th_end;
    }

    if (!xs_rm(ctx->xs.xsh, th, be_dev_path)) {
        ret = errno;
        goto th_end;
    }

th_end:
    if (ret) {
        xs_transaction_end(ctx->xs.xsh, th, true);
    } else {
        if (!xs_transaction_end(ctx->xs.xsh, th, false)) {
            if (errno == EAGAIN) {
                goto th_start;
            } else {
                ret = errno;
            }
        }
    }

    free(be_dev_path);
out_fe:
    free(fe_dev_path);
out:
    return ret;
}
