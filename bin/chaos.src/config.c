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

#include "config.h"

#include <errno.h>
#include <jansson.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>


#define DEV_MAX_COUNT 16

struct config {
    const char* name;
    const char* kernel;
    const char* cmdline;

    bool memory_set;
    unsigned int memory;

    bool vcpus_set;
    unsigned int vcpus;

    int vifs_nr;
    struct {
        const char* mac;
        const char* bridge;
    } vifs[DEV_MAX_COUNT];

    bool paused;
    bool paused_set;
};
typedef struct config config;


static void __init(config* conf)
{
    memset(conf, 0, sizeof(config));
}

static int __check(config* conf)
{
    int ret;

    ret = EINVAL;

    if (conf->name == NULL) {
        fprintf(stderr, "Missing parameter 'name'\n");
        goto out;
    }

    if (conf->kernel == NULL) {
        fprintf(stderr, "Missing parameter 'kernel'\n");
        goto out;
    }

    if (conf->memory == 0) {
        fprintf(stderr, "Missing parameter 'memory'\n");
        goto out;
    }

    if (conf->vcpus == 0) {
        fprintf(stderr, "Missing parameter 'vcpus'\n");
        goto out;
    }

    ret = 0;

out:
    return ret;
}

static int __to_h2_xen(config* conf, h2_guest** guest)
{
    int ret;

    ret = h2_guest_alloc(guest, h2_hyp_t_xen);
    if (ret) {
        goto out;
    }

    (*guest)->name = strdup(conf->name);

    (*guest)->kernel.type = h2_kernel_buff_t_file;
    (*guest)->kernel.buff.path = strdup(conf->kernel);
    (*guest)->cmdline = strdup(conf->cmdline);

    (*guest)->memory = conf->memory * 1024;
    (*guest)->vcpus.count = conf->vcpus;

    (*guest)->paused = conf->paused;

    (*guest)->hyp.info.xen->xs.active = true;

    (*guest)->hyp.info.xen->devs[0].type = h2_xen_dev_t_console;
    (*guest)->hyp.info.xen->devs[0].dev.console.meth = h2_xen_dev_meth_t_xs;
    (*guest)->hyp.info.xen->devs[0].dev.console.backend_id = 0;

    for (int i = 0; i < conf->vifs_nr; i++) {
        (*guest)->hyp.info.xen->devs[i + 1].type = h2_xen_dev_t_vif;
        (*guest)->hyp.info.xen->devs[i + 1].dev.vif.id = i;
        (*guest)->hyp.info.xen->devs[i + 1].dev.vif.backend_id = 0;
        (*guest)->hyp.info.xen->devs[i + 1].dev.vif.meth = h2_xen_dev_meth_t_xs;
        (*guest)->hyp.info.xen->devs[i + 1].dev.vif.mac = strdup(conf->vifs[i].mac);
        (*guest)->hyp.info.xen->devs[i + 1].dev.vif.bridge = strdup(conf->vifs[i].bridge);
    }

    return 0;

out:
    return ret;
}

static int __parse_vif(json_t* vif, config* conf)
{
    int ret;

    int vid;
    json_t* value;
    const char* key;

    if (!json_is_object(vif)) {
        fprintf(stderr, "Parameter 'vifs' contains invalid element. Must be array of objects.\n");
        ret = EINVAL;
        goto out;
    }

    vid = conf->vifs_nr;

    json_object_foreach(vif, key, value) {
        if (strcmp(key, "mac") == 0) {
            if (conf->vifs[vid].mac) {
                fprintf(stderr, "Parameter 'mac' defined multiple times.\n");
                ret = EINVAL;
                goto out;
            }

            conf->vifs[vid].mac = json_string_value(value);
            if (conf->vifs[vid].mac == NULL) {
                fprintf(stderr, "Parameter 'mac' has invalid type, must be string.\n");
                ret = EINVAL;
                goto out;
            }
        } else if (strcmp(key, "bridge") == 0) {
            if (conf->vifs[vid].bridge) {
                fprintf(stderr, "Parameter 'bridge' defined multiple times.\n");
                ret = EINVAL;
                goto out;
            }

            conf->vifs[vid].bridge = json_string_value(value);
            if (conf->vifs[vid].bridge == NULL) {
                fprintf(stderr, "Parameter 'bridge' has invalid type, must be string.\n");
                ret = EINVAL;
                goto out;
            }
        } else {
            fprintf(stderr, "Invalid parameter '%s' on vif definition.\n", key);
            ret = EINVAL;
            goto out;
        }
    }

    conf->vifs_nr++;

    return 0;

out:
    return ret;
}

static int __parse_root(json_t* root, config* conf)
{
    int ret;

    size_t idx;
    json_t* value;
    json_t* vif;
    const char* key;

    if (!json_is_object(root)) {
        fprintf(stderr, "Root element has invalid type, must be object.\n");
        ret = EINVAL;
        goto out;
    }

    json_object_foreach(root, key, value) {
        if (strcmp(key, "name") == 0) {
            if (conf->name) {
                fprintf(stderr, "Parameter 'name' defined multiple times.\n");
                ret = EINVAL;
                goto out;
            }

            conf->name = json_string_value(value);
            if (conf->name == NULL) {
                fprintf(stderr, "Parameter 'name' has invalid type, must be string.\n");
                ret = EINVAL;
                goto out;
            }
        } else if (strcmp(key, "kernel") == 0) {
            if (conf->kernel) {
                fprintf(stderr, "Parameter 'kernel' defined multiple times.\n");
                ret = EINVAL;
                goto out;
            }

            conf->kernel = json_string_value(value);
            if (conf->kernel == NULL) {
                fprintf(stderr, "Parameter 'kernel' has invalid type, must be string.\n");
                ret = EINVAL;
                goto out;
            }
        } else if (strcmp(key, "cmdline") == 0) {
            if (conf->cmdline) {
                fprintf(stderr, "Parameter 'cmdline' defined multiple times.\n");
                ret = EINVAL;
                goto out;
            }

            conf->cmdline = json_string_value(value);
            if (conf->cmdline == NULL) {
                fprintf(stderr, "Parameter 'cmdline' has invalid type, must be string.\n");
                ret = EINVAL;
                goto out;
            }
        } else if (strcmp(key, "memory") == 0) {
            if (conf->memory) {
                fprintf(stderr, "Parameter 'memory' defined multiple times.\n");
                ret = EINVAL;
                goto out;
            }

            /* It's okay to call without check type, returns 0. */
            conf->memory = json_integer_value(value);
            if (conf->memory == 0) {
                fprintf(stderr, "Parameter 'memory' is invalid, must be positive integer.\n");
                ret = EINVAL;
                goto out;
            }
        } else if (strcmp(key, "vcpus") == 0) {
            if (conf->vcpus) {
                fprintf(stderr, "Parameter 'vcpus' defined multiple times.\n");
                ret = EINVAL;
                goto out;
            }

            /* It's okay to call without check type, returns 0. */
            conf->vcpus = json_integer_value(value);
            if (conf->vcpus == 0) {
                fprintf(stderr, "Parameter 'vpcus' is invalid, must be positive integer.\n");
                ret = EINVAL;
                goto out;
            }
        } else if (strcmp(key, "vifs") == 0) {
            if (conf->vifs_nr) {
                fprintf(stderr, "Parameter 'vifs' defined multiple times.\n");
                ret = EINVAL;
                goto out;
            }

            if (!json_is_array(value)) {
                fprintf(stderr, "Parameter 'vifs' has invalid type, must be array.\n");
                ret = EINVAL;
                goto out;
            }

            ret = 0;
            json_array_foreach(value, idx, vif) {
                ret = __parse_vif(vif, conf);
                if (ret) {
                    goto out;
                }
            }
        } else if (strcmp(key, "paused") == 0) {
            if (conf->paused_set) {
                fprintf(stderr, "Parameter 'paused' defined multiple times.\n");
                ret = EINVAL;
                goto out;
            }

            if (!json_is_boolean(value)) {
                fprintf(stderr, "Parameter 'paused' has invalid type, must be boolean.\n");
                ret = EINVAL;
                goto out;
            }

            conf->paused = json_boolean_value(value);
            conf->paused_set = true;
        } else {
            fprintf(stderr, "Invalid parameter '%s' on guest definition.\n", key);
            ret = EINVAL;
            goto out;
        }
    }

    return 0;

out:
    return ret;
}

int config_parse(char* fpath, h2_hyp_t hyp, h2_guest** guest)
{
    int ret;

    config conf;
    json_t* root;
    json_error_t json_err;

    __init(&conf);

    root = json_load_file(fpath, 0, &json_err);
    if (root == NULL) {
        fprintf(stderr, "Failed to load file (%s).\n", json_err.text);
        ret = EINVAL;
        goto out;
    }

    ret = __parse_root(root, &conf);
    if (ret) {
        goto out_root;
    }

    ret = __check(&conf);
    if (ret) {
        goto out_root;
    }

    switch(hyp) {
        case h2_hyp_t_xen:
            ret = __to_h2_xen(&conf, guest);
            break;
    }
    if (ret) {
        goto out_root;
    }

    json_decref(root);

    return 0;

out_root:
    json_decref(root);

out:
    return ret;
}
