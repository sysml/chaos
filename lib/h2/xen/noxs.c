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

#include <h2/xen/noxs.h>

#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <xen/noxs.h>
#include <xen/devctl.h>
#include <xencall.h>
#include <xenforeignmemory.h>


static int __guest_pre(h2_xen_ctx* ctx, h2_guest* guest)
{
    int ret;

    if (ctx == NULL || guest == NULL) {
        ret = EINVAL;
        goto out_err;
    }

    if (!guest->hyp.guest.xen->noxs.active) {
        ret = EINVAL;
        goto out_err;
    }

    return 0;

out_err:
    return ret;
}

static int __dev_append(h2_xen_ctx* ctx, h2_guest* guest, noxs_dev_type_t type,
        noxs_dev_id_t dev_id, domid_t be_id, evtchn_port_t evtchn, grant_ref_t grant)
{
    int ret;
    xen_devctl_t devctl;

    xencall_handle* ch;

    /* FIXME: keep xencall open */
    ch = xencall_open(ctx->xc.xtl, XENCALL_OPENFLAG_NON_REENTRANT);

    devctl.version = XEN_DEVCTL_VERSION;
    devctl.cmd = XEN_DEVCTL_dev_add;

    devctl.domain = guest->id;
    devctl.u.dev_add.dev.type = type;
    devctl.u.dev_add.dev.id = dev_id;
    devctl.u.dev_add.dev.be_id = be_id;
    devctl.u.dev_add.dev.comm.grant = grant;
    devctl.u.dev_add.dev.comm.evtchn = evtchn;

    ret = xencall1(ch, __HYPERVISOR_devctl, (uint64_t)(&devctl));

    xencall_close(ch);

    return ret;
}

static int __dev_remove(h2_xen_ctx* ctx, h2_guest* guest, noxs_dev_type_t type,
        noxs_dev_id_t dev_id)
{
    int ret;
    xen_devctl_t devctl;

    xencall_handle* ch;

    /* FIXME: keep xencall open */
    ch = xencall_open(ctx->xc.xtl, XENCALL_OPENFLAG_NON_REENTRANT);

    devctl.version = XEN_DEVCTL_VERSION;
    devctl.cmd = XEN_DEVCTL_dev_rem;

    devctl.domain = guest->id;
    devctl.u.dev_rem.dev.type = type;
    devctl.u.dev_rem.dev.devid = dev_id;

    ret = xencall1(ch, __HYPERVISOR_devctl, (uint64_t)(&devctl));

    xencall_close(ch);

    return ret;
}

static int __dev_enumerate(h2_xen_ctx* ctx, h2_guest* guest)
{
    int ret;
    xen_devctl_t devctl;
    h2_xen_dev* devs;
    noxs_dev_page_entry_t* dev;

    xencall_handle* ch;

    /* FIXME: keep xencall open */
    ch = xencall_open(ctx->xc.xtl, XENCALL_OPENFLAG_NON_REENTRANT);

    devctl.version = XEN_DEVCTL_VERSION;
    devctl.cmd = XEN_DEVCTL_dev_enum;

    devctl.domain = guest->id;

    ret = xencall1(ch, __HYPERVISOR_devctl, (uint64_t)(&devctl));

    xencall_close(ch);

    if (ret) {
        return ret;
    }

    devs = guest->hyp.guest.xen->devs;
    for (int i = 0, j = 0; i < devctl.u.dev_enum.dev_count; i++) {
        dev = &(devctl.u.dev_enum.devs[i]);

        for (; j < H2_XEN_DEV_COUNT_MAX; j++) {
            if (devs[j].type == h2_xen_dev_t_none) {
                break;
            }
        }

        if (j == H2_XEN_DEV_COUNT_MAX) {
            return ENOBUFS;
        }

        switch (dev->type) {
            case noxs_dev_vif:
                devs[j].type = h2_xen_dev_t_vif;
                devs[j].dev.vif.id = dev->id;
                devs[j].dev.vif.backend_id = dev->be_id;

                devs[j].dev.vif.meth = h2_xen_dev_meth_t_noxs;
                devs[j].dev.vif.valid = true;
                break;

            default:
                break;
        }
    }

    return ret;
}

int h2_xen_noxs_open(h2_xen_ctx* ctx)
{
    int ret;

    if (ctx == NULL) {
        ret = EINVAL;
        goto out_err;
    }

    if (ctx->noxs.fd > 0) {
        ret = EINVAL;
        goto out_err;
    }

    ctx->noxs.fd = open("/dev/xen/noxs_backend", O_RDWR|O_CLOEXEC);
    if (ctx->noxs.fd == -1) {
        ret = errno;
        goto out_err;
    }

    return 0;

out_err:
    return ret;
}

int h2_xen_noxs_close(h2_xen_ctx* ctx)
{
    int ret;

    if (ctx == NULL) {
        ret = EINVAL;
        goto out_err;
    }

    if (ctx->noxs.fd <= 0) {
        ret = EINVAL;
        goto out_err;
    }

    ret = close(ctx->noxs.fd);
    if (ret) {
        ret = errno;
        goto out_err;
    }

    return 0;

out_err:
    return ret;
}


int h2_xen_noxs_probe_guest(h2_xen_ctx* ctx, h2_guest* guest)
{
    guest->hyp.guest.xen->noxs.active = true;
    return 0;
}

int h2_xen_noxs_dev_enumerate(h2_xen_ctx* ctx, h2_guest* guest)
{
    int ret;

    ret = __guest_pre(ctx, guest);
    if (ret) {
        goto out_err;
    }

    ret = __dev_enumerate(ctx, guest);
    if (ret) {
        goto out_err;
    }

    return 0;

out_err:
    return ret;
}


int h2_xen_noxs_console_create(h2_xen_ctx* ctx, h2_guest* guest,
        evtchn_port_t evtchn, unsigned int fn)
{
    return ENOSYS;
}

int h2_xen_noxs_console_destroy(h2_xen_ctx* ctx, h2_guest* guest)
{
    return ENOSYS;
}


int h2_xen_noxs_vif_create(h2_xen_ctx* ctx, h2_guest* guest, h2_xen_dev_vif* vif)
{
    int ret;
    struct noxs_ioctl_dev_create ioctlc;
    struct noxs_ioctl_dev_destroy ioctld;

    ret = __guest_pre(ctx, guest);
    if (ret) {
        goto out_err;
    }

    if (vif == NULL) {
        ret = EINVAL;
    }

    ioctlc.type = noxs_user_dev_vif;
    ioctlc.be_id = vif->backend_id;
    ioctlc.fe_id = guest->id;
    ioctlc.devid = vif->id;
    ioctlc.cfg.vif.ip = vif->ip.s_addr;
    memcpy(ioctlc.cfg.vif.mac, vif->mac, sizeof(uint8_t[6]));
    snprintf(ioctlc.cfg.vif.bridge, IFNAMSIZ, "%s", vif->bridge);

    ret = ioctl(ctx->noxs.fd, IOCTL_NOXS_DEV_CREATE, &ioctlc);
    if (ret) {
        goto out_err;
    }

    ret = __dev_append(ctx, guest, noxs_dev_vif,
            ioctlc.devid, ioctlc.be_id, ioctlc.evtchn, ioctlc.grant);
    if (ret) {
        goto out_vif;
    }

    return 0;

out_vif:
    ioctld.type = ioctlc.type;
    ioctld.be_id = ioctlc.be_id;
    ioctld.fe_id = ioctlc.fe_id;
    ioctld.devid = ioctlc.devid;

    ioctl(ctx->noxs.fd, IOCTL_NOXS_DEV_DESTROY, &ioctld);

out_err:
    return ret;
}

int h2_xen_noxs_vif_destroy(h2_xen_ctx* ctx, h2_guest* guest, h2_xen_dev_vif* vif)
{
    int ret;
    struct noxs_ioctl_dev_destroy ioctld;

    ret = __guest_pre(ctx, guest);
    if (ret) {
        goto out_err;
    }

    if (vif == NULL) {
        ret = EINVAL;
    }

    ioctld.type = noxs_user_dev_vif;
    ioctld.be_id = vif->backend_id;
    ioctld.fe_id = guest->id;
    ioctld.devid = vif->id;

    ret = ioctl(ctx->noxs.fd, IOCTL_NOXS_DEV_DESTROY, &ioctld);
    if (ret) {
        goto out_err;
    }

    __dev_remove(ctx, guest, noxs_dev_vif, vif->id);

    return 0;

out_err:
    return ret;
}
