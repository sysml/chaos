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
    bool name_set;
    const char* kernel;
    bool kernel_set;
    const char* cmdline;
    bool cmdline_set;

    unsigned int memory;
    bool memory_set;

    struct {
        int count;
        bool count_set;

        h2_cpu_mask_t cpumask[H2_GUEST_VCPUS_MAX];
        bool cpumap_set;
    } vcpus;
    bool vcpus_set;

    unsigned int address_size;
    bool address_size_set;

    struct {
        struct in_addr ip;
        bool ip_set;
        uint8_t mac[6];
        bool mac_set;
        const char* bridge;
        bool bridge_set;
    } vifs[DEV_MAX_COUNT];
    int vifs_count;
    bool vifs_set;

    bool paused;
    bool paused_set;

    struct {
        bool pvh;
        bool pvh_set;

        h2_xen_dev_meth_t dev_meth;
        bool dev_meth_set;
    } xen;
    bool xen_set;

    bool error;
};
typedef struct config config;


static void __init(config* conf)
{
    memset(conf, 0, sizeof(config));

    conf->address_size = 64;
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

    for (int i = 0; i < conf->vifs_count; i++) {
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


static void __parse_vifs(json_t* vifs, config* conf)
{
    int ret;

    size_t idx;
    json_t* vif;
    json_t* value;
    const char* key;

    if (!json_is_array(vifs)) {
        fprintf(stderr, "Parameter 'vifs' has invalid type, must be array.\n");
        conf->error = true;
        return;
    }

    json_array_foreach(vifs, idx, vif) {
        /* In case of error the loop will `continue` without incrementing the
         * counter, so do it at the beginning and access `vifs_count - 1`.
         */
        conf->vifs_count++;

		if (conf->vifs_count >= DEV_MAX_COUNT) {
			fprintf(stderr, "Too many vifs defined. Maximum %d supported.\n", DEV_MAX_COUNT);
            conf->error = true;
            break;
        }

        if (!json_is_object(vif)) {
            fprintf(stderr, "Parameter 'vifs' contains invalid element. Must be array of objects.\n");
            conf->error = true;
            continue;
        }

        json_object_foreach(vif, key, value) {
            if (strcmp(key, "ip") == 0) {
                if (conf->vifs[conf->vifs_count - 1].ip_set) {
                    fprintf(stderr, "Parameter 'ip' defined multiple times.\n");
                    conf->error = true;
                    continue;
                }

                conf->vifs[conf->vifs_count - 1].ip_set = true;

                const char* ip_str = json_string_value(value);
                if (ip_str == NULL) {
                    fprintf(stderr, "Parameter 'ip' has invalid type, must be string.\n");
                    conf->error = true;
                    continue;
                }

                ret = __parse_ip(&(conf->vifs[conf->vifs_count - 1].ip), ip_str);
                if (ret) {
                    fprintf(stderr, "Parameter 'ip' is an invalid IP.\n");
                    conf->error = true;
                }
            } else if (strcmp(key, "mac") == 0) {
                if (conf->vifs[conf->vifs_count - 1].mac_set) {
                    fprintf(stderr, "Parameter 'mac' defined multiple times.\n");
                    conf->error = true;
                    continue;
                }

                conf->vifs[conf->vifs_count - 1].mac_set = true;

                const char* mac_str = json_string_value(value);
                if (mac_str == NULL) {
                    fprintf(stderr, "Parameter 'mac' has invalid type, must be string.\n");
                    conf->error = true;
                    continue;
                }

                ret = __parse_mac(conf->vifs[conf->vifs_count - 1].mac, mac_str);
                if (ret) {
                    fprintf(stderr, "Parameter 'mac' is an invalid MAC.\n");
                    conf->error = true;
                }
            } else if (strcmp(key, "bridge") == 0) {
                if (conf->vifs[conf->vifs_count - 1].bridge_set) {
                    fprintf(stderr, "Parameter 'bridge' defined multiple times.\n");
                    conf->error = true;
                    continue;
                }

                conf->vifs[conf->vifs_count - 1].bridge_set = true;

                conf->vifs[conf->vifs_count - 1].bridge = json_string_value(value);
                if (conf->vifs[conf->vifs_count - 1].bridge == NULL) {
                    fprintf(stderr, "Parameter 'bridge' has invalid type, must be string.\n");
                    conf->error = true;
                }
            } else {
                fprintf(stderr, "Invalid parameter '%s' on vif definition.\n", key);
                conf->error = true;
            }
        }
    }
}

static void __parse_vcpus(json_t* vcpus, config* conf)
{
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
                    conf->error = true;
                    continue;
                }

                conf->vcpus.count_set = true;

                conf->vcpus.count = json_integer_value(value);
                if (conf->vcpus.count == 0) {
                    fprintf(stderr, "Parameter 'count' is invalid, must be positive integer.\n");
                    conf->error = true;
                }
            } else if (strcmp(key, "cpumap") == 0) {
                if (conf->vcpus.cpumap_set) {
                    fprintf(stderr, "Parameter 'cpumap' defined multiple times.\n");
                    conf->error = true;
                    continue;
                }

                conf->vcpus.cpumap_set = true;

                if (!json_is_array(value)) {
                    fprintf(stderr, "Parameter 'cpumap' of 'vcpus' has invalid type,"
                            "must be array.\n");
                    conf->error = true;
                    continue;
                }

                json_array_foreach(value, idxi, map) {
                    if (!json_is_array(map)) {
                        fprintf(stderr, "Element of 'cpumap' in 'vcpus' has invalid type,"
                                "must be array.\n");
                        conf->error = true;
                        continue;
                    }

                    json_array_foreach(map, idxj, cpuid) {
                        if (!json_is_integer(cpuid)) {
                            fprintf(stderr, "Element of 'cpumap' in 'vcpus' contains invalid"
                                    "element. Must be array of integers.\n");
                            conf->error = true;
                            continue;
                        }

                        cpuid_val = json_integer_value(cpuid);
                        if (cpuid_val < 0) {
                            fprintf(stderr, "Invalid cpuid specified on 'cpumap' definition.\n");
                            conf->error = true;
                            continue;
                        }

                        h2_cpu_mask_set(conf->vcpus.cpumask[idxi], cpuid_val);
                    }
                }
            } else {
                fprintf(stderr, "Invalid parameter '%s' on vcpus definition.\n", key);
                conf->error = true;
            }
        }
    } else {
        conf->vcpus.count_set = true;

        /* It's okay to call without check type, returns 0. */
        conf->vcpus.count = json_integer_value(vcpus);
        if (conf->vcpus.count == 0) {
            fprintf(stderr, "Parameter 'vpcus' is invalid, must be positive integer.\n");
            conf->error = true;
        }
    }

    if (!conf->vcpus.count_set) {
        fprintf(stderr, "Missing parameter 'count' on 'vcpus'\n");
        conf->error = true;
    }
}

static void __parse_xen(json_t* xen, config* conf)
{
    json_t* value;
    const char* key;

    if (!json_is_object(xen)) {
        fprintf(stderr, "Parameter 'xen' has invalid type, must be object.\n");
        conf->error = true;
        return;
    }

    json_object_foreach(xen, key, value) {
        if (strcmp(key, "pvh") == 0) {
            if (conf->xen.pvh_set) {
                fprintf(stderr, "Parameter 'pvh' defined multiple times.\n");
                conf->error = true;
                continue;
            }

            conf->xen.pvh_set = true;

            if (!json_is_boolean(value)) {
                fprintf(stderr, "Parameter 'pvh' has invalid type, must be boolean.\n");
                conf->error = true;
                continue;
            }

            conf->xen.pvh = json_boolean_value(value);
        } else if (strcmp(key, "dev_method") == 0) {
            if (conf->xen.dev_meth_set) {
                fprintf(stderr, "Parameter 'dev_meth' defined multiple times.\n");
                conf->error = true;
                continue;
            }

            conf->xen.dev_meth_set = true;

            const char* meth = json_string_value(value);
            if (meth == NULL) {
                fprintf(stderr, "Parameter 'dev_meth' has invalid type, must be string.\n");
                conf->error = true;
                continue;
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
                conf->error = true;
            }
        } else {
            fprintf(stderr, "Invalid parameter '%s' on xen definition.\n", key);
            conf->error = true;
        }
    }
}

static void __parse_root(json_t* root, config* conf)
{
    json_t* value;
    const char* key;

    if (!json_is_object(root)) {
        fprintf(stderr, "Root element has invalid type, must be object.\n");
        conf->error = true;
        return;
    }

    json_object_foreach(root, key, value) {
        if (strcmp(key, "name") == 0) {
            if (conf->name_set) {
                fprintf(stderr, "Parameter 'name' defined multiple times.\n");
                conf->error = true;
                continue;
            }

            conf->name_set = true;

            conf->name = json_string_value(value);
            if (conf->name == NULL) {
                fprintf(stderr, "Parameter 'name' has invalid type, must be string.\n");
                conf->error = true;
            }
        } else if (strcmp(key, "kernel") == 0) {
            if (conf->kernel_set) {
                fprintf(stderr, "Parameter 'kernel' defined multiple times.\n");
                conf->error = true;
                continue;
            }

            conf->kernel_set = true;

            conf->kernel = json_string_value(value);
            if (conf->kernel == NULL) {
                fprintf(stderr, "Parameter 'kernel' has invalid type, must be string.\n");
                conf->error = true;
            }
        } else if (strcmp(key, "cmdline") == 0) {
            if (conf->cmdline_set) {
                fprintf(stderr, "Parameter 'cmdline' defined multiple times.\n");
                conf->error = true;
                continue;
            }

            conf->cmdline_set = true;

            conf->cmdline = json_string_value(value);
            if (conf->cmdline == NULL) {
                fprintf(stderr, "Parameter 'cmdline' has invalid type, must be string.\n");
                conf->error = true;
            }
        } else if (strcmp(key, "memory") == 0) {
            if (conf->memory_set) {
                fprintf(stderr, "Parameter 'memory' defined multiple times.\n");
                conf->error = true;
                continue;
            }

            conf->memory_set = true;

            /* It's okay to call without check type, returns 0. */
            conf->memory = json_integer_value(value);
            if (conf->memory == 0) {
                fprintf(stderr, "Parameter 'memory' is invalid, must be positive integer.\n");
                conf->error = true;
            }
        } else if (strcmp(key, "vcpus") == 0) {
            if (conf->vcpus_set) {
                fprintf(stderr, "Parameter 'vcpus' defined multiple times.\n");
                conf->error = true;
                continue;
            }

            conf->vcpus_set = true;

            __parse_vcpus(value, conf);
        } else if (strcmp(key, "address_size") == 0) {
            if (conf->address_size_set) {
                fprintf(stderr, "Parameter 'address_size' defined multiple times.\n");
                conf->error = true;
                continue;
            }

            conf->address_size_set = true;

            /* It's okay to call without check type, returns 0. */
            conf->address_size = json_integer_value(value);
            if (conf->address_size != 32 && conf->address_size != 64) {
                fprintf(stderr, "Parameter 'vpcus' is invalid, must be positive integer.\n");
                conf->error = true;
            }
        } else if (strcmp(key, "vifs") == 0) {
            if (conf->vifs_set) {
                fprintf(stderr, "Parameter 'vifs' defined multiple times.\n");
                conf->error = true;
                continue;
            }

            conf->vifs_set = true;

            __parse_vifs(value, conf);
        } else if (strcmp(key, "paused") == 0) {
            if (conf->paused_set) {
                fprintf(stderr, "Parameter 'paused' defined multiple times.\n");
                conf->error = true;
                continue;
            }

            conf->paused_set = true;

            if (!json_is_boolean(value)) {
                fprintf(stderr, "Parameter 'paused' has invalid type, must be boolean.\n");
                conf->error = true;
                continue;
            }

            conf->paused = json_boolean_value(value);
        } else if (strcmp(key, "xen") == 0) {
            if (conf->xen_set) {
                fprintf(stderr, "Parameter 'xen' defined multiple times.\n");
                conf->error = true;
                continue;
            }

            conf->xen_set = true;

            __parse_xen(value, conf);
        } else {
            fprintf(stderr, "Invalid parameter '%s' on guest definition.\n", key);
            conf->error = true;
        }
    }

    if (!conf->name_set) {
        fprintf(stderr, "Missing parameter 'name'\n");
        conf->error = true;
    }

    if (!conf->kernel_set) {
        fprintf(stderr, "Missing parameter 'kernel'\n");
        conf->error = true;
    }

    if (!conf->memory_set) {
        fprintf(stderr, "Missing parameter 'memory'\n");
        conf->error = true;
    }

    if (!conf->vcpus_set) {
        fprintf(stderr, "Missing parameter 'vcpus'\n");
        conf->error = true;
    }
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

    __parse_root(root, &conf);
    if (conf.error) {
        ret = EINVAL;
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
