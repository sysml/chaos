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

#include <chaos/cmdline.h>

#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static void __init(cmdline* cmd)
{
    memset(cmd, 0, sizeof(cmdline));

    cmd->enable_xs = true;
#ifdef CONFIG_H2_XEN_NOXS
    cmd->enable_noxs = true;
#endif

    cmd->nr_doms = 1;
}

static int __get_int(const char* str, int* val)
{
    char* endp;
    long int lval;

    errno = 0;
    lval = strtol(str, &endp, 10);

    /* If all string was consumed endp will point to '\0' */
    if (errno || *endp != '\0') {
        return EINVAL;
    }

    if (lval > INT_MAX) {
        return EINVAL;
    }

    (*val) = lval;

    return 0;
}

static void __parse_guest_id(const char* id, cmdline* cmd)
{
    int ret;
    int gid;

    ret = __get_int(id, &gid);
    if (ret || gid < 1) {
        fprintf(stderr, "Invalid value for 'guest_id' argument.\n");
        cmd->error = true;
    } else {
        cmd->gid = gid;
    }
}

static void __parse_create(int argc, char** argv, cmdline* cmd)
{
    int ret;
    int nr_doms;

    const char *short_opts = "n:";
    const struct option long_opts[] = {
        { "nr-doms"            , required_argument , NULL , 'n' },
        { NULL , 0 , NULL , 0 }
    };

    int opt;
    int opt_index;

    while (1) {
        opt = getopt_long(argc, argv, short_opts, long_opts, &opt_index);

        if (opt == -1) {
            break;
        }

        switch (opt) {
            case 'n':
                ret = __get_int(optarg, &(nr_doms));
                if (ret || nr_doms < 1) {
                    fprintf(stderr, "Invalid value for 'nr-doms' argument.\n");
                    cmd->error = true;
                } else {
                    cmd->nr_doms = nr_doms;
                }
                break;

            default:
                cmd->error = true;
                break;
        }
    }

    /* Now parse command */
    if ((argc - optind) == 1) {
        cmd->kernel = argv[optind];
    } else {
        fprintf(stderr, "Invalid number of arguments for 'create'.\n");
        cmd->error = true;
    }
}

static void __parse_destroy(int argc, char** argv, cmdline* cmd)
{
    if (argc != 2) {
        fprintf(stderr, "Invalid number of arguments for 'destroy'.\n");
        cmd->error = true;
        return;
    }

    __parse_guest_id(argv[1], cmd);
}

static void __parse_save(int argc, char** argv, cmdline* cmd)
{
    if (argc != 3) {
        fprintf(stderr, "Invalid number of arguments for 'save'.\n");
        cmd->error = true;
        return;
    }

    __parse_guest_id(argv[1], cmd);

    cmd->filename = argv[2];
}

static void __parse_restore(int argc, char** argv, cmdline* cmd)
{
    if (argc != 2) {
        fprintf(stderr, "Invalid number of arguments for 'restore'.\n");
        cmd->error = true;
        return;
    }

    cmd->filename = argv[1];
}

static void __parse_migrate(int argc, char** argv, cmdline* cmd)
{
    if (argc != 4) {
        fprintf(stderr, "Invalid number of arguments for 'migrate'.\n");
        cmd->error = true;
        return;
    }

    __parse_guest_id(argv[1], cmd);

    errno = 0;
    cmd->gid = (h2_guest_id) strtol(argv[1], NULL, 10);
    if (errno) {
        fprintf(stderr, "Invalid value for 'guest_id' argument'.\n");
        cmd->error = true;
    }

    if (inet_aton(argv[2], &cmd->destination.ip) == 0) {
        cmd->error = true;
        return;
    }

    cmd->destination.port = atoi(argv[3]);
}

static void __validate(cmdline* cmd)
{
    if (cmd->op == op_none && !cmd->help) {
        cmd->error = true;
        fprintf(stderr, "No command specified.\n");
    }

    if (!cmd->enable_xs
#ifdef CONFIG_H2_XEN_NOXS
            && !cmd->enable_noxs
#endif
            ) {
        fprintf(stderr, "No bus enabled.\n");
        cmd->error = true;
    }
}


int cmdline_parse(int argc, char** argv, cmdline* cmd)
{
    __init(cmd);


    const char *short_opts = "+h";
    const struct option long_opts[] = {
        { "help"               , no_argument       , NULL , 'h' },
        { "no-xs"              , no_argument       , NULL , 'X' },
#ifdef CONFIG_H2_XEN_NOXS
        { "no-noxs"            , no_argument       , NULL , 'N' },
#endif
        { NULL , 0 , NULL , 0 }
    };

    int opt;
    int opt_index;

    while (1) {
        opt = getopt_long(argc, argv, short_opts, long_opts, &opt_index);

        if (opt == -1) {
            break;
        }

        switch (opt) {
            case 'h':
                cmd->help = true;
                break;

            case 'X':
                cmd->enable_xs = false;
                break;

#ifdef CONFIG_H2_XEN_NOXS
            case 'N':
                cmd->enable_noxs = false;
                break;
#endif

            default:
                cmd->error = true;
                break;
        }
    }

    /* Now parse command */
    if (optind < argc) {
        argc -= optind;
        argv += optind;

        optind = 0;
        optopt = 0;

        if (strcmp(argv[optind], "create") == 0) {
            cmd->op = op_create;
            __parse_create(argc, argv, cmd);

        } else if (strcmp(argv[optind], "destroy") == 0) {
            cmd->op = op_destroy;
            __parse_destroy(argc, argv, cmd);

        } else if (strcmp(argv[optind], "shutdown") == 0) {
            cmd->op = op_shutdown;
            __parse_destroy(argc, argv, cmd);

        } else if (strcmp(argv[optind], "save") == 0) {
            cmd->op = op_save;
            __parse_save(argc, argv, cmd);

        } else if (strcmp(argv[optind], "restore") == 0) {
            cmd->op = op_restore;
            __parse_restore(argc, argv, cmd);

        } else if (strcmp(argv[optind], "migrate") == 0) {
            cmd->op = op_migrate;
            __parse_migrate(argc, argv, cmd);


        } else {
            cmd->error = true;

            fprintf(stderr, "Invalid command '%s'.\n", argv[optind]);
        }
    }

    __validate(cmd);

    return 0;
}

void cmdline_usage(char* argv0)
{
    printf("Usage: %s [option]... <command> [args...]\n", argv0);
    printf("\n");
    printf("  -h, --help             Display this help and exit.\n");
    printf("      --no-xs            Disable Xenstore.\n");
    printf("      --no-noxs          Disable NoXenstore.\n");
    printf("\n");
    printf("commands:\n");
    printf("    create [options] <config_file>\n");
    printf("        Create a new guest based on <config_file>.\n");
    printf("\n");
    printf("        -n, --nr-doms    Number of domains to create.\n");
    printf("\n");
    printf("    destroy <guest_id>\n");
    printf("        Create a new guest based on <config_file>.\n");
    printf("\n");
}
