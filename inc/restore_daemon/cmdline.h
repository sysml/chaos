#ifndef __DAEMON__CMDLINE__H__
#define __DAEMON__CMDLINE__H__

#include <h2/h2.h>

#include <stdbool.h>


struct cmdline {
    bool help;
    bool error;

    int port;
};
typedef struct cmdline cmdline;


int cmdline_parse(int argc, char** argv, cmdline* cmd);
void cmdline_usage(char* argv0);

#endif /* __DAEMON__CMDLINE__H__ */
