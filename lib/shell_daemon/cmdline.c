#include <shell_daemon/cmdline.h>

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


    const char *short_opts = "hm:s:v";
    const struct option long_opts[] = {
        { "help"               , no_argument       , NULL , 'h' },
        { "memory"             , required_argument , NULL , 'm' },
        { "shells"             , required_argument , NULL , 's' },
        { "verbose"            , required_argument , NULL , 'v' },
        { NULL , 0 , NULL , 0 }
    };

    int opt;
    int opt_index;

    // Default values
    cmd->memory = 64 * 1024;
    cmd->shells = 10;
    cmd->verbose = false;

    while (1) {
        opt = getopt_long(argc, argv, short_opts, long_opts, &opt_index);

        if (opt == -1) {
            break;
        }

        switch (opt) {
            case 'h':
                cmd->help = true;
                break;

            case 'm':
                cmd->memory = strtoul(optarg, NULL, 10) * 1024;
                if (cmd->memory == 0) {
                    fprintf(stderr, "Could not parse -m option.\n");
                    cmd->error = true;
                }
                break;

            case 's':
                cmd->shells = strtoul(optarg, NULL, 10);
                if (cmd->shells == 0) {
                    fprintf(stderr, "Could not parse -s option.\n");
                    cmd->error = true;
                }
                break;

            case 'v':
                cmd->verbose = true;
                break;

            default:
                cmd->error = true;
                break;
        }
    }

    return 0;
}

void cmdline_usage(char* argv0)
{
    printf("Usage: %s [options]\n", argv0);
    printf("\n");
    printf("  -h, --help             Display this help and exit.\n");
    printf("  -m, --memory           Amount of memory per shell [MB]\n");
    printf("  -s, --shells           Number of shells to precreate\n");
    printf("  -v, --verbose          Write more detailed information to syslog\n");
    printf("\n");
}
