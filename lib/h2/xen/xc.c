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
 * THIS HEADER MAY NOT BE EXTRACTED OR MODIFIED IN ANY WAY.
 */

#include <h2/xen/xc.h>

#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <xc_dom.h>


static int __evtchn_alloc_unbound(h2_xen_ctx* ctx, domid_t lid, domid_t rid, evtchn_port_t* evtchn)
{
    int ret;

    xc_evtchn_port_or_error_t ec_ret;

    ret = 0;

    ec_ret = xc_evtchn_alloc_unbound(ctx->xc.xci, lid, rid);
    if (ec_ret == -1) {
        ret = errno;
    } else {
        (*evtchn) = ec_ret;
    }

    return ret;
}

int h2_xen_xc_open(h2_xen_ctx* ctx, h2_xen_cfg* cfg)
{
    int ret;

    if (ctx == NULL || cfg == NULL) {
        ret = EINVAL;
        goto out_err;
    }

    if (ctx->xlib != h2_xen_xlib_t_xc) {
        ret = EINVAL;
        goto out_err;
    }

    /* FIXME: log level should be configurable. Keep debug while developing. */
    ctx->xc.xtl = (xentoollog_logger*) xtl_createlogger_stdiostream(stderr, XTL_DEBUG, 0);
    if (ctx->xc.xtl == NULL) {
        ret = errno;
        goto out_err;
    }

    ctx->xc.xci = xc_interface_open(ctx->xc.xtl, ctx->xc.xtl, 0);
    if (ctx->xc.xci == NULL) {
        ret = errno;
        goto out_xtl;
    }

    return 0;

out_xtl:
    xtl_logger_destroy(ctx->xc.xtl);
    ctx->xc.xtl = NULL;

out_err:
    return ret;
}

void h2_xen_xc_close(h2_xen_ctx* ctx)
{
    if (ctx == NULL) {
        return;
    }

    if (ctx->xc.xci) {
        xc_interface_close(ctx->xc.xci);
        ctx->xc.xci = NULL;
    }

    if (ctx->xc.xtl) {
        xtl_logger_destroy(ctx->xc.xtl);
        ctx->xc.xtl = NULL;
    }
}


int h2_xen_xc_domain_create(h2_xen_ctx* ctx, h2_guest* guest)
{
    int ret;

    uint32_t domid;
    uint32_t flags;
    xen_domain_handle_t dom_handle;
    xc_domain_configuration_t dom_config;

    /* NOTE: H2 only supports PV or PVH guests */

    /* Setting domid to 0 will tell the hypervisor to auto-allocate an id. */
    domid = 0;

    flags = 0;
    dom_config.emulation_flags = 0;

    if (guest->hyp.info.xen->pvh) {
        flags |= XEN_DOMCTL_CDF_pvh_guest;
        flags |= XEN_DOMCTL_CDF_hap;
    }

    /* FIXME: what is the ssidref parameter? */
    ret = xc_domain_create(ctx->xc.xci, 0, dom_handle, flags, &domid, &dom_config);
    if (ret) {
        goto out_err;
    }

    ret = xc_domain_max_vcpus(ctx->xc.xci, domid, guest->vcpus.count);
    if (ret) {
        goto out_dom;
    }

    /* TODO: Support CPU pools */

    int cpu_max;
    xc_cpumap_t cpu_map;

    cpu_max = xc_get_max_cpus(ctx->xc.xci);

    /* TODO: Support vCPU pinning. For now pinning to all available CPUs. */
    for (int vcpu = 0; vcpu < guest->vcpus.count; vcpu++) {
        cpu_map = xc_cpumap_alloc(ctx->xc.xci);

        for (int cpu = 0; cpu < cpu_max; cpu ++) {
            xc_cpumap_setcpu(cpu, cpu_map);
        }

        ret = xc_vcpu_setaffinity(ctx->xc.xci, domid, vcpu, cpu_map, NULL, XEN_VCPUAFFINITY_HARD);
        free(cpu_map);

        if (ret) {
            goto out_dom;
        }
    }

    ret = xc_domain_setmaxmem(ctx->xc.xci, domid, guest->memory);
    if (ret) {
        goto out_dom;
    }

    /* FIXME: Check what is the proper TSC Mode for pv and use macros */
    if (guest->hyp.info.xen->pvh) {
        ret = xc_domain_set_tsc_info(ctx->xc.xci, domid, 2, 0, 0, 0);
    } else {
        ret = xc_domain_set_tsc_info(ctx->xc.xci, domid, 0, 0, 0, 0);
    }
    if (ret) {
        goto out_dom;
    }

    guest->id = domid;

    return 0;

out_dom:
    xc_domain_destroy(ctx->xc.xci, domid);

out_err:
    return ret;
}

static void __choose_guest_type(h2_guest* guest, struct xc_dom_image* dom)
{
    //TODO: do we need to differentiate between pv and pvh? 32-PAE vs non-PAE?
    switch (guest->address_size) {
        case 32:
            dom->guest_type = "xen-3.0-x86_32";
            break;
        case 64:
            dom->guest_type = "xen-3.0-x86_64";
            break;
        default:
            dom->guest_type = "xen-3.0-unknown";
    }
}

static struct timespec allin, allout;

int h2_xen_xc_domain_init(h2_xen_ctx* ctx, h2_guest* guest, h2_xen_xc_dom* h2_dom)
{
    int ret;

    char* features;
    struct xc_dom_image* dom;

    features = NULL;
    if (guest->hyp.info.xen->pvh) {
        features =
            "|writable_descriptor_tables"
            "|auto_translated_physmap"
            "|supervisor_mode_kernel"
            "|hvm_callback_vector";
    }
    dom = xc_dom_allocate(ctx->xc.xci, guest->cmdline, features);
    if (dom == NULL) {
        ret = errno;
        goto out_err;
    }


    if (h2_dom->xs.active) {
        dom->xenstore_domid = h2_dom->xs.be_id;
        ret = __evtchn_alloc_unbound(ctx, guest->id, h2_dom->xs.be_id, &(h2_dom->xs.evtchn));
        if (ret) {
            goto out_dom;
        }
        dom->xenstore_evtchn = h2_dom->xs.evtchn;
    }

    if (h2_dom->console.active) {
        dom->console_domid = h2_dom->console.be_id;
        ret = __evtchn_alloc_unbound(ctx, guest->id, h2_dom->console.be_id, &(h2_dom->console.evtchn));
        if (ret) {
            goto out_xs_evtchn;
        }
        dom->console_evtchn = h2_dom->console.evtchn;
    }

    dom->flags = 0;
    if (guest->hyp.info.xen->pvh) {
        dom->pvh_enabled = 1;
    }

    ret = xc_dom_boot_xen_init(dom, ctx->xc.xci, guest->id);
    if (ret) {
        goto out_console_evtchn;
    }

#if defined(__arm__) || defined(__aarch64__)
    ret = xc_dom_rambase_init(dom, GUEST_RAM_BASE);
    if (ret) {
        goto out_console_evtchn;
    }
#endif

__choose_guest_type(guest, dom);

    ret = xc_dom_mem_init(dom, guest->memory / 1024);
    if (ret) {
        goto out_console_evtchn;
    }

    ret = xc_dom_boot_mem_init(dom);
    if (ret) {
        goto out_console_evtchn;
    }

    xc_cpuid_apply_policy(ctx->xc.xci, guest->id, NULL, 0);

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
        goto out_console_evtchn;
    }

    ret = xc_dom_parse_image(dom);
    if (ret) {
        goto out_console_evtchn;
    }

    ret = xc_dom_build_image(dom);
    if (ret) {
        goto out_console_evtchn;
    }

    ret = xc_dom_boot_image(dom);
    if (ret) {
        goto out_console_evtchn;
    }

    ret = xc_dom_gnttab_init(dom);
    if (ret) {
        goto out_console_evtchn;
    }

    if (h2_dom->xs.active) {
        if (guest->hyp.info.xen->pvh) {
            h2_dom->xs.mfn = dom->xenstore_pfn;
        } else {
            h2_dom->xs.mfn = xc_dom_p2m(dom, dom->xenstore_pfn);
        }
    }

    if (h2_dom->console.active) {
        if (guest->hyp.info.xen->pvh) {
            h2_dom->console.mfn = dom->console_pfn;
        } else {
            h2_dom->console.mfn = xc_dom_p2m(dom, dom->console_pfn);
        }
    }

    xc_dom_release(dom);

    return 0;

    /* FIXME: How to close the unbound evtchn opened for xenstore and console? */
out_console_evtchn:

out_xs_evtchn:

out_dom:
    xc_dom_release(dom);

out_err:
    return ret;
}

int h2_xen_xc_domain_destroy(h2_xen_ctx* ctx, h2_guest* guest)
{
    return xc_domain_destroy(ctx->xc.xci, guest->id);
}

int h2_xen_xc_domain_unpause(h2_xen_ctx* ctx, h2_guest* guest)
{
    return xc_domain_unpause(ctx->xc.xci, guest->id);
}
