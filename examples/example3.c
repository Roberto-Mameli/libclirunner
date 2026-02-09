// examples/example3.c
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "clirunner.h"

static void on_stdout(cli_session_t *s, const char *buf, size_t n)
{
    (void)s;
    fwrite(buf, 1, n, stdout);
    fflush(stdout);
}

static void on_stderr(cli_session_t *s, const char *buf, size_t n) {
    (void)s;
    fwrite(buf, 1, n, stderr);
    fflush(stderr);
}

static void on_exit(cli_session_t *s, int code)
{
    (void)s;
    fprintf(stderr, "\n[child exited with code %d]\n", code);
}

int main(void)
{
    /* Definitions */
    cli_session_t  *sess;
    cli_callbacks_t cb = { .on_stdout = on_stdout,
                           .on_stderr = on_stderr,
                           .on_exit   = on_exit   };
    char *const     argv[] = { "cat", NULL };

    sess = cli_session_create();
    if (!sess)
    {
        perror("cli_session_create");
        return 1;
    }

    if (cli_session_start(sess, "cat", argv, &cb) != 0)
    {
        perror("cli_session_start");
        cli_session_destroy(sess);
        return 1;
    }

    /* The following array emulates a user that provides some input */
    const char *choices[] = {"1\n","2\n","q\n" };

    for (size_t i = 0; i < 3; ++i)
    {
        cli_session_write_stdin(sess,
                                choices[i],
                                strlen(choices[i]));
        sleep (1);
    }

    /* Close stdin, but do not close the child process  */
    /* with cli_session_stop(), since it will terminate */
    /* when stdin is closed (EOF received by cat)       */
    cli_session_close_stdin(sess);
    cli_session_join(sess);
    cli_session_destroy(sess);

    return 0;
}

