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

#ifndef CLIRUNNER_H
#define CLIRUNNER_H


/*****************
 * Include Files *
 *****************/
#include <stddef.h>
#include <sys/types.h>


#ifdef __cplusplus
extern "C" {
#endif


/*******************************
 * General Purpose Definitions *
 *******************************/


/********************
 * Type Definitions *
 ********************/
/* One-shot execution API */
typedef struct
{
    int     exit_code;   /* exit status or 128+signal */
    char   *out;         /* stdout buffer (malloc'd) */
    size_t  out_len;
    char   *err;         /* stderr buffer (malloc'd) */
    size_t  err_len;
} oneshot_result_t;

/* Interactive session API */
typedef struct cli_session cli_session_t;
typedef void (*cli_on_stdout)(cli_session_t *s, const char *buf, size_t n);
typedef void (*cli_on_stderr)(cli_session_t *s, const char *buf, size_t n);
typedef void (*cli_on_exit)(cli_session_t *s, int exit_code);
typedef struct
{
    cli_on_stdout on_stdout;
    cli_on_stderr on_stderr;
    cli_on_exit   on_exit;
    void         *user;
} cli_callbacks_t;


/***********************
 * Function Prototypes *
 ***********************/
/* One-shot execution API - Free buffers inside oneshot_result_t */
void oneshot_result_free(oneshot_result_t *r);

/* One-shot execution API - Execute a command and wait for completion
   - cmd           executable name (searched via PATH)
   - argv          argv array (argv[0] should be cmd)
   - stdin_payload optional input buffer
   - stdin_len     size of stdin_payload
   - timeout_ms    <0 = infinite
   - res_out       result structure (output buffers owned by caller)
   Returns 0 on success, -1 on error (errno set) */
int run_oneshot(const char *cmd,
                char *const argv[],
                const void *stdin_payload,
                size_t stdin_len,
                int timeout_ms,
                oneshot_result_t *res_out);


/* Interactive session API - Create an interactive CLI session */
/* Returns the pointer to the newly created session            */
cli_session_t *cli_session_create(void);

/* Interactive session API - Start an interactive CLI session */
/* Forks the process and launches the command cmd with its    */
/* arguments argv[] in the child. It returns only in the      */
/* parent, where it creates a thread which monitors the child */
/* and handles callback functions. It returns the result of   */
/* pthread_create(), i.e. 0 on success                        */
int cli_session_start(cli_session_t *s,
                      const char *cmd,
                      char *const argv[],
                      const cli_callbacks_t *cb);

/* Interactive session API - Write to child stdin */
/* Returns bytes written or -1 on error           */
ssize_t cli_session_write_stdin(cli_session_t *s,
                                const void *buf,
                                size_t n);

/* Interactive session API - Closes only the child stdin */
/* The CLI session cannot receive any input, but stdout  */
/* and stderr are still collected                        */
void cli_session_close_stdin(cli_session_t *s);

/* Interactive session API - Stop session
   - sig == 0 --> closes the thread in the parent,
                  leaves the child running
   - sig > 0  --> closes the thread in the parent,
                  send signal sig to child */
int cli_session_stop(cli_session_t *s, int sig);

/* Interactive session API - Wait for session thread to exit */
int cli_session_join(cli_session_t *s);

/* Interactive session API - Destroys an interactive CLI session */
/* previously created and releases all allocated resources       */
void cli_session_destroy(cli_session_t *s);


#ifdef __cplusplus
}
#endif

#endif /* CLIRUNNER_H */
