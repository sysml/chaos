/*
 * chaos shell daemon
 *
 * Authors: Florian Schmidt <florian.schmidt@neclab.eu>
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

#include <fcntl.h>
#include <string.h>
#include <syslog.h>
#include <linux/un.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>

#include <shell_daemon/cmdline.h>

#define ERROR(format...) syslog(LOG_DAEMON | LOG_ERR, format)
#define NOTICE(format...) syslog(LOG_DAEMON | LOG_NOTICE, format)
#define INFO(format...) syslog(LOG_DAEMON | LOG_INFO, format)

/* Global state */
struct {
    bool socket_created;
    int sockfd;
    bool shutting_down;
} global;

/* TODO: UDS name should be configurable */
static char *sockname = "/tmp/shell_daemon_socket";

void shutdown_shell_daemon(void)
{
    /* close socket so accept() stops blocking */
    if (global.socket_created) {
        close(global.sockfd);
        unlink(sockname);
    }
    global.shutting_down = true;
}

void handle_signal(int sig)
{
    switch (sig) {
        case SIGINT:
        case SIGTERM:
            NOTICE("caught signal %d (%s), terminating...\n", sig, strsignal(sig));
            shutdown_shell_daemon();
            break;
        /* TODO these could be used to manually make the daemon stock up on precreated shells, for example */
        case SIGUSR1:
        case SIGUSR2:
            INFO("caught signal %d (%s), ignoring (for now)...\n", sig, strsignal(sig));
            break;
        default:
            /* all signals that are handled should be handled by an explicit case statement */
            ERROR("caught unexpected signal %d (%s). Ignoring...\n", sig, strsignal(sig));
            break;
    }
}

int daemonize(void)
{
    pid_t pid;
    int fd;
    int ret = 0;

    /* Fork once */
    pid = fork();
    if (pid < 0) {
        ret = errno;
        fprintf(stderr, "First fork() failed: %d (%s)\n", ret, strerror(ret));
        return -ret;
    }

    /* let parent end so child gets adopted by init */
    if (pid > 0) {
        waitpid(pid, NULL, 0);
        exit(0);
    }

    openlog("chaos_daemon", LOG_PID | LOG_CONS, LOG_DAEMON);
    INFO("daemonizing...\n");

    /* close all potentially open file descriptors, then reopen
     * std{in, out, err} to something more sensible for a daemon */
    for (fd = sysconf(_SC_OPEN_MAX); fd >= 0; fd--) {
        close(fd);
    }
    fd = open("/dev/null", O_RDONLY);
    if (fd < 0) {
        ERROR("Error reopening stdin: %d (%s)\n", fd, strerror(fd));
    }
    else if (fd != 0) {
        ERROR("Unexpected fd for stdin (expected 0, got %d)\n", fd);
    }
    /* TODO log file location should be configurable */
    fd = open("/var/log/chaos_daemon.out", O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        ERROR("Error reopening stdout: %d (%s)\n", fd, strerror(fd));
    }
    else if (fd != 1) {
        ERROR("Unexpected fd for stdout (expected 1, got %d)\n", fd);
    }
    fd = open("/var/log/chaos_daemon.err", O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        ERROR("Error reopening stderr: %d (%s)\n", fd, strerror(fd));
    }
    else if (fd != 2) {
        ERROR("Unexpected fd for stderr (expected 2, got %d)\n", fd);
    }

    setsid();
    umask(S_IRWXG | S_IRWXO | S_IXUSR);
    chdir("/tmp/");
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGUSR1, handle_signal);
    signal(SIGUSR2, handle_signal);
    signal(SIGCHLD, SIG_IGN);

    /* Fork again to eventually become an orphan grandchild.
     * Since by now, we're a child of init, we inherit information
     * from init, irrevocably preventing terminal access.
     * AFAIK, this shouldn't be necessary when using setsid(), but
     * better safe than sorry. */
    pid = fork();
    if (pid < 0) {
        ret = errno;
        ERROR("Second fork() failed: %d (%s)\n", ret, strerror(ret));
        return -ret;
    }

    /* let child end, only grandchild continues */
    if (pid > 0) {
        exit(0);
    }

    INFO("daemonizing done.\n");
    return ret;
}

/* Set up the Unix Domain socket over which we receive commands locally */
int create_uds(void) {
    int retval;
    int sockfd;
    struct sockaddr_un addr;


    sockfd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (sockfd < 0) {
        retval = errno;
        ERROR("socket creation failed: %s\n", strerror(retval));
        return -retval;
    }

    unlink(sockname);

    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sockname, UNIX_PATH_MAX);
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        retval = errno;
        ERROR("socket bind() failed: %s\n", strerror(retval));
        return -retval;
    }
    if (listen(sockfd, 1) < 0) {
        retval = errno;
        ERROR("socket listen() failed: %s\n", strerror(retval));
        return -retval;
    }

    global.socket_created = true;
    return sockfd;
}

void wait_for_sockdata(void) {
    struct sockaddr_un addr;
    socklen_t addrlen = sizeof(addr);
    char *buf;
    ssize_t BUFLEN = 64;
    ssize_t recvd;
    int connfd;

    buf = malloc(BUFLEN);

    connfd = accept(global.sockfd, (struct sockaddr*)&addr, &addrlen);
    if (connfd < 0) {
        /* shutdown handler closes socket, releasing the blocking wait,
         * but also leading to an error message that we're not really
         * interested in at this point */
        if (!global.shutting_down) {
            ERROR("error during accept(): %d (%s)\n", errno, strerror(errno));
        }
    }
    else {
        recvd = recv(connfd, buf, BUFLEN-1, 0);
        if (recvd < 0) {
            ERROR("error during recv(): %d (%s)\n", errno, strerror(errno));
        }
        else {
            if (recvd < BUFLEN)
                buf[recvd] = '\0';
            else
                buf[BUFLEN-1] = '\0';
            INFO("received %s (%lu) from %s (%lu), %ld bytes\n", (char *)buf, strlen(buf), addr.sun_path, strlen(addr.sun_path), recvd);
            send(connfd, buf, strlen(buf), 0);
        }
        close(connfd);
    }
}

int main(int argc, char **argv)
{
    int ret;

    cmdline cmd;

    cmdline_parse(argc, argv, &cmd);

    if (cmd.error || cmd.help) {
        cmdline_usage(argv[0]);
        ret = cmd.error ? -EINVAL : 0;
        goto out;
    }

    ret = daemonize();
    if (ret) {
        goto out;
    }

    global.sockfd = create_uds();
    if (global.sockfd < 0) {
        ret = global.sockfd;
        goto out;
    }

    while (1) {
        if (global.shutting_down) {
            ret = 0;
            break;
        }

        wait_for_sockdata();
    }

out:
    return ret;
}
