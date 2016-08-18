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

#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <xc_dom.h>


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

    free(*guest);
    (*guest) = NULL;
}


static h2_xen_dev* __xen_get_next_dev(h2_guest* guest, enum h2_xen_dev_t type, int* idx)
{
    for (; (*idx) < H2_XEN_DEV_COUNT_MAX; (*idx)++) {
        if (guest->hyp.info.xen->devs[(*idx)].type == type) {
            return &(guest->hyp.info.xen->devs[(*idx)]);
        }
    }

    return NULL;
}

static int __xc_domain_create(h2_xen_ctx* ctx, h2_guest* guest)
{
    int ret;

    uint32_t domid;
    uint32_t flags;
    xen_domain_handle_t dom_handle;
    xc_domain_configuration_t dom_config;

    /* NOTE: H2 only supports PV or PVH guests */

    /* TODO: Support PVH */
    flags = 0;
    dom_config.emulation_flags = 0;

    /* FIXME: what is the ssidref parameter? */
    ret = xc_domain_create(ctx->xci, 0, dom_handle, flags, &domid, &dom_config);
    if (ret) {
        goto out_err;
    }

    ret = xc_domain_max_vcpus(ctx->xci, domid, guest->vcpus.count);
    if (ret) {
        goto out_dom;
    }

    /* TODO: Support CPU pools */
    ret = xc_cpupool_movedomain(ctx->xci, 0, domid);
    if (ret) {
        goto out_dom;
    }

    int cpu_max;
    xc_cpumap_t cpu_map;

    cpu_max = xc_get_max_cpus(ctx->xci);

    /* TODO: Support vCPU pinning. For now pinning to all available CPUs. */
    for (int vcpu = 0; vcpu < guest->vcpus.count; vcpu++) {
        cpu_map = xc_cpumap_alloc(ctx->xci);

        for (int cpu = 0; cpu < cpu_max; cpu ++) {
            xc_cpumap_setcpu(cpu, cpu_map);
        }

        ret = xc_vcpu_setaffinity(ctx->xci, domid, vcpu, cpu_map, NULL, XEN_VCPUAFFINITY_HARD);
        free(cpu_map);

        if (ret) {
            goto out_dom;
        }
    }

    /* FIXME: Figure how this memory calls actually work */
    ret = xc_domain_setmaxmem(ctx->xci, domid, guest->memory);
    if (ret) {
        goto out_dom;
    }

    ret = xc_domain_set_memmap_limit(ctx->xci, domid, guest->memory);
    if (ret) {
        goto out_dom;
    }

    /* FIXME: Set proper TSC Mode */
    ret = xc_domain_set_tsc_info(ctx->xci, domid, 0, 0, 0, 0);
    if (ret) {
        goto out_dom;
    }


    guest->id = domid;

    return 0;

out_dom:
    xc_domain_destroy(ctx->xci, domid);

out_err:
    return ret;
}

static int __xc_domain_init(h2_xen_ctx* ctx, h2_guest* guest,
        h2_xen_dev_xenstore* xenstore, h2_xen_dev_console* console)
{
    int ret;

    struct xc_dom_image* dom;

    dom = xc_dom_allocate(ctx->xci, guest->cmdline, "");
    if (dom == NULL) {
        ret = errno;
        goto out_err;
    }

    if (xenstore != NULL) {
        dom->xenstore_domid = xenstore->backend_id;
        dom->xenstore_evtchn = xenstore->evtchn;
    }

    if (console != NULL) {
        dom->console_domid = console->backend_id;
        dom->console_evtchn = console->evtchn;
    }

    dom->flags = 0;

    /* FIXME: Support PVH */
    /* dom->pvh_enable = 1; */

    switch (guest->kernel.type) {
        case h2_kernel_buff_t_mem:
            ret = xc_dom_kernel_mem(dom,
                    guest->kernel.buff.mem.ptr, guest->kernel.buff.mem.size);
            break;

        case h2_kernel_buff_t_file:
            ret = xc_dom_kernel_file(dom, guest->kernel.buff.path);
            break;

        default:
            ret = EINVAL;
            break;
    }
    if (ret) {
        goto out_dom;
    }

    ret = xc_dom_boot_xen_init(dom, ctx->xci, guest->id);
    if (ret) {
        goto out_dom;
    }

#if defined(__arm__) || defined(__aarch64__)
    ret = xc_dom_rambase_init(dom, GUEST_RAM_BASE);
    if (ret) {
        goto out_dom;
    }
#endif

    ret = xc_dom_parse_image(dom);
    if (ret) {
        goto out_dom;
    }

    ret = xc_dom_mem_init(dom, guest->memory / 1024);
    if (ret) {
        goto out_dom;
    }

    ret = xc_dom_boot_mem_init(dom);
    if (ret) {
        goto out_dom;
    }

    ret = xc_dom_build_image(dom);
    if (ret) {
        goto out_dom;
    }

    ret = xc_dom_boot_image(dom);
    if (ret) {
        goto out_dom;
    }

    ret = xc_dom_gnttab_init(dom);
    if (ret) {
        goto out_dom;
    }

    if (xenstore != NULL) {
        xenstore->mfn = xc_dom_p2m(dom, dom->xenstore_pfn);
    }

    if (console != NULL) {
        console->mfn = xc_dom_p2m(dom, dom->console_pfn);
    }

    xc_dom_release(dom);

    return 0;

out_dom:
    xc_dom_release(dom);

out_err:
    return ret;
}

static int __xc_domain_destroy(h2_xen_ctx* ctx, h2_guest* guest)
{
    return xc_domain_destroy(ctx->xci, guest->id);
}

static int __xc_domain_unpause(h2_xen_ctx* ctx, h2_guest* guest)
{
    return xc_domain_unpause(ctx->xci, guest->id);
}

static int __xs_domain_create(h2_xen_ctx* ctx, h2_guest* guest)
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

static int __xs_domain_destroy(h2_xen_ctx* ctx, h2_guest* guest)
{
    if (!xs_rm(ctx->xsh, XBT_NULL, guest->hyp.info.xen->xs_dom_path)) {
        return errno;
    }

    return 0;
}

static int __xs_domain_intro(h2_xen_ctx* ctx, h2_guest* guest, h2_xen_dev_xenstore* xenstore)
{
    if (!xs_introduce_domain(ctx->xsh, guest->id, xenstore->mfn, xenstore->evtchn)) {
        return errno;
    }

    return 0;
}

static int __xs_console_create(h2_xen_ctx* ctx, h2_guest* guest, h2_xen_dev_console* console)
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


int h2_xen_domain_create(h2_xen_ctx* ctx, h2_guest* guest)
{
    int ret;

    int idx;
    h2_xen_dev* dev;
    h2_xen_dev_xenstore* xenstore;
    h2_xen_dev_console* console;
    xc_evtchn_port_or_error_t ec_ret;

    ret = __xc_domain_create(ctx, guest);
    if (ret) {
        goto out_err;
    }

    idx = 0;
    dev = NULL;
    xenstore = NULL;
    dev = __xen_get_next_dev(guest, h2_xen_dev_t_xenstore, &idx);
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
    dev = __xen_get_next_dev(guest, h2_xen_dev_t_console, &idx);
    if (dev != NULL) {
        console = &(dev->dev.console);
        ec_ret = xc_evtchn_alloc_unbound(ctx->xci, guest->id, console->backend_id);
        if (ec_ret == -1) {
            ret = errno;
            goto out_dom;
        }
        console->evtchn = ec_ret;
    }

    ret = __xc_domain_init(ctx, guest, xenstore, console);
    if (ret) {
        goto out_dom;
    }

    ret = __xs_domain_create(ctx, guest);
    if (ret) {
        goto out_dom;
    }

    if (console != NULL) {
        ret = __xs_console_create(ctx, guest, console);
        if (ret) {
            goto out_xs;
        }
    }

    if (xenstore != NULL) {
        ret = __xs_domain_intro(ctx, guest, xenstore);
        if (ret) {
            goto out_xs;
        }
    }

    ret = __xc_domain_unpause(ctx, guest);
    if (ret) {
        goto out_xs;
    }

    return 0;

out_xs:
    __xs_domain_destroy(ctx, guest);

out_dom:
    __xc_domain_destroy(ctx, guest);

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

    _ret = __xc_domain_destroy(ctx, guest);
    if (_ret && !ret) {
        ret = _ret;
    }

    _ret = __xs_domain_destroy(ctx, guest);
    if (_ret && !ret) {
        ret = _ret;
    }

out:
    return ret;
}
