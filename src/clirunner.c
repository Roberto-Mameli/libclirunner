/**********************************************************************************/
/*  ---------------------------------------                                       */
/*  C/C++ CLI Runner Library (libclirunner)                                       */
/*  ---------------------------------------                                       */
/*  Copyright 2026 Roberto Mameli                                                 */
/*                                                                                */
/*  Licensed under the Apache License, Version 2.0 (the "License");               */
/*  you may not use this file except in compliance with the License.              */
/*  You may obtain a copy of the License at                                       */
/*                                                                                */
/*      http://www.apache.org/licenses/LICENSE-2.0                                */
/*                                                                                */
/*  Unless required by applicable law or agreed to in writing, software           */
/*  distributed under the License is distributed on an "AS IS" BASIS,             */
/*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.      */
/*  See the License for the specific language governing permissions and           */
/*  limitations under the License.                                                */
/*  ------------------------------------------------------------------------      */
/*                                                                                */
/*  FILE:        libclirunner source file                                         */
/*  VERSION:     1.0.0                                                            */
/*  AUTHOR(S):   Roberto Mameli                                                   */
/*  PRODUCT:     Library libclirunner                                             */
/*  DESCRIPTION: Compact and robust C library that provides a minimal, safe and   */
/*               dependency-free C Library for managing external processes        */
/*  REV HISTORY: See updated Revision History in file CHANGELOG.md                */
/*  NOTE WELL:   If an application needs services and functions from this         */
/*               API, it MUST necessarily:                                        */
/*               - include the library header file                                */
/*                    #include "clirunner.h"                                      */
/*               - be linked by including either the shared or the static         */
/*                 library libclirunner                                           */
/**********************************************************************************/


/*****************
 * Include Files *
 *****************/
#include "clirunner.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>


/********************
 * Type Definitions *
 ********************/
/* Type definition for Internal Functions */
 typedef struct
{
    char  *data;
    size_t len;
    size_t cap;
} dynbuf_t;

/* Type definition for Process Spawning */
typedef struct
{
    int   in_w;
    int   out_r;
    int   err_r;
    pid_t pid;
} child_pipes_t;


/******************************
 * Global variables and types *
 ******************************/
/* Opaque struct referenced outside through cli_session_t type (defined in clirunner.h) */
struct cli_session {
    pthread_t     th;
    child_pipes_t cp;
    cli_callbacks_t cb;
    int           ctl_pipe[2];
    atomic_bool   running;
};


/*******************************
 * Static Functions (Internal) *
 *******************************/
static void db_init(dynbuf_t *b)
{
    b->data = NULL;
    b->len = b->cap = 0;
}

static void db_free(dynbuf_t *b)
{
    free(b->data);
    b->data = NULL;
    b->len = b->cap = 0;
}

static int db_reserve(dynbuf_t *b, size_t need)
{
    /* Local Variables */
    char  *p;
    size_t ncap;
    
    if (need <= b->cap)
        return 0;

    ncap = b->cap ? b->cap : 4096;
    while (ncap < need)
        ncap *= 2;

    p = realloc(b->data, ncap);
    
    if (!p)
        return -1;

    b->data = p;
    b->cap = ncap;
    
    return 0;
}

static int db_append(dynbuf_t *b, const void *src, size_t n)
{
    if (!n)
        return 0;

    if (db_reserve(b, b->len + n + 1))
        return -1;
    memcpy(b->data + b->len, src, n);
    b->len += n;
    b->data[b->len] = '\0';

    return 0;
}

static int64_t now_ms(void)
{
    /* Local Variables */
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int set_nonblock(int fd)
{
    int fl = fcntl(fd, F_GETFL, 0);

    return (fl < 0) ? -1 : fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

static int write_all(int fd, const void *buf, size_t len, int timeout_ms)
{
    const uint8_t *p = buf;
    size_t left = len;
    int64_t deadline = (timeout_ms >= 0) ? now_ms() + timeout_ms : -1;

    while (left > 0)
    {
        ssize_t n = write(fd, p, left);

        if (n > 0)
        {
            p += n;
            left -= n;
            continue;
        }
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            if (deadline >= 0 && now_ms() > deadline)
            {
                errno = ETIMEDOUT;
                return -1;
            }
            struct pollfd pfd = { fd, POLLOUT, 0 };
            poll(&pfd, 1, 50);
            continue;
        }
        return -1;
    }
    return 0;
}

static int spawn_with_pipes(const char *cmd, char *const argv[], child_pipes_t *cp)
{
    int   in_p[2],
          out_p[2],
          err_p[2];
    pid_t pid;

    if (pipe(in_p) || pipe(out_p) || pipe(err_p))
        return -1;

    pid = fork();

    if (pid < 0)
        return -1;

    if (pid == 0)
    {
        dup2(in_p[0], STDIN_FILENO);
        dup2(out_p[1], STDOUT_FILENO);
        dup2(err_p[1], STDERR_FILENO);

        close(in_p[0]); close(in_p[1]);
        close(out_p[0]); close(out_p[1]);
        close(err_p[0]); close(err_p[1]);

        execvp(cmd, argv);
        _exit(127);
    }

    close(in_p[0]);
    close(out_p[1]);
    close(err_p[1]);

    cp->pid = pid;
    cp->in_w = in_p[1];
    cp->out_r = out_p[0];
    cp->err_r = err_p[0];

    set_nonblock(cp->in_w);
    set_nonblock(cp->out_r);
    set_nonblock(cp->err_r);

    return 0;
}

static void *session_thread(void *arg)
{
    /* Local Variables */
    cli_session_t *s = arg;
    char           buf[8192];
    int            open,
                   status,
                   exit_code,
                   r,
                   i;
    ssize_t        n;

    atomic_store(&s->running, true);

    struct pollfd pfds[3] = { { s->cp.out_r, POLLIN, 0 },
                              { s->cp.err_r, POLLIN, 0 },
                              { s->ctl_pipe[0], POLLIN, 0 } };

    open = 2;

    while (atomic_load(&s->running) && open > 0)
    {
        r = poll(pfds, 3, 2000);
        if (r < 0)
        {
            if (errno == EINTR) continue;
            break; /* Critical Error --> exit from the loop */
        }
        if (r == 0) continue; /* Timeout --> loop again */

        if (pfds[2].revents & POLLIN)
            break;

        for (i = 0; i < 2; i++)
        {
            if (pfds[i].fd < 0)
                continue;
            if (pfds[i].revents & (POLLIN | POLLHUP | POLLERR))
            {
                while ((n = read(pfds[i].fd, buf, sizeof(buf))) > 0)
                {
                    if (i == 0 && s->cb.on_stdout)
                        s->cb.on_stdout(s, buf, n);
                    if (i == 1 && s->cb.on_stderr)
                        s->cb.on_stderr(s, buf, n);
                }
                if (n == 0)
                {
                    /* EOF --> close descriptor */
                    close(pfds[i].fd);
                    pfds[i].fd = -1;
                    open--;
                }
            }
        }
    }

    waitpid(s->cp.pid, &status, 0);
    exit_code = WIFEXITED(status) ? WEXITSTATUS(status) :  WIFSIGNALED(status) ? 128 + WTERMSIG(status) : -1;

    if (s->cb.on_exit)
        s->cb.on_exit(s, exit_code);

    return NULL;
}


/***************************
 *  Public Functions (API) *
 ***************************/
/* One-shot execution API - Free buffers inside oneshot_result_t */
void oneshot_result_free(oneshot_result_t *r)
{
    if (!r) return;
    free(r->out);
    free(r->err);
    memset(r, 0, sizeof(*r));
}

/* One-shot execution API - Execute a command and wait for completion
   - cmd           executable name (searched via PATH)
   - argv          argv array (argv[0] should be cmd)
   - stdin_payload optional input buffer
   - stdin_len     size of stdin_payload
   - timeout_ms    <0 = infinite
   - res_out       result structure (output buffers owned by caller)
   Returns 0 on success, -1 on error (errno set) */
int run_oneshot(const char *cmd, char *const argv[],
                const void *stdin_payload, size_t stdin_len,
                int timeout_ms,
                oneshot_result_t *res)
{
    /* Local Variables */
    child_pipes_t   cp;
    dynbuf_t        bout,
                    berr;
    int             open,
                    tmo,
                    status,
                    r,
                    i;
    char            buf[8192];
    int64_t         deadline,
                    now;
    ssize_t         n;

    if (!cmd || !argv || !res)
    {
        errno = EINVAL;
        return -1;
    }
    memset(res, 0, sizeof(*res));
    signal(SIGPIPE, SIG_IGN);

    if (spawn_with_pipes(cmd, argv, &cp) < 0)
        return -1;
    
    db_init(&bout);
    db_init(&berr);

    if (stdin_payload && stdin_len)
    {
        if (write_all(cp.in_w, stdin_payload, stdin_len, timeout_ms) < 0)
            goto fail;
    }
    close(cp.in_w);
    cp.in_w = -1;

    struct pollfd pfds[2] = { { cp.out_r, POLLIN, 0 },
                              { cp.err_r, POLLIN, 0 }  };

    open = 2;
    deadline = (timeout_ms >= 0) ? now_ms() + timeout_ms : -1;

    while (open > 0)
    {
        tmo = -1;
        if (deadline >= 0)
        {
            now = now_ms();
            if (now >= deadline) goto timeout;
            tmo = (int)(deadline - now);
        }

        r = poll(pfds, 2, tmo);
        if (r < 0 && errno != EINTR)
            goto fail;
        if (r == 0)
            goto timeout;

        for (i = 0; i < 2; i++)
        {
            if (pfds[i].fd < 0)
                continue;
            if (pfds[i].revents & (POLLIN | POLLHUP | POLLERR))
            {
                for (;;)
                {
                    n = read(pfds[i].fd, buf, sizeof(buf));
                    if (n > 0)
                    {
                        db_append(i ? &berr : &bout, buf, n);
                    }
                    else if (n == 0 || errno == EAGAIN)
                    {
                        close(pfds[i].fd);
                        pfds[i].fd = -1;
                        open--;
                        break;
                    } else if (errno != EINTR)
                    {
                        goto fail;
                    }
                }
            }
        }
    }

    waitpid(cp.pid, &status, 0);
    res->exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : WIFSIGNALED(status) ? 128 + WTERMSIG(status) : -1;
    res->out = bout.data; res->out_len = bout.len;
    res->err = berr.data; res->err_len = berr.len;

    return 0;

timeout:
    kill(cp.pid, SIGTERM);
    poll(NULL, 0, 200);
    kill(cp.pid, SIGKILL);
    waitpid(cp.pid, NULL, 0);

fail:
    {
        int e = errno;
        db_free(&bout); db_free(&berr);
        waitpid(cp.pid, NULL, WNOHANG);
        errno = e;
        return -1;
    }
}


/* Interactive session API - Create an interactive CLI session */
/* Returns the pointer to the newly created session            */
cli_session_t *cli_session_create(void)
{
    return calloc(1, sizeof(struct cli_session));
}

/* Interactive session API - Start an interactive CLI session */
/* Forks the process and launches the command cmd with its    */
/* arguments argv[] in the child. It returns only in the      */
/* parent, where it creates a thread which monitors the child */
/* and handles callback functions. It returns the result of   */
/* pthread_create(), i.e. 0 on success                        */
int cli_session_start(cli_session_t *s,
                      const char *cmd,
                      char *const argv[],
                      const cli_callbacks_t *cb)
{

    if (!s || !cmd || !argv)
    {
        errno = EINVAL;
        return -1;
    }

    memset(s, 0, sizeof(*s));
    if (cb)
        s->cb = *cb;

    if (spawn_with_pipes(cmd, argv, &s->cp) < 0)
        return -1;

    //pipe(s->ctl_pipe);
    if (pipe(s->ctl_pipe) < 0)
    {
        int e = errno;
        kill(s->cp.pid, SIGKILL);
        errno = e;
        return -1;
    }

    set_nonblock(s->ctl_pipe[0]);
    set_nonblock(s->ctl_pipe[1]);

    return pthread_create(&s->th, NULL, session_thread, s);
}

/* Interactive session API - Write to child stdin */
/* Returns bytes written or -1 on error           */
ssize_t cli_session_write_stdin(cli_session_t *s, const void *buf, size_t n)
{
    return (!s || s->cp.in_w < 0) ? -1 : write(s->cp.in_w, buf, n);
}

/* Interactive session API - Closes only the child stdin */
/* The CLI session cannot receive any input, but stdout  */
/* and stderr are still collected                        */
void cli_session_close_stdin(cli_session_t *s)
{
    if (s && s->cp.in_w != -1)
    {
        close(s->cp.in_w);
        s->cp.in_w = -1;
    }
}

/* Interactive session API - Stop session
   - sig == 0 --> closes the thread in the parent,
                  leaves the child running
   - sig > 0  --> closes the thread in the parent,
                  send signal sig to child */
int cli_session_stop(cli_session_t *s, int sig)
{

    if (!s)
        return -1;

    atomic_store(&s->running, false);

    if (sig > 0)
        kill(s->cp.pid, sig);

    if (write(s->ctl_pipe[1], "X", 1))
    {
        /* ignore: pipe may be closed */
    };

    return 0;
}

/* Interactive session API - Wait for session thread to exit */
int cli_session_join(cli_session_t *s)
{
    if (!s) return -1;

    pthread_join(s->th, NULL);

    return 0;
}

/* Interactive session API - Destroys an interactive CLI session */
/* previously created and releases all allocated resources       */
void cli_session_destroy(cli_session_t *s)
{
    if (!s) return;
    free(s);
}
