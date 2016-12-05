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

#ifndef __H2__GUEST__H__
#define __H2__GUEST__H__

#include <h2/hyp.h>


typedef uint64_t h2_guest_id;

enum h2_kernel_buff_t {
    h2_kernel_buff_t_mem  ,
    h2_kernel_buff_t_file ,
};
typedef enum h2_kernel_buff_t h2_kernel_buff_t;


#define H2_CPUS_MAX 256
#define H2_GUEST_VCPUS_MAX 128

typedef uint64_t h2_cpu_mask_t[4];

#define h2_cpu_mask_set(mask, cpuid) \
    (mask)[cpuid / 64] |= (1LU << (cpuid % 64))

#define h2_cpu_mask_clear(mask, cpuid) \
    (mask)[cpuid / 64] &= ~(1LU << (cpuid % 64))

#define h2_cpu_mask_is_set(mask, cpuid) \
    (((mask)[cpuid / 64] & (1LU << (cpuid % 64))) != 0)

struct h2_guest {
    h2_guest_id id;

    char* name;
    char* cmdline;

    uint memory;

    struct {
        int count;
        h2_cpu_mask_t mask[H2_GUEST_VCPUS_MAX];
    } vcpus;

    uint address_size;

    struct {
        h2_kernel_buff_t type;
        union {
            struct {
                void* ptr;
                size_t size;
            } mem;
            char* path;
        } buff;
    } kernel;

    bool paused;

    h2_hyp_guest hyp;
};
typedef struct h2_guest h2_guest;

#endif /* __H2__GUEST__H__ */
