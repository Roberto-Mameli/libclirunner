// examples/example1.c
#include <stdio.h>
#include <string.h>
#include "clirunner.h"

int main(void)
{
    /* Definitions */
    char *const argv[] = { "echo", "Hello, world", NULL };
    oneshot_result_t res;

    if (run_oneshot("echo", argv,
                    NULL, 0,
                    3000,
                    &res) != 0)
    {
        perror("run_oneshot");
        return 1;
    }

    printf("exit code: %d\n", res.exit_code);
    printf("stdout:\n%.*s", (int)res.out_len, res.out);
    printf("stderr:\n%.*s", (int)res.err_len, res.err);

    oneshot_result_free(&res);
    return 0;
}
