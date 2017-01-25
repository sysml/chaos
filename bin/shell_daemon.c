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

#include <h2/h2.h>
#include <h2/xen.h>
#include <h2/xen/dev.h>
#include <ipc.h>
#include <shell_daemon/cmdline.h>

#define ERROR(format...) syslog(LOG_DAEMON | LOG_ERR, format)
#define WARN(format...) syslog(LOG_DAEMON | LOG_WARNING, format)
#define NOTICE(format...) syslog(LOG_DAEMON | LOG_NOTICE, format)
#define INFO(format...) syslog(LOG_DAEMON | LOG_INFO, format)

/* Global state */
struct {
    bool socket_created;
    int sockfd;
    bool shutting_down;
    bool shell_initialized;
    h2_ctx *ctx;
    struct h2_guest *shell[MAX_SHELLS];
    unsigned long remaining_shells;
    uint16_t last_ipaddr;
    char buf[MAX_CONFFILE_SIZE];
} global;

void shutdown_shell_daemon(void)
{
    /* close socket so accept() stops blocking */
    if (global.socket_created) {
        close(global.sockfd);
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

int fastboot_domain(h2_serialized_cfg* cfg)
{
    struct h2_guest* request;
    struct h2_guest* shell = global.shell[global.remaining_shells-1];
    int ret = 0;

    if (global.remaining_shells == 0) {
        // If anybody can think of a better fitting error code...
        return ENODEV;
    }

    ret = h2_guest_alloc(&request, h2_hyp_t_xen);
    if (ret) {
        return ret;
    }

    ret = config_parse(cfg, h2_hyp_t_xen, &request);
    if (ret) {
        goto out_h2;
    }

    // For now, just some very basic checks
    if ( (request->memory > shell->memory)
        || (request->address_size != shell->address_size)
        || (request->vcpus.count > shell->vcpus.count) ) {
        ret = EINVAL;
        goto out_h2;
    }

    if (request->kernel.type == h2_kernel_buff_t_file) {
        if (request->kernel.buff.file.k_path) {
            shell->kernel.buff.file.k_path = strdup(request->kernel.buff.file.k_path);
        }
        if (request->kernel.buff.file.rd_path) {
            shell->kernel.buff.file.rd_path = strdup(request->kernel.buff.file.rd_path);
        }
    }
    else {
        WARN("%s:%d This has never been tested and might break horribly!\n", __FILE__, __LINE__);
    }

    if (request->cmdline) {
        shell->cmdline = strdup(request->cmdline);
    }
    if (request->name) {
        if (shell->name) {
            free(shell->name);
        }
        shell->name = strdup(request->name);
    }

    ret = h2_xen_domain_fastboot(global.ctx->hyp.ctx.xen, shell);
    if (ret) {
        goto out_h2;
    }
    else {
        global.remaining_shells--;
        INFO("Fastbooted domain from shell %lu\n", global.remaining_shells);
    }

out_h2:
    h2_guest_free(&request);
    return ret;
}

void wait_for_sockdata(void) {
    struct sockaddr_un addr;
    socklen_t addrlen = sizeof(addr);
    ssize_t len;
    int ret;
    int connfd;
    h2_serialized_cfg cfg;

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
        len = recv(connfd, global.buf, MAX_CONFFILE_SIZE, 0);
        if (len < 0) {
            ERROR("error during recv(): %d (%s)\n", errno, strerror(errno));
            // chances are this might also fail in this case, but we might as well try
            *(int *)global.buf = (int)len;
            send(connfd, global.buf, sizeof(int), 0);
        }
        else {
            cfg.data = global.buf;
            cfg.size = len;
            ret = fastboot_domain(&cfg);
            *(int *)global.buf = ret;
            send(connfd, global.buf, sizeof(int), 0);
        }

        close(connfd);
    }
}

h2_guest* precreate_shell(unsigned long ind, h2_hyp_cfg* cfg, unsigned long memory)
{
    int ret;
    int i;
    h2_guest* shell;

    ret = h2_guest_alloc(&shell, h2_hyp_t_xen);
    if (ret) {
        ERROR("Allocating shell failed with error code %d.\n", ret);
        return NULL;
    }

    shell->name = strdup("[shell]");
    /* This is set on actual creation
    shell->cmdline = strdup(""); */
    shell->memory = memory;
    shell->vcpus.count = 1;
    shell->address_size = 64;
    shell->paused = false;

    shell->kernel.type = h2_kernel_buff_t_file;
    /* This is set on actual creation
    shell->kernel.buff.file.k_path
    shell->kernel.buff.file.rd_path */

    shell->hyp.guest.xen->pvh = false;
    shell->hyp.guest.xen->xs.active = true;
#ifdef CONFIG_H2_XEN_NOXS
    shell->hyp.guest.xen->noxs.active = true;
#endif
    shell->hyp.guest.xen->console.active = true;
    shell->hyp.guest.xen->console.meth = h2_xen_dev_meth_t_xs;
    shell->hyp.guest.xen->console.be_id = 0;
    shell->hyp.guest.xen->devs[1].type = h2_xen_dev_t_vif;
    shell->hyp.guest.xen->devs[1].dev.vif.id = 0;
    shell->hyp.guest.xen->devs[1].dev.vif.backend_id = 0;
    shell->hyp.guest.xen->devs[1].dev.vif.meth = h2_xen_dev_meth_t_xs;
    // increment IP address...
    global.last_ipaddr++;
    // .. but make sure to skip a.b.c.0 and a.b.c.255
    if ((global.last_ipaddr&0xff) == 0) {
        global.last_ipaddr += 1;
    }
    else if ((global.last_ipaddr&0xff) == 0xff) {
        global.last_ipaddr += 2;
    }
    shell->hyp.guest.xen->devs[1].dev.vif.ip.s_addr = (0x0a80<<16) | (global.last_ipaddr&0xff); /* 10.128.x.y*/
    shell->hyp.guest.xen->devs[1].dev.vif.mac[0] = 0xde;
    shell->hyp.guest.xen->devs[1].dev.vif.mac[1] = 0xad;
    shell->hyp.guest.xen->devs[1].dev.vif.mac[2] = 0xbe;
    shell->hyp.guest.xen->devs[1].dev.vif.mac[3] = 0xef;
    // make MAC match IP, easy to remember
    shell->hyp.guest.xen->devs[1].dev.vif.mac[4] = ((global.last_ipaddr>>8) & 0xff);
    shell->hyp.guest.xen->devs[1].dev.vif.mac[5] = (global.last_ipaddr & 0xff);
    shell->hyp.guest.xen->devs[1].dev.vif.bridge = strdup("xenbr");

    h2_open(&global.ctx, h2_hyp_t_xen, cfg);

    ret = h2_xen_domain_precreate(global.ctx->hyp.ctx.xen, shell);
    if (ret) {
        ERROR("Precreating shell failed with error code %d.\n", ret);
        goto out_free;
    }

    for (i = 0; i < H2_XEN_DEV_COUNT_MAX && !ret; i++) {
        ret = h2_xen_dev_create(global.ctx->hyp.ctx.xen, shell,
                                    &(shell->hyp.guest.xen->devs[i]));
    }
    if (ret) {
        ERROR("Failed to initialize Xen device %u with error code %d.\n", i, ret);
        for (i = 0; i < H2_XEN_DEV_COUNT_MAX; i++) {
            h2_xen_dev_destroy(global.ctx->hyp.ctx.xen, shell,
                                    &(shell->hyp.guest.xen->devs[i]));
        }
        goto out_free;
    }

    return shell;

out_free:
    h2_xen_domain_destroy(global.ctx->hyp.ctx.xen, shell);
    return NULL;
}

int precreate_shells(unsigned long shells, unsigned long memory)
{
    int i;
    h2_hyp_cfg cfg;

    if (shells > MAX_SHELLS) {
        NOTICE("Requested number of precreated shells is above maximum (%lu > %lu), only creating %lu shells.\n",
                shells, MAX_SHELLS, MAX_SHELLS);
        shells = MAX_SHELLS;
    }

    cfg.xen.xs.domid = 0;
    cfg.xen.xs.active = true;
#ifdef CONFIG_H2_XEN_NOXS
    cfg.xen.noxs.active = true;
#endif
    cfg.xen.xlib = h2_xen_xlib_t_xc;

    global.remaining_shells = 0;
    global.last_ipaddr = 0;
    for (i = 0; i < shells; i++) {
        global.shell[i] = precreate_shell(i, &cfg, memory);
        if (!(global.shell[i])) {
            ERROR("Precreating shell no %u failed, stopping precreation.\n", i);
            return -ENOMEM;
        }
        else {
            global.remaining_shells++;
        }
    }
    INFO("Precreated %u shells.\n", i);
    return 0;
}

int shell_daemon_cleanup(void) {
    int i;
    int ret = 0;

    unlink(sockname);

    INFO("Destroying %lu unused precreated shells...\n", global.remaining_shells);
    for (i = 0; i < global.remaining_shells; i++) {
        // save first potential error
        if (!ret) {
            ret = h2_guest_destroy(global.ctx, global.shell[i]);
        }
        else {
            ret = h2_guest_destroy(global.ctx, global.shell[i]);
        }
        h2_guest_free(&global.shell[i]);
    }
    h2_close(&global.ctx);

    return ret;
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

    precreate_shells(cmd.shells, cmd.memory);

    while (1) {
        if (global.shutting_down) {
            ret = shell_daemon_cleanup();
            INFO("Shutdown done.\n");
            break;
        }

        wait_for_sockdata();
    }

out:
    return ret;
}
