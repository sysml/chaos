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

#ifndef __CHAOS__CONFIG__H__
#define __CHAOS__CONFIG__H__

#include <h2/guest.h>


struct h2_serialized_cfg {
    char*  data;
    size_t size;
};
typedef struct h2_serialized_cfg h2_serialized_cfg;


int config_parse(h2_serialized_cfg* cfg, h2_hyp_t hyp, h2_guest** guest);
int config_dump (h2_serialized_cfg* cfg, h2_hyp_t hyp, h2_guest*  guest);

int  h2_serialized_cfg_alloc(h2_serialized_cfg* cfg, size_t size);
void h2_serialized_cfg_free(h2_serialized_cfg* cfg);
int  h2_serialized_cfg_read(h2_serialized_cfg* cfg, stream_desc* sd);
int  h2_serialized_cfg_write(h2_serialized_cfg* cfg, stream_desc* sd);

int h2_vdev_to_vbd_id(const char* vdev, int* out_disk, int* out_partition);

#endif /* __CHAOS__CONFIG__H__ */
