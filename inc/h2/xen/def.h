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

#ifndef __H2__XEN__DEF__H__
#define __H2__XEN__DEF__H__

#define _GNU_SOURCE

#include <errno.h>
#include <stdlib.h>
#include <xenstore.h>
#include <xenctrl.h>
#include <xenevtchn.h>


struct h2_xen_ctx {
    struct xs_handle* xsh;

    xc_interface* xci;
    xentoollog_logger *xtl;
};
typedef struct h2_xen_ctx h2_xen_ctx;


#define H2_XEN_DEV_COUNT_MAX 32

enum h2_xen_dev_t {
    h2_xen_dev_t_none = 0 ,
    h2_xen_dev_t_xenstore ,
    h2_xen_dev_t_console  ,
};

struct h2_xen_dev_xenstore {
    domid_t backend_id;
    evtchn_port_t evtchn;
    unsigned long mfn;
};
typedef struct h2_xen_dev_xenstore h2_xen_dev_xenstore;

struct h2_xen_dev_console {
    domid_t backend_id;
    evtchn_port_t evtchn;
    unsigned long mfn;
};
typedef struct h2_xen_dev_console h2_xen_dev_console;

struct h2_xen_dev {
    enum h2_xen_dev_t type;
    union {
        h2_xen_dev_xenstore xenstore;
        h2_xen_dev_console console;
    } dev;
};
typedef struct h2_xen_dev h2_xen_dev;


struct h2_xen_guest {
    char* xs_dom_path;

    h2_xen_dev devs[H2_XEN_DEV_COUNT_MAX];
};
typedef struct h2_xen_guest h2_xen_guest;

#endif /* __H2__XEN__DEF__H__ */
