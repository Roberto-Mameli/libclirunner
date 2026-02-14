# Examples

The `examples/` directory contains three small programs that demonstrate the main usage patterns of `libclirunner`.

## **_example1.c_** - One-shot command execution
This example is the classical *"Hello, world"*, launched in a child process. It shows how to use the **one-shot API** to execute a command synchronously.

It demonstrates:

- Running a simple external program.
- Optionally providing input.
- Collecting `stdout` and `stderr`.
- Retrieving the final exit code.

It is intended as the simplest entry point and illustrates how to use the library for batch-style command execution where no interactive behavior is required.



## **_example2.c_** - Interactive session with timed stop

This example demonstrates the **interactive API** with a long-running command.

It shows how to:

- Start a CLI process in interactive mode.
- Register callbacks for:
  - `stdout`
  - `stderr`
  - process exit
- Let the child run for a fixed amount of time.
- Stop the session explicitly.
- Join and destroy the session safely.

This example is useful to understand how the worker thread and the callback-based architecture behave when managing continuously running commands.



## **_example3.c_** - Interactive input and graceful EOF handling

This example demonstrates a fully interactive workflow using a simple program (e.g., `cat`) to emulate user-driven input.

It illustrates how to:

- Start an interactive session.
- Send multiple inputs to the child process.
- Close the child’s `stdin` explicitly to signal EOF.
- Wait for the child to terminate naturally.
- Handle the final exit code via callback.

This example highlights correct lifecycle management in interactive mode, especially the difference between:

- closing `stdin` (graceful termination), and
- forcefully stopping the session.

Together, these three examples cover the core features of `libclirunner` and provide practical guidance for both simple and advanced usage scenarios.
