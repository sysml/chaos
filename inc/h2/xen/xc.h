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

#ifndef __H2__XEN__XC__H__
#define __H2__XEN__XC__H__

#include <h2/h2.h>

#include <xenctrl.h>


int h2_xen_xc_open(h2_xen_ctx* ctx, h2_xen_cfg* cfg);
void h2_xen_xc_close(h2_xen_ctx* ctx);

void h2_xen_xc_priv_free(h2_xen_guest* guest);



int h2_xen_xc_domain_preinit(h2_xen_ctx* ctx, h2_guest* guest);
int h2_xen_xc_domain_fastboot(h2_xen_ctx* ctx, h2_guest* guest);
int h2_xen_xc_domain_restore(h2_xen_ctx* ctx, h2_guest* guest);
int h2_xen_xc_domain_create(h2_xen_ctx* ctx, h2_guest* guest);
int h2_xen_xc_domain_query(h2_xen_ctx* ctx, h2_guest* guest);
int h2_xen_xc_domain_destroy(h2_xen_ctx* ctx, h2_guest* guest);
int h2_xen_xc_domain_unpause(h2_xen_ctx* ctx, h2_guest* guest);
int h2_xen_xc_domain_save(h2_xen_ctx* ctx, h2_guest* guest, h2_shutdown_callback_t shutdown_cb, void* user);
int h2_xen_xc_domain_resume(h2_xen_ctx* ctx, h2_guest* guest);

int h2_xen_xc_domain_list(h2_xen_ctx* ctx, struct guestq* guests);
#endif /* __H2__XEN__XC__H__ */
