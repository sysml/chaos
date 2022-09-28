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
#include <xenevtchn.h>


static int __guest_pre(h2_xen_ctx* ctx, h2_guest* guest)
{
    char* dom_path;

    if (!guest->hyp.guest.xen->priv.xs.active) {
        return EINVAL;
    }

    if (guest->hyp.guest.xen->priv.xs.dom_path == NULL) {
        dom_path = xs_get_domain_path(ctx->xs.xsh, guest->id);
        if (dom_path == NULL) {
            return errno;
        }

        guest->hyp.guest.xen->priv.xs.dom_path = dom_path;
    }

    return 0;
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

    dom_path = guest->hyp.guest.xen->priv.xs.dom_path;

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

    /* Domain has a console so lets add it as the default console. */

    guest->hyp.guest.xen->console.active = true;

    /* FIXME: Assuming console backend is Domain-0 */
    guest->hyp.guest.xen->console.be_id = 0;
    guest->hyp.guest.xen->console.meth = h2_xen_dev_meth_t_xs;

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

    asprintf(&fe_path, "%s/device/%s", guest->hyp.guest.xen->priv.xs.dom_path, "vif");

    xs_list = xs_directory(ctx->xs.xsh, XBT_NULL, fe_path, &xs_list_num);
    if (xs_list == NULL) {
        /* List is empty. */
        goto out_path;
    }

    idx = 0;
    for (int i = 0; i < xs_list_num; i++) {
        char* be_dev_path;
        char* be_id_str;
        char* ip_str;
        char* mac_str;
        char* bridge_str;

        dev = h2_xen_dev_get_next(guest, h2_xen_dev_t_none, &idx);
        if (!dev) {
            ret = ENOMEM;
            break;
        }

        asprintf(&fe_dev_path, "%s/%s", fe_path, xs_list[i]);

        ret = __read_kv(ctx, XBT_NULL, fe_dev_path, "backend", &be_dev_path);
        if (ret) {
            goto free_fe_dev_path;
        }

        ret = __read_kv(ctx, XBT_NULL, fe_dev_path, "backend-id", &be_id_str);
        if (ret) {
            goto free_be_dev_path;
        }

        ret = __read_kv(ctx, XBT_NULL, be_dev_path, "ip", &ip_str);
        if (ret) {
            goto free_be_id;
        }

        ret = __read_kv(ctx, XBT_NULL, be_dev_path, "mac", &mac_str);
        if (ret) {
            goto free_ip;
        }

        ret = __read_kv(ctx, XBT_NULL, be_dev_path, "bridge", &bridge_str);
        if (ret) {
            goto free_mac;
        }

        dev->type = h2_xen_dev_t_vif;
        dev->dev.vif.id = atoi(xs_list[i]);
        dev->dev.vif.valid = true;
        dev->dev.vif.meth = h2_xen_dev_meth_t_xs;
        dev->dev.vif.backend_id = atoi(be_id_str);

        if (inet_aton(ip_str, &dev->dev.vif.ip) == 0) {
            goto free_bridge;
        }

        ret = sscanf(mac_str, "%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8,
                     &dev->dev.vif.mac[0], &dev->dev.vif.mac[1], &dev->dev.vif.mac[2],
                     &dev->dev.vif.mac[3], &dev->dev.vif.mac[4], &dev->dev.vif.mac[5]);
        if (ret != 6) {
            goto free_bridge;
        }

        dev->dev.vif.bridge = strdup(bridge_str);
        dev->dev.vif.script = NULL;

free_bridge:
        free(bridge_str);
free_mac:
        free(mac_str);
free_ip:
        free(ip_str);
free_be_id:
        free(be_id_str);
free_be_dev_path:
        free(be_dev_path);
free_fe_dev_path:
        free(fe_dev_path);
    }

    free(xs_list);

out_path:
    free(fe_path);

    return ret;
}

static int __enumerate_vbd(h2_xen_ctx* ctx, h2_guest* guest)
{
    int ret;

    char** xs_list;
    unsigned int xs_list_num;

    char* fe_dev_path;
    char* fe_path;

    int idx;
    h2_xen_dev* dev;

    ret = 0;

    asprintf(&fe_path, "%s/device/%s", guest->hyp.guest.xen->priv.xs.dom_path, "vbd");

    xs_list = xs_directory(ctx->xs.xsh, XBT_NULL, fe_path, &xs_list_num);
    if (xs_list == NULL) {
        /* List is empty. */
        goto out_path;
    }

    idx = 0;
    for (int i = 0; i < xs_list_num; i++) {
        char* toolstack;
        char* be_dev_path;
        char* be_id_str;

        dev = h2_xen_dev_get_next(guest, h2_xen_dev_t_none, &idx);
        if (!dev) {
            ret = ENOMEM;
            break;
        }

        asprintf(&fe_dev_path, "%s/%s", fe_path, xs_list[i]);

        ret = __read_kv(ctx, XBT_NULL, fe_dev_path, "backend", &be_dev_path);
        if (ret) {
            goto free_fe_dev_path;
        }

        ret = __read_kv(ctx, XBT_NULL, fe_dev_path, "backend-id", &be_id_str);
        if (ret) {
            goto free_be_dev_path;
        }

        /* Check if created by chaos */
        ret = __read_kv(ctx, XBT_NULL, be_dev_path, "toolstack", &toolstack);
        if (ret) {
            goto free_be_id;
        }
        if (toolstack == NULL || strcmp(toolstack, "chaos") != 0) {
            ret = EINVAL;
            goto free_toolstack;
        }

        dev->type = h2_xen_dev_t_vbd;
        dev->dev.vbd.id = atoi(xs_list[i]);
        dev->dev.vbd.valid = true;
        dev->dev.vbd.meth = h2_xen_dev_meth_t_xs;
        dev->dev.vbd.backend_id = atoi(be_id_str);

        ret = __read_kv(ctx, XBT_NULL, be_dev_path, "params", &dev->dev.vbd.target);
        if (ret) {
            goto free_be_id;
        }

        ret = __read_kv(ctx, XBT_NULL, be_dev_path, "type", &dev->dev.vbd.target_type);
        if (ret) {
            goto free_be_id;
        }

        ret = __read_kv(ctx, XBT_NULL, be_dev_path, "dev", &dev->dev.vbd.vdev);
        if (ret) {
            goto free_be_id;
        }

        ret = __read_kv(ctx, XBT_NULL, be_dev_path, "mode", &dev->dev.vbd.access);
        if (ret) {
            goto free_be_id;
        }

        ret = __read_kv(ctx, XBT_NULL, be_dev_path, "script", &dev->dev.vbd.script);
        if (ret) {
            goto free_be_id;
        }

free_toolstack:
        free(toolstack);
free_be_id:
        free(be_id_str);
free_be_dev_path:
        free(be_dev_path);
free_fe_dev_path:
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


void h2_xen_xs_priv_free(h2_xen_guest* guest)
{
    if (guest->priv.xs.dom_path) {
        free(guest->priv.xs.dom_path);
        guest->priv.xs.dom_path = NULL;
    }
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

    dom_path = guest->hyp.guest.xen->priv.xs.dom_path;

    asprintf(&domid_str, "%u", (unsigned int) guest->id);
    asprintf(&data_path, "%s/data", dom_path);
    asprintf(&shutdown_path, "%s/control/shutdown", dom_path);

th_start:
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

    if (!xs_rm(ctx->xs.xsh, XBT_NULL, guest->hyp.guest.xen->priv.xs.dom_path)) {
        ret = errno;
    }

out:
    return ret;
}

int h2_xen_xs_domain_intro(h2_xen_ctx* ctx, h2_guest* guest,
        evtchn_port_t evtchn, unsigned int gmfn)
{
    int ret;

    ret = __guest_pre(ctx, guest);
    if (ret) {
        goto out;
    }

    if (!xs_introduce_domain(ctx->xs.xsh, guest->id, gmfn, evtchn)) {
        ret = errno;
    }

out:
    return ret;
}

static int __xs_domain_pwrctl(h2_xen_ctx* ctx, h2_guest* guest,
        char* cmd, char* token)
{
    int ret;
    char* dom_path = NULL;
    char* shutdown_path = NULL;
    xs_transaction_t th;

    ret = __guest_pre(ctx, guest);
    if (ret) {
        goto out;
    }

    dom_path = guest->hyp.guest.xen->priv.xs.dom_path;

th_start:
    th = xs_transaction_start(ctx->xs.xsh);

    ret = __write_kv(ctx, th, dom_path, "control/shutdown", cmd);
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

out:
    if (shutdown_path) {
        free(shutdown_path);
    }
    return ret;
}

int h2_xen_xs_shutdown_ctx_close(h2_xen_xs_shutdown_ctx* sctx)
{
    if (sctx->shutdown_path) {
        free(sctx->shutdown_path);
        sctx->shutdown_path = NULL;
    }
    if (sctx->token) {
        free(sctx->token);
        sctx->token = NULL;
    }

    return 0;
}

int h2_xen_xs_shutdown_ctx_open(h2_xen_xs_shutdown_ctx* sctx,
        h2_shutdown_reason reason,
        h2_xen_ctx* ctx, h2_guest* guest,
        h2_query_callback_t query_func, bool wait)
{
    int ret;

    ret = 0;

    memset(sctx, 0, sizeof(*sctx));

    switch (reason) {
        case h2_shutdown_poweroff:
            sctx->reason = "poweroff";
            break;
        case h2_shutdown_suspend:
            sctx->reason = "suspend";
            break;
        default:
            ret = EINVAL;
            goto out_err;
    }

    sctx->pollfd.fd = xs_fileno(ctx->xs.xsh);
    if (sctx->pollfd.fd < 0) {
        ret = errno;
        goto out_close;
    }
    sctx->pollfd.events = POLLIN | POLLPRI;

    sctx->query_func = query_func;
    sctx->wait = wait;


    asprintf(&sctx->token, "chaos-%lu", guest->id);
    if (sctx->token == NULL) {
        ret = errno;
        goto out_err;
    }

    return 0;

out_close:
    h2_xen_xs_shutdown_ctx_close(sctx);
out_err:
    return ret;
}

struct h2_xs_watch {
    char* path;
    bool initialized;
    int skip_events_num;
};

int h2_xen_xs_domain_shutdown(h2_xen_ctx* ctx, h2_guest* guest,
        h2_xen_xs_shutdown_ctx* sctx)
{
    int ret;
    int timeout_ms, dec_ms;
    bool acked, released;

    char* dom_path;
    char* shutdown_path;
    struct h2_xs_watch watches[2];
    char** retw;


    dom_path = guest->hyp.guest.xen->priv.xs.dom_path;

    asprintf(&shutdown_path, "%s/%s", dom_path, "control/shutdown");

    memset(watches, 0, sizeof(watches));

    /* watch shutdown path for guest ack */
    watches[0].path = shutdown_path;
    watches[0].skip_events_num = 1;
    ret = xs_watch(ctx->xs.xsh, watches[0].path, sctx->token);
    if (ret == false) {
        ret = errno;
        goto out_ret;
    }
    /* watch @releaseDomain for guest shutdown completion */
    watches[1].path = "@releaseDomain";
    ret = xs_watch(ctx->xs.xsh, watches[1].path, sctx->token);
    if (ret == false) {
        ret = errno;
        goto out_unwatch0;
    }

    /* trigger shutdown */
    ret = __xs_domain_pwrctl(ctx, guest, sctx->reason, sctx->token);
    if (ret) {
        ret = errno;
        goto out_unwatch1;
    }

    if (!sctx->wait) {
        goto out_unwatch1;
    }


    timeout_ms = 60 * 1000;
    dec_ms = 1;

    acked = false;
    released = false;

    while (timeout_ms > 0) {
        ret = sctx->query_func(ctx, guest);
        if (ret) {
            goto out_unwatch1;
        }

        if (guest->shutdown) {
            break;

        } else if (released) {
            ret = -1; /* TODO this is a fatal one */
            goto out_unwatch1;
        }

        ret = poll(&sctx->pollfd, 1, dec_ms);
        if (ret < 0) {
            ret = errno;
            goto out_unwatch1;
        }

        if (ret > 0) {
            /* We wait one event */
            if (ret != 1 || (sctx->pollfd.revents & POLLIN) == 0) {
                ret = errno;
                goto out_unwatch1;
            }

            retw = xs_check_watch(ctx->xs.xsh);
            if (!retw) {
                if (errno == EAGAIN || errno == EINTR)
                    continue;

                ret = errno;
                goto out_unwatch1;
            }

            for (int i = 0; i < 2; i++) {
                struct h2_xs_watch* w = &watches[i];

                if (strcmp(retw[0], w->path) || strcmp(retw[1], sctx->token))
                    continue;

                if (!w->initialized) {
                    /* received spurious event */
                    w->initialized = true;

                } else if (w->skip_events_num) {
                    /* watch generated by our write */
                    w->skip_events_num--;

                } else {
                    if (strcmp(w->path, shutdown_path) == 0) {
                        /* guest ack'd */
                        acked = true;

                    } else if (strcmp(w->path, "@releaseDomain") == 0) {
                        if (acked) {
                            released = true;
                        }
                        /* else guest crashed */
                    }
                }
            }

            free(retw);

            if (ret < 0) {
                goto out_ret;
            }
        }

        timeout_ms -= dec_ms;
    }

    ret = 0;

out_unwatch1:
    xs_unwatch(ctx->xs.xsh, watches[1].path, sctx->token);
out_unwatch0:
    xs_unwatch(ctx->xs.xsh, watches[0].path, sctx->token);
out_ret:
    if (shutdown_path) {
        free(shutdown_path);
    }

    return ret;
}

int h2_xen_xs_probe_guest(h2_xen_ctx* ctx, h2_guest* guest)
{
    int ret;

    char* dom_path;
    char* xs_val;
    unsigned int xs_val_len;

    ret = 0;

    guest->hyp.guest.xen->priv.xs.active = false;
    if (guest->hyp.guest.xen->priv.xs.dom_path) {
        free(guest->hyp.guest.xen->priv.xs.dom_path);
        guest->hyp.guest.xen->priv.xs.dom_path = NULL;
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
        guest->hyp.guest.xen->priv.xs.active = false;
        guest->hyp.guest.xen->xs.active = false;
        free(dom_path);
    } else {
        guest->hyp.guest.xen->priv.xs.active = true;
        guest->hyp.guest.xen->priv.xs.dom_path = dom_path;
        guest->hyp.guest.xen->xs.active = true;
        free(xs_val);

        ret = __read_kv(ctx, XBT_NULL, dom_path, "name", &guest->name);
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

    ret = __enumerate_vbd(ctx, guest);
    if (ret) {
        goto out;
    }

out:
    return ret;
}

int h2_xen_xs_console_create(h2_xen_ctx* ctx, h2_guest* guest,
        evtchn_port_t evtchn, unsigned int gmfn)
{
    int ret;

    xs_transaction_t th;

    char* console_path;
    char* type_val;
    char* ringref_val;
    char* evtchn_val;

    struct xs_permissions dom_rw[1];

    ret = __guest_pre(ctx, guest);
    if (ret) {
        goto out;
    }

    dom_rw[0].id = guest->id;
    dom_rw[0].perms = XS_PERM_NONE;

    asprintf(&console_path, "%s/console", guest->hyp.guest.xen->priv.xs.dom_path);
    asprintf(&ringref_val, "%u", gmfn);
    asprintf(&evtchn_val, "%u", evtchn);
    type_val = "xenconsoled";

th_start:
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

    ret = __write_kv(ctx, th, console_path, "ring-ref", ringref_val);
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
    free(ringref_val);
    free(evtchn_val);
out:
    return ret;
}

int h2_xen_xs_console_destroy(h2_xen_ctx* ctx, h2_guest* guest)
{
    int ret;

    char* console_path;

    ret = __guest_pre(ctx, guest);
    if (ret) {
        goto out;
    }

    asprintf(&console_path, "%s/console", guest->hyp.guest.xen->priv.xs.dom_path);

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
    asprintf(&mac_str, "%02"SCNx8":%02"SCNx8":%02"SCNx8":%02"SCNx8":%02"SCNx8":%02"SCNx8,
            vif->mac[0], vif->mac[1], vif->mac[2], vif->mac[3], vif->mac[4], vif->mac[5]);

    fe_dom_path = guest->hyp.guest.xen->priv.xs.dom_path;
    asprintf(&fe_path, "%s/device/%s/%s", fe_dom_path, "vif", dev_id_str);
    asprintf(&fe_id_str, "%u", (domid_t) guest->id);

    be_dom_path = xs_get_domain_path(ctx->xs.xsh, vif->backend_id);
    asprintf(&be_path, "%s/backend/%s/%u/%d", be_dom_path, "vif", (domid_t) guest->id, vif->id);
    asprintf(&be_id_str, "%d", vif->backend_id);

th_start:
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

    asprintf(&fe_dev_path, "%s/device/%s/%d",
            guest->hyp.guest.xen->priv.xs.dom_path, "vif", vif->id);

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

int h2_xen_xs_vbd_create(h2_xen_ctx* ctx, h2_guest* guest, h2_xen_dev_vbd* vbd)
{
    int ret;

    xs_transaction_t th;

    char* dev_id_str;
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
    fe_perms[1].id = vbd->backend_id;
    fe_perms[1].perms = XS_PERM_READ;

    be_perms[0].id = vbd->backend_id;
    be_perms[0].perms = XS_PERM_NONE;
    be_perms[1].id = guest->id;
    be_perms[1].perms = XS_PERM_READ;

    asprintf(&dev_id_str, "%d", vbd->id);

    fprintf(stderr, "h2_xen_xs_vbd_create\n");

    fe_dom_path = guest->hyp.guest.xen->priv.xs.dom_path;
    asprintf(&fe_path, "%s/device/%s/%s", fe_dom_path, "vbd", dev_id_str);
    asprintf(&fe_id_str, "%u", (domid_t) guest->id);

    be_dom_path = xs_get_domain_path(ctx->xs.xsh, vbd->backend_id);
    asprintf(&be_path, "%s/backend/%s/%u/%d", be_dom_path, "vbd", (domid_t) guest->id, vbd->id);
    asprintf(&be_id_str, "%d", vbd->backend_id);

th_start:
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

    if (!xs_mkdir(ctx->xs.xsh, th, be_path)) {
        ret = errno;
        goto th_end;
    }

    if (!xs_set_permissions(ctx->xs.xsh, th, be_path, be_perms, 2)) {
        ret = errno;
        goto th_end;
    }

    ret = __write_kv(ctx, th, be_path, "toolstack", "chaos");
    if (ret) {
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

    ret = __write_kv(ctx, th, be_path, "state", "1");
    if (ret) {
        goto th_end;
    }

    ret = __write_kv(ctx, th, be_path, "params", vbd->target);
    if (ret) {
        goto th_end;
    }

    ret = __write_kv(ctx, th, be_path, "type", vbd->target_type);
    if (ret) {
        goto th_end;
    }

    ret = __write_kv(ctx, th, be_path, "dev", vbd->vdev);
    if (ret) {
        goto th_end;
    }

    ret = __write_kv(ctx, th, be_path, "mode", vbd->access);
    if (ret) {
        goto th_end;
    }

    ret = __write_kv(ctx, th, be_path, "hotplug-status", "");
    if (ret) {
        goto th_end;
    }

    ret = __write_kv(ctx, th, be_path, "script", vbd->script ? vbd->script : "");
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
    free(fe_path);
    free(fe_id_str);
    free(be_dom_path);
    free(be_path);
    free(be_id_str);
out:
    return ret;
}

int h2_xen_xs_vbd_destroy(h2_xen_ctx* ctx, h2_guest* guest, h2_xen_dev_vbd* vbd)
{
    int ret;

    xs_transaction_t th;

    char* fe_dev_path;
    char* be_dev_path;

    ret = __guest_pre(ctx, guest);
    if (ret) {
        goto out;
    }

    asprintf(&fe_dev_path, "%s/device/%s/%d",
            guest->hyp.guest.xen->priv.xs.dom_path, "vbd", vbd->id);

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
