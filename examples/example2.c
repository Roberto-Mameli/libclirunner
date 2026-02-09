// examples/example2.c
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include "clirunner.h"

static void on_stdout(cli_session_t *s, const char *buf, size_t n)
{
    (void)s;
    fwrite(buf, 1, n, stdout);
    fflush(stdout);
}

static void on_exit(cli_session_t *s, int code)
{
    (void)s;
    fprintf(stderr, "\n[process exited with code %d]\n", code);
}

int main(void)
{
    /* Definitions */
    cli_session_t  *sess;
    cli_callbacks_t cb = { .on_stdout = on_stdout,
                           .on_exit   = on_exit    };

    char *const     argv[] = { "yes", "Hello, world", NULL };

    
    sess = cli_session_create();
    if (!sess)
    {
        perror("cli_session_create");
        return 1;
    }

    if (cli_session_start(sess, "yes", argv, &cb) != 0) {
        perror("cli_session_start");
        cli_session_destroy(sess);
        return 1;
    }

    /* Let's close immediately stdin */
    cli_session_close_stdin(sess);

    /* Let's wait 10 seconds */
    sleep(10);

    /* End the tail command running in the child process */
    cli_session_stop(sess, SIGTERM);
    cli_session_join(sess);
    cli_session_destroy(sess);

    return 0;
}

