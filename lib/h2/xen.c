/*
 * chaos
 *
 * Authors: Filipe Manco <filipe.manco@neclab.eu>
 *          Florian Schmidt <florian.schmidt@neclab.eu>
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

#include <h2/xen.h>
#include <h2/xen/console.h>
#include <h2/xen/dev.h>
#ifdef CONFIG_H2_XEN_NOXS
#include <h2/xen/noxs.h>
#endif
#include <h2/xen/xc.h>
#include <h2/xen/xs.h>

#include <xc_dom.h>


int h2_xen_open(h2_xen_ctx** ctx, h2_xen_cfg* cfg)
{
    int ret;

    if (ctx == NULL || cfg == NULL) {
        ret = EINVAL;
        goto out_err;
    }

    (*ctx) = (h2_xen_ctx*) calloc(1, sizeof(h2_xen_ctx));
    if ((*ctx) == NULL) {
        ret = errno;
        goto out_err;
    }

    (*ctx)->xlib = cfg->xlib;
    switch ((*ctx)->xlib) {
        case h2_xen_xlib_t_xc:
            ret = h2_xen_xc_open(*ctx, cfg);
            break;
    }
    if (ret) {
        goto out_mem;
    }

    if (cfg->xs.active) {
        (*ctx)->xs.active = true;
        (*ctx)->xs.domid = cfg->xs.domid;

        ret = h2_xen_xs_open(*ctx);
        if (ret) {
            goto out_xlib;
        }
    }

#ifdef CONFIG_H2_XEN_NOXS
    if (cfg->noxs.active) {
        (*ctx)->noxs.active = true;

        ret = h2_xen_noxs_open(*ctx);
        if (ret) {
            goto out_xs;
        }
    }
#endif

    return 0;

#ifdef CONFIG_H2_XEN_NOXS
out_xs:
    if ((*ctx)->xs.active) {
        h2_xen_xs_close(*ctx);
    }
#endif

out_xlib:
    switch ((*ctx)->xlib) {
        case h2_xen_xlib_t_xc:
            h2_xen_xc_close(*ctx);
            break;
    }

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

#ifdef CONFIG_H2_XEN_NOXS
    if ((*ctx)->noxs.active) {
        h2_xen_noxs_close(*ctx);
    }
#endif

    if ((*ctx)->xs.active) {
        h2_xen_xs_close(*ctx);
    }

    switch ((*ctx)->xlib) {
        case h2_xen_xlib_t_xc:
            h2_xen_xc_close(*ctx);
            break;
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

int h2_xen_guest_query(h2_xen_ctx* ctx, h2_guest* guest)
{
    int ret;

    if (ctx == NULL || guest == NULL) {
        ret = EINVAL;
        goto out_err;
    }

    ret = h2_xen_xc_domain_query(ctx, guest);
    if (ret) {
        goto out_err;
    }

    if (ctx->xs.active) {
        h2_xen_xs_probe_guest(ctx, guest);
    }

#ifdef CONFIG_H2_XEN_NOXS
    if (ctx->noxs.active) {
        h2_xen_noxs_probe_guest(ctx, guest);
    }
#endif

    h2_xen_dev_enumerate(ctx, guest);

    return 0;

out_err:
    return ret;
}

void h2_xen_guest_reuse(h2_xen_guest* guest)
{
    if (guest == NULL) {
        return;
    }

    for (int i = 0; i < H2_XEN_DEV_COUNT_MAX; i++) {
        h2_xen_dev_reuse(&(guest->devs[i]));
    }

    h2_xen_xs_priv_free(guest);

    h2_xen_xc_priv_free(guest);
}

void h2_xen_guest_free(h2_xen_guest** guest)
{
    if (guest == NULL || (*guest) == NULL) {
        return;
    }

    for (int i = 0; i < H2_XEN_DEV_COUNT_MAX; i++) {
        h2_xen_dev_free(&((*guest)->devs[i]));
    }

    h2_xen_xs_priv_free((*guest));

    h2_xen_xc_priv_free((*guest));

    free(*guest);
    (*guest) = NULL;
}


int h2_xen_guest_list(h2_xen_ctx* ctx, struct guestq* guests)
{
    int ret;

    h2_guest* guest;

    ret = h2_xen_xc_domain_list(ctx, guests);
    if (ret) {
        goto out_err;
    }

    TAILQ_FOREACH(guest, guests, list) {
        if (ctx->xs.active) {
            h2_xen_xs_probe_guest(ctx, guest);
        }

#ifdef CONFIG_H2_XEN_NOXS
        if (ctx->noxs.active) {
            h2_xen_noxs_probe_guest(ctx, guest);
        }
#endif

        h2_xen_dev_enumerate(ctx, guest);
    }

    return 0;

out_err:
    return ret;
}


int h2_xen_domain_precreate(h2_xen_ctx* ctx, h2_guest* guest)
{
    int ret;
    h2_xen_guest* xguest;

    if (ctx == NULL || guest == NULL) {
        ret = EINVAL;
        goto out_err;
    }

    xguest = guest->hyp.guest.xen;

    if (xguest == NULL) {
        ret = EINVAL;
        goto out_err;
    }

    xguest->priv.xs.active = (ctx->xs.active && xguest->xs.active);

    switch (ctx->xlib) {
        case h2_xen_xlib_t_xc:
            ret = h2_xen_xc_domain_create(ctx, guest);
            if (ret) {
                goto out_err;
            }
            break;
    }

    switch (ctx->xlib) {
        case h2_xen_xlib_t_xc:
            ret = h2_xen_xc_domain_preinit(ctx, guest);
            if (ret) {
                goto out_dom;
            }
            break;
    }

    if (xguest->priv.xs.active) {
        ret = h2_xen_xs_domain_create(ctx, guest);
        if (ret) {
            goto out_dom;
        }
    }

    return 0;

out_dom:
    switch (ctx->xlib) {
        case h2_xen_xlib_t_xc:
            h2_xen_xc_domain_destroy(ctx, guest);
            break;
    }

out_err:
    return ret;
}

int h2_xen_domain_fastboot(h2_xen_ctx* ctx, h2_guest* guest)
{
    int ret;
    h2_xen_guest* xguest;

    if (ctx == NULL || guest == NULL) {
        ret = EINVAL;
        goto out_err;
    }

    xguest = guest->hyp.guest.xen;
    if (xguest == NULL) {
        ret = EINVAL;
        goto out_err;
    }

    switch (ctx->xlib) {
        case h2_xen_xlib_t_xc:
            if (guest->kernel.type == h2_kernel_buff_t_none) {
                ret = h2_xen_xc_domain_restore(ctx, guest);
            } else {
                ret = h2_xen_xc_domain_fastboot(ctx, guest);
            }
            if (ret) {
                goto out_dom;
            }
            break;
    }

    if (xguest->console.active) {
        ret = h2_xen_console_create(ctx, guest,
                xguest->priv.console.evtchn, xguest->priv.console.gmfn);
        if (ret) {
            goto out_dom;
        }
    }

    if (xguest->priv.xs.active) {
        ret = h2_xen_xs_domain_intro(ctx, guest,
                xguest->priv.xs.evtchn, xguest->priv.xs.gmfn);
        if (ret) {
            goto out_console;
        }
    }

    if (!guest->paused) {
        switch (ctx->xlib) {
            case h2_xen_xlib_t_xc:
                ret = h2_xen_xc_domain_unpause(ctx, guest);
                break;
        }
    }
    if (ret) {
        goto out_xs;
    }

    return 0;

out_xs:
    if (xguest->priv.xs.active) {
        h2_xen_xs_domain_destroy(ctx, guest);
    }

out_console:
    if (xguest->console.active) {
        h2_xen_console_destroy(ctx, guest);
    }

out_dom:
    switch (ctx->xlib) {
        case h2_xen_xlib_t_xc:
            h2_xen_xc_domain_destroy(ctx, guest);
            break;
    }

out_err:
    return ret;
}

int h2_xen_domain_create(h2_xen_ctx* ctx, h2_guest* guest)
{
    int ret;

    ret = h2_xen_domain_precreate(ctx, guest);
    if (ret) {
        goto out_err;
    }

    ret = 0;
    for (int i = 0; i < H2_XEN_DEV_COUNT_MAX && !ret; i++) {
        ret = h2_xen_dev_create(ctx, guest, &(guest->hyp.guest.xen->devs[i]));
    }
    if (ret) {
        goto out_dev;
    }

    ret = h2_xen_domain_fastboot(ctx, guest);
    if (ret) {
        goto out_dev;
    }

    return 0;

out_dev:
    for (int i = 0; i < H2_XEN_DEV_COUNT_MAX; i++) {
        h2_xen_dev_destroy(ctx, guest, &(guest->hyp.guest.xen->devs[i]));
    }

    h2_xen_domain_destroy(ctx, guest);

out_err:
    return ret;
}

int h2_xen_domain_destroy(h2_xen_ctx* ctx, h2_guest* guest)
{
    int ret;
    int _ret;

    if (ctx == NULL || guest == NULL) {
        return EINVAL;
    }

    ret = 0;

    for (int i = 0; i < H2_XEN_DEV_COUNT_MAX; i++) {
        _ret = h2_xen_dev_destroy(ctx, guest, &(guest->hyp.guest.xen->devs[i]));
        if (_ret && !ret) {
            ret = _ret;
        }
    }

    if (guest->hyp.guest.xen->console.active) {
        _ret = h2_xen_console_destroy(ctx, guest);
        if (_ret && !ret) {
            ret = _ret;
        }
    }

    if (ctx->xs.active && guest->hyp.guest.xen->xs.active) {
        _ret = h2_xen_xs_domain_destroy(ctx, guest);
        if (_ret && !ret) {
            ret = _ret;
        }
    }

    _ret = h2_xen_xc_domain_destroy(ctx, guest);
    if (_ret && !ret) {
        ret = _ret;
    }


    return ret;
}

int h2_xen_domain_shutdown(h2_xen_ctx* ctx, h2_guest* guest, bool wait)
{
    int ret;
    int _ret;

    if (ctx == NULL || guest == NULL) {
        return EINVAL;
    }

    ret = 0;

#ifdef CONFIG_H2_XEN_NOXS
    if (ctx->noxs.active && guest->hyp.guest.xen->noxs.active) {
        h2_xen_noxs_shutdown_ctx noxs_sctx;

        ret = h2_xen_noxs_shutdown_ctx_open(&noxs_sctx, h2_shutdown_poweroff,
                h2_xen_xc_domain_query, wait);
        if (ret) {
            ret = errno;
            goto out_ret;
        }

        ret = h2_xen_noxs_domain_shutdown(ctx, guest, &noxs_sctx);

        _ret = h2_xen_noxs_shutdown_ctx_close(&noxs_sctx);
        if (_ret && !ret) {
            ret = _ret;
        }

    } else if (ctx->xs.active && guest->hyp.guest.xen->xs.active)
#endif
    {
        h2_xen_xs_shutdown_ctx xs_sctx;

        ret = h2_xen_xs_shutdown_ctx_open(&xs_sctx, h2_shutdown_poweroff,
                ctx, guest, h2_xen_xc_domain_query, wait);
        if (ret) {
            ret = errno;
            goto out_ret;
        }

        ret = h2_xen_xs_domain_shutdown(ctx, guest, &xs_sctx);

        _ret = h2_xen_xs_shutdown_ctx_close(&xs_sctx);
        if (_ret && !ret) {
            ret = _ret;
        }
    }

out_ret:
    return ret;
}

int h2_xen_domain_save(h2_xen_ctx* ctx, h2_guest* guest, bool wait)
{
    int ret;
    int _ret;

    if (ctx == NULL || guest == NULL) {
        return EINVAL;
    }

    ret = 0;

#ifdef CONFIG_H2_XEN_NOXS
    if (ctx->noxs.active && guest->hyp.guest.xen->noxs.active) {
        h2_xen_noxs_shutdown_ctx noxs_sctx;

        ret = h2_xen_noxs_shutdown_ctx_open(&noxs_sctx, h2_shutdown_suspend,
                h2_xen_xc_domain_query, wait);
        if (ret) {
            ret = errno;
            goto out_ret;
        }

        ret = h2_xen_xc_domain_save(ctx, guest,
                (h2_shutdown_callback_t) h2_xen_noxs_domain_shutdown, &noxs_sctx);

        _ret = h2_xen_noxs_shutdown_ctx_close(&noxs_sctx);
        if (_ret && !ret) {
            ret = _ret;
        }


    } else if (ctx->xs.active && guest->hyp.guest.xen->xs.active)
#endif
    {
        h2_xen_xs_shutdown_ctx xs_sctx;

        ret = h2_xen_xs_shutdown_ctx_open(&xs_sctx, h2_shutdown_suspend,
                ctx, guest, h2_xen_xc_domain_query, wait);
        if (ret) {
            ret = errno;
            goto out_ret;
        }

        ret = h2_xen_xc_domain_save(ctx, guest,
                (h2_shutdown_callback_t) h2_xen_xs_domain_shutdown, &xs_sctx);

        _ret = h2_xen_xs_shutdown_ctx_close(&xs_sctx);
        if (_ret && !ret) {
            ret = _ret;
        }

    }

out_ret:
    return ret;
}

int h2_xen_domain_resume(h2_xen_ctx* ctx, h2_guest* guest)
{
    return h2_xen_xc_domain_resume(ctx, guest);
}
