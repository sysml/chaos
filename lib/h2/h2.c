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

#include <h2/h2.h>
#include <h2/xen.h>

#include <string.h>
#include <stdlib.h>
#include <errno.h>


int h2_open(h2_ctx** ctx, h2_hyp_t hyp)
{
    int ret;

    if (ctx == NULL) {
        ret = EINVAL;
        goto out_err;
    }

    (*ctx) = (h2_ctx*) malloc(sizeof(h2_ctx));
    if ((*ctx) == NULL) {
        ret = errno;
        goto out_err;
    }

    (*ctx)->hyp.type = hyp;

    switch (hyp) {
        case h2_hyp_t_xen:
            ret = h2_xen_open(&((*ctx)->hyp.ctx.xen));
            break;
        default:
            ret = EINVAL;
            break;
    }
    if (ret) {
        goto out_mem;
    }

    return 0;

out_mem:
    free(*ctx);
    (*ctx) = NULL;

out_err:
    return ret;
}

void h2_close(h2_ctx** ctx)
{
    if (ctx == NULL || (*ctx) == NULL) {
        return;
    }

    switch((*ctx)->hyp.type) {
        case h2_hyp_t_xen:
            h2_xen_close(&((*ctx)->hyp.ctx.xen));
            break;
    }

    free(*ctx);
    (*ctx) = NULL;
}

int h2_guest_alloc(h2_guest** guest, h2_hyp_t hyp)
{
    int ret;

    if (guest == NULL) {
        ret = EINVAL;
        goto out_err;
    }

    (*guest) = (h2_guest*) calloc(1, sizeof(h2_guest));
    if ((*guest) == NULL) {
        ret = errno;
        goto out_err;
    }

    (*guest)->hyp.type = hyp;
    switch (hyp) {
        case h2_hyp_t_xen:
            ret = h2_xen_guest_alloc(&((*guest)->hyp.info.xen));
            break;

        default:
            ret = EINVAL;
            break;
    }
    if (ret)  {
        goto out_mem;
    }

    return 0;

out_mem:
    free(*guest);
    (*guest) = NULL;

out_err:
    return ret;
}

void h2_guest_free(h2_guest** guest)
{
    if (guest == NULL || (*guest) == NULL) {
        return;
    }

    if ((*guest)->name) {
        free((*guest)->name);
        (*guest)->name = NULL;
    }

    if ((*guest)->cmdline) {
        free((*guest)->cmdline);
        (*guest)->cmdline = NULL;
    }

    switch ((*guest)->kernel.type) {
        case h2_kernel_buff_t_file:
            free((*guest)->kernel.buff.path);
            (*guest)->kernel.buff.path = NULL;
            break;

        case h2_kernel_buff_t_mem:
            break;
    }

    switch ((*guest)->hyp.type) {
        case h2_hyp_t_xen:
            h2_xen_guest_free(&((*guest)->hyp.info.xen));
            break;
    }

    free(*guest);
    (*guest) = NULL;
}


int h2_guest_create(h2_ctx* ctx, h2_guest* guest)
{
    int ret;

    switch (ctx->hyp.type) {
        case h2_hyp_t_xen:
            ret = h2_xen_domain_create(ctx->hyp.ctx.xen, guest);
            break;
        default:
            ret = EINVAL;
            break;
    }

    return ret;
}

int h2_guest_destroy(h2_ctx* ctx, h2_guest_id id)
{
    int ret;

    switch (ctx->hyp.type) {
        case h2_hyp_t_xen:
            ret = h2_xen_domain_destroy(ctx->hyp.ctx.xen, id);
            break;
        default:
            ret = EINVAL;
            break;
    }

    return ret;
}
