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

#include <h2/guest.h>

#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
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

    struct {
        int count;
        bool count_set;

        h2_cpu_mask_t cpumask[H2_GUEST_VCPUS_MAX];
        bool cpumap_set;
    } vcpus;
    bool vcpus_set;

    bool address_size_set;
    unsigned int address_size;

    int vifs_nr;
    struct {
        bool ip_set;
        struct in_addr ip;
        bool mac_set;
        uint8_t mac[6];
        const char* bridge;
    } vifs[DEV_MAX_COUNT];

    bool paused;
    bool paused_set;

    struct {
        bool pvh;
        bool pvh_set;

        h2_xen_dev_meth_t dev_meth;
        bool dev_meth_set;
    } xen;
    bool xen_set;
};
typedef struct config config;


static void __init(config* conf)
{
    memset(conf, 0, sizeof(config));

    conf->address_size = 64;
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

    if (!conf->vcpus_set) {
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
    (*guest)->vcpus.count = conf->vcpus.count;
    if (conf->vcpus.cpumap_set) {
        memcpy((*guest)->vcpus.mask, conf->vcpus.cpumask,
                sizeof(h2_cpu_mask_t[H2_GUEST_VCPUS_MAX]));
    }
    (*guest)->address_size = conf->address_size;

    (*guest)->paused = conf->paused;

    (*guest)->hyp.guest.xen->pvh = conf->xen.pvh;

    switch (conf->xen.dev_meth) {
        case h2_xen_dev_meth_t_xs:
            (*guest)->hyp.guest.xen->xs.active = true;
            break;

#ifdef CONFIG_H2_XEN_NOXS
        case h2_xen_dev_meth_t_noxs:
            (*guest)->hyp.guest.xen->noxs.active = true;
            break;
#endif
    };

    /* TODO: Add console to domain when using NoXS
     * Currently there is no noxs backend available for console, therefore
     * avoid adding a console to the domain since that will make the creation
     * fail.
     */
#ifdef CONFIG_H2_XEN_NOXS
    if (conf->xen.dev_meth != h2_xen_dev_meth_t_noxs)
#endif
    {
        (*guest)->hyp.guest.xen->console.active = true;
        (*guest)->hyp.guest.xen->console.meth = conf->xen.dev_meth;
        /* TODO: Allow user to configure console backend_id. */
        (*guest)->hyp.guest.xen->console.be_id = 0;
    }

    for (int i = 0; i < conf->vifs_nr; i++) {
        (*guest)->hyp.guest.xen->devs[i].type = h2_xen_dev_t_vif;
        (*guest)->hyp.guest.xen->devs[i].dev.vif.id = i;
        (*guest)->hyp.guest.xen->devs[i].dev.vif.backend_id = 0;
        (*guest)->hyp.guest.xen->devs[i].dev.vif.meth = conf->xen.dev_meth;
        memcpy(&((*guest)->hyp.guest.xen->devs[i].dev.vif.ip), &(conf->vifs[i].ip),
                sizeof(struct in_addr));
        memcpy((*guest)->hyp.guest.xen->devs[i].dev.vif.mac, conf->vifs[i].mac, 6);
        (*guest)->hyp.guest.xen->devs[i].dev.vif.bridge = strdup(conf->vifs[i].bridge);
    }

    return 0;

out:
    return ret;
}

static int __parse_ip(struct in_addr* ip, const char* ip_str)
{
    if (inet_aton(ip_str, ip) == 0) {
        return EINVAL;
    }

    return 0;
}

static int __parse_mac(uint8_t mac[6], const char* mac_str)
{
    int ret;

    ret = sscanf(mac_str, "%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8,
                &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);

    if (ret != 6) {
        return EINVAL;
    }

    return 0;
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
        if (strcmp(key, "ip") == 0) {
            if (conf->vifs[vid].ip_set) {
                fprintf(stderr, "Parameter 'ip' defined multiple times.\n");
                ret = EINVAL;
                goto out;
            }

            const char* ip_str = json_string_value(value);
            if (ip_str == NULL) {
                fprintf(stderr, "Parameter 'ip' has invalid type, must be string.\n");
                ret = EINVAL;
                goto out;
            }

            ret = __parse_ip(&(conf->vifs[vid].ip), ip_str);
            if (ret) {
                fprintf(stderr, "Parameter 'ip' is an invalid IP.\n");
                ret = EINVAL;
                goto out;
            }
            conf->vifs[vid].ip_set = true;
        } else if (strcmp(key, "mac") == 0) {
            if (conf->vifs[vid].mac_set) {
                fprintf(stderr, "Parameter 'mac' defined multiple times.\n");
                ret = EINVAL;
                goto out;
            }

            const char* mac_str = json_string_value(value);
            if (mac_str == NULL) {
                fprintf(stderr, "Parameter 'mac' has invalid type, must be string.\n");
                ret = EINVAL;
                goto out;
            }

            ret = __parse_mac(conf->vifs[vid].mac, mac_str);
            if (ret) {
                fprintf(stderr, "Parameter 'mac' is an invalid MAC.\n");
                ret = EINVAL;
                goto out;
            }
            conf->vifs[vid].mac_set = true;
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

static int __parse_vcpus(json_t* vcpus, config* conf)
{
    int ret;

    size_t idxi;
    size_t idxj;
    json_t* map;
    json_t* cpuid;
    json_t* value;
    int cpuid_val;
    const char* key;

    if (json_is_object(vcpus)) {
        json_object_foreach(vcpus, key, value) {
            if (strcmp(key, "count") == 0) {
                if (conf->vcpus.count_set) {
                    fprintf(stderr, "Parameter 'count' defined multiple times.\n");
                    ret = EINVAL;
                    goto out;
                }

                conf->vcpus.count = json_integer_value(value);
                if (conf->vcpus.count == 0) {
                    fprintf(stderr, "Parameter 'count' is invalid, must be positive integer.\n");
                    ret = EINVAL;
                    goto out;
                }
                conf->vcpus.count_set = true;
            } else if (strcmp(key, "cpumap") == 0) {
                if (conf->vcpus.cpumap_set) {
                    fprintf(stderr, "Parameter 'cpumap' defined multiple times.\n");
                    ret = EINVAL;
                    goto out;
                }

                if (!json_is_array(value)) {
                    fprintf(stderr, "Parameter 'cpumap' has invalid type, must be array.\n");
                    ret = EINVAL;
                    goto out;
                }

                json_array_foreach(value, idxi, map) {
                    if (!json_is_array(map)) {
                        fprintf(stderr, "Element of 'cpumap' has invalid type,"
                                "must be array of integers.\n");
                        ret = EINVAL;
                        goto out;
                    }

                    json_array_foreach(map, idxj, cpuid) {
                        if (!json_is_integer(cpuid)) {
                            fprintf(stderr, "Element of 'cpumap' has invalid type,"
                                    "must be array of integers.\n");
                            ret = EINVAL;
                            goto out;
                        }

                        cpuid_val = json_integer_value(cpuid);
                        if (cpuid_val < 0) {
                            fprintf(stderr, "Invalid cpuid specified on 'cpumap' definition.\n");
                            ret = EINVAL;
                            goto out;
                        }

                        h2_cpu_mask_set(conf->vcpus.cpumask[idxi], cpuid_val);
                    }
                }
                conf->vcpus.cpumap_set = true;
            } else {
                fprintf(stderr, "Invalid parameter '%s' on vcpus definition.\n", key);
                ret = EINVAL;
                goto out;
            }
        }
    } else {
        /* It's okay to call without check type, returns 0. */
        conf->vcpus.count = json_integer_value(vcpus);
        if (conf->vcpus.count == 0) {
            fprintf(stderr, "Parameter 'vpcus' is invalid, must be positive integer.\n");
            ret = EINVAL;
            goto out;
        }
        conf->vcpus.count_set = true;
    }

    if (!conf->vcpus.count_set) {
        fprintf(stderr, "Missing parameter 'count' on 'vcpus'\n");
        ret = EINVAL;
        goto out;
    }

    return 0;

out:
    return ret;
}

static int __parse_xen(json_t* xen, config* conf)
{
    int ret;

    json_t* value;
    const char* key;

    if (!json_is_object(xen)) {
        fprintf(stderr, "Parameter 'xen' has invalid type, must be object.\n");
        ret = EINVAL;
        goto out;
    }

    json_object_foreach(xen, key, value) {
        if (strcmp(key, "pvh") == 0) {
            if (conf->xen.pvh_set) {
                fprintf(stderr, "Parameter 'pvh' defined multiple times.\n");
                ret = EINVAL;
                goto out;
            }

            if (!json_is_boolean(value)) {
                fprintf(stderr, "Parameter 'pvh' has invalid type, must be boolean.\n");
                ret = EINVAL;
                goto out;
            }

            conf->xen.pvh = json_boolean_value(value);
            conf->xen.pvh_set = true;
        } else if (strcmp(key, "dev_method") == 0) {
            if (conf->xen.dev_meth_set) {
                fprintf(stderr, "Parameter 'dev_meth' defined multiple times.\n");
                ret = EINVAL;
                goto out;
            }

            const char* meth = json_string_value(value);
            if (meth == NULL) {
                fprintf(stderr, "Parameter 'dev_meth' has invalid type, must be string.\n");
                ret = EINVAL;
                goto out;
            }

            if (strcmp(meth, "xenstore") == 0) {
                conf->xen.dev_meth = h2_xen_dev_meth_t_xs;
#ifdef CONFIG_H2_XEN_NOXS
            } else if (strcmp(meth, "noxs") == 0) {
                conf->xen.dev_meth = h2_xen_dev_meth_t_noxs;
#endif
            } else {
                fprintf(stderr, "Parameter 'dev_meth' has invalid value, must be xenstore"
#ifdef CONFIG_H2_XEN_NOXS
                        " or noxs"
#endif
                        ".\n");
                ret = EINVAL;
                goto out;
            }

            conf->xen.dev_meth_set = true;
        } else {
            fprintf(stderr, "Invalid parameter '%s' on xen definition.\n", key);
            ret = EINVAL;
            goto out;
        }
    }

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
            if (conf->vcpus_set) {
                fprintf(stderr, "Parameter 'vcpus' defined multiple times.\n");
                ret = EINVAL;
                goto out;
            }

            ret = __parse_vcpus(value, conf);
            if (ret) {
                goto out;
            }
            conf->vcpus_set = true;
        } else if (strcmp(key, "address_size") == 0) {
            if (conf->address_size_set) {
                fprintf(stderr, "Parameter 'address_size' defined multiple times.\n");
                ret = EINVAL;
                goto out;
            }

            /* It's okay to call without check type, returns 0. */
            conf->address_size = json_integer_value(value);
            if (conf->address_size != 32 && conf->address_size != 64) {
                fprintf(stderr, "Parameter 'vpcus' is invalid, must be positive integer.\n");
                ret = EINVAL;
                goto out;
            }
            conf->address_size_set = true;
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
        } else if (strcmp(key, "xen") == 0) {
            if (conf->xen_set) {
                fprintf(stderr, "Parameter 'xen' defined multiple times.\n");
                ret = EINVAL;
                goto out;
            }

            ret = __parse_xen(value, conf);
            if (ret) {
                goto out;
            }
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
