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
 */

#ifndef __H2__XEN__DEF__H__
#define __H2__XEN__DEF__H__

#define _GNU_SOURCE

#include <arpa/inet.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <xenstore.h>
#include <xenctrl.h>
#ifdef CONFIG_H2_XEN_NOXS
/* FIXME: Find out why this *has* to be included after xenctrl.h */
#include <xen/io/noxs.h>
#endif


enum h2_xen_xlib_t {
    h2_xen_xlib_t_xc ,
};
typedef enum h2_xen_xlib_t h2_xen_xlib_t;

struct h2_xen_cfg {
    struct {
        bool active;
        domid_t domid;
    } xs;

#ifdef CONFIG_H2_XEN_NOXS
    struct {
        bool active;
    } noxs;
#endif

    h2_xen_xlib_t xlib;
};
typedef struct h2_xen_cfg h2_xen_cfg;

struct h2_xen_ctx {
    struct {
        bool active;
        domid_t domid;
        struct xs_handle* xsh;
    } xs;

    struct {
        xc_interface* xci;
        xentoollog_logger *xtl;
    } xc;

#ifdef CONFIG_H2_XEN_NOXS
    struct {
        bool active;

        int fd;
    } noxs;
#endif

    h2_xen_xlib_t xlib;
};
typedef struct h2_xen_ctx h2_xen_ctx;


#define H2_XEN_DEV_COUNT_MAX 32

enum h2_xen_dev_meth_t {
    h2_xen_dev_meth_t_xs ,
#ifdef CONFIG_H2_XEN_NOXS
    h2_xen_dev_meth_t_noxs ,
#endif
};
typedef enum h2_xen_dev_meth_t h2_xen_dev_meth_t;

enum h2_xen_dev_t {
    h2_xen_dev_t_none = 0 ,
    h2_xen_dev_t_sysctl ,
    h2_xen_dev_t_vif ,
    h2_xen_dev_t_vbd ,
};
typedef enum h2_xen_dev_t h2_xen_dev_t;

struct h2_xen_dev_sysctl {
    domid_t backend_id;
};
typedef struct h2_xen_dev_sysctl h2_xen_dev_sysctl;

struct h2_xen_dev_vif {
    int id;
    bool valid;
    h2_xen_dev_meth_t meth;

    domid_t backend_id;
    struct in_addr ip;
    uint8_t mac[6];
    char* bridge;
    char* script;
};
typedef struct h2_xen_dev_vif h2_xen_dev_vif;

struct h2_xen_dev_vbd {
    int id;
    bool valid;
    h2_xen_dev_meth_t meth;

    domid_t backend_id;
    char* target;
    char* target_type;
    char* vdev;
    char* access;
    char* script;

    int major;
    int minor;
};
typedef struct h2_xen_dev_vbd h2_xen_dev_vbd;

struct h2_xen_dev {
    h2_xen_dev_t type;
    union {
        h2_xen_dev_sysctl sysctl;
        h2_xen_dev_vif vif;
        h2_xen_dev_vbd vbd;
    } dev;
};
typedef struct h2_xen_dev h2_xen_dev;

struct h2_xen_guest_priv {
    struct {
        bool active;

        evtchn_port_t evtchn;
        unsigned int gmfn;

        char* dom_path;
    } xs;

    struct {
        evtchn_port_t evtchn;
        unsigned int gmfn;
    } console;

    h2_xen_xlib_t xlib;
    union {
        struct {
            bool active;
            struct xc_dom_image* img;
        } xc;
    } xlibd;
};
typedef struct h2_xen_guest_priv h2_xen_guest_priv;

struct h2_xen_guest {
    bool pvh;

    struct {
        bool active;
    } xs;

#ifdef CONFIG_H2_XEN_NOXS
    struct {
        bool active;
    } noxs;
#endif

    struct {
        bool active;

        domid_t be_id;
        h2_xen_dev_meth_t meth;
    } console;

    h2_xen_dev devs[H2_XEN_DEV_COUNT_MAX];

    h2_xen_guest_priv priv;
};
typedef struct h2_xen_guest h2_xen_guest;

#endif /* __H2__XEN__DEF__H__ */
