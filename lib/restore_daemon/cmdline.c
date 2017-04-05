#include <restore_daemon/cmdline.h>

#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static void __init(cmdline* cmd)
{
    memset(cmd, 0, sizeof(cmdline));
}

int cmdline_parse(int argc, char** argv, cmdline* cmd)
{
    __init(cmd);


    const char *short_opts = "h";
    const struct option long_opts[] = {
        { "help"               , no_argument       , NULL , 'h' },
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

        cmd->port = atoi(argv[optind]);

    } else if (!cmd->help) {
        cmd->error = true;
    }

    return 0;
}

void cmdline_usage(char* argv0)
{
    printf("Usage: %s [option]... <port>\n", argv0);
    printf("\n");
    printf("  <port>                 Local port for migration receive.\n");
    printf("\n");
    printf("  -h, --help             Display this help and exit.\n");
    printf("\n");
}
