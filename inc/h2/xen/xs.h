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

#ifndef __H2__XEN__XS__H__
#define __H2__XEN__XS__H__

#include <h2/h2.h>
#include <poll.h>


int h2_xen_xs_open(h2_xen_ctx* ctx);
int h2_xen_xs_close(h2_xen_ctx* ctx);

void h2_xen_xs_priv_free(h2_xen_guest* guest);

int h2_xen_xs_domain_create(h2_xen_ctx* ctx, h2_guest* guest);
int h2_xen_xs_domain_destroy(h2_xen_ctx* ctx, h2_guest* guest);
int h2_xen_xs_domain_intro(h2_xen_ctx* ctx, h2_guest* guest,
        evtchn_port_t evtchn, unsigned int gmfn);


struct h2_xen_xs_shutdown_ctx {
    char* reason;
    char* shutdown_path;
    char* token;

    struct pollfd pollfd;

    bool wait;
    h2_query_callback_t query_func;
};
typedef struct h2_xen_xs_shutdown_ctx h2_xen_xs_shutdown_ctx;

int h2_xen_xs_shutdown_ctx_open(h2_xen_xs_shutdown_ctx* sctx,
        h2_shutdown_reason reason,
        h2_xen_ctx* ctx, h2_guest* guest,
        h2_query_callback_t query_func, bool wait);
int h2_xen_xs_shutdown_ctx_close(h2_xen_xs_shutdown_ctx* sctx);

int h2_xen_xs_domain_shutdown(h2_xen_ctx* ctx, h2_guest* guest,
        h2_xen_xs_shutdown_ctx* sctx);


int h2_xen_xs_probe_guest(h2_xen_ctx* ctx, h2_guest* guest);
int h2_xen_xs_dev_enumerate(h2_xen_ctx* ctx, h2_guest* guest);

int h2_xen_xs_console_create(h2_xen_ctx* ctx, h2_guest* guest,
        evtchn_port_t evtchn, unsigned int gmfn);
int h2_xen_xs_console_destroy(h2_xen_ctx* ctx, h2_guest* guest);

int h2_xen_xs_vif_create(h2_xen_ctx* ctx, h2_guest* guest, h2_xen_dev_vif* vif);
int h2_xen_xs_vif_destroy(h2_xen_ctx* ctx, h2_guest* guest, h2_xen_dev_vif* vif);

int h2_xen_xs_vbd_create(h2_xen_ctx* ctx, h2_guest* guest, h2_xen_dev_vbd* vbd);
int h2_xen_xs_vbd_destroy(h2_xen_ctx* ctx, h2_guest* guest, h2_xen_dev_vbd* vbd);

#endif /* __H2__XEN__XS__H__ */
