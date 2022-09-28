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

#ifndef __H2__H2__H__
#define __H2__H2__H__

#include <h2/hyp.h>
#include <h2/guest.h>
#include <h2/guest_ctrl.h>


struct h2_ctx {
    h2_hyp_ctx hyp;
};
typedef struct h2_ctx h2_ctx;

TAILQ_HEAD(guestq, h2_guest);

int h2_open(h2_ctx** ctx, h2_hyp_t hyp, h2_hyp_cfg* cfg);
void h2_close(h2_ctx** ctx);

int h2_guest_alloc(h2_guest** guest, h2_hyp_t hyp);
int h2_guest_query(h2_ctx* ctx, h2_guest_id id, h2_guest** guest);
void h2_guest_reuse(h2_guest* guest);
void h2_guest_free(h2_guest** guest);

int h2_guest_list(h2_ctx* ctx, struct guestq* guests);

int h2_guest_create(h2_ctx* ctx, h2_guest* guest);
int h2_guest_destroy(h2_ctx* ctx, h2_guest* guest);
int h2_guest_shutdown(h2_ctx* ctx, h2_guest* guest, bool wait);

int h2_guest_save(h2_ctx* ctx, h2_guest* guest, bool wait);
int h2_guest_resume(h2_ctx* ctx, h2_guest* guest);

int h2_guest_serialize(h2_ctx* ctx, h2_guest_ctrl_save* gs, h2_guest* guest);
int h2_guest_deserialize(h2_ctx* ctx, h2_guest_ctrl_create* gc, h2_guest** guest);


enum h2_shutdown_reason {
    h2_shutdown_none ,
    h2_shutdown_poweroff ,
    h2_shutdown_suspend
};
typedef enum h2_shutdown_reason h2_shutdown_reason;


typedef int (*h2_query_callback_t)(h2_xen_ctx* ctx, h2_guest* guest);
typedef int (*h2_shutdown_callback_t)(h2_xen_ctx* ctx, h2_guest* guest, void* user);

#endif /* __H2__H2__H__ */
