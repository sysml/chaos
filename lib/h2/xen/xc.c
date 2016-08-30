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

#include <h2/xen/xc.h>

#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <xc_dom.h>


int h2_xen_xc_domain_create(h2_xen_ctx* ctx, h2_guest* guest)
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

int h2_xen_xc_domain_init(h2_xen_ctx* ctx, h2_guest* guest,
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
        dom->xenstore_domid = ctx->xs_domid;
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

int h2_xen_xc_domain_destroy(h2_xen_ctx* ctx, h2_guest* guest)
{
    return xc_domain_destroy(ctx->xci, guest->id);
}

int h2_xen_xc_domain_unpause(h2_xen_ctx* ctx, h2_guest* guest)
{
    return xc_domain_unpause(ctx->xci, guest->id);
}

