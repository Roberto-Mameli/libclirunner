- [Disclaimer](#disclaimer)
- [Introduction](#introduction)
- [Key Features](#key-features)
- [How to Build and Install *libclirunner*](#how-to-build-and-install-libclirunner)
- [How to Use *libclirunner*](#how-to-use-libclirunner)
- [Comparison with Other C/C++ Libraries](#comparison-with-other-cc-libraries)
  - [1. *Boost.Process (C++)*](#1-boostprocess-c)
  - [2. *Qt – QProcess (C++)*](#2-qt--qprocess-c)
  - [3. *GLib/GIO – GSubprocess (C)*](#3-glibgio--gsubprocess-c)
  - [4. *POCO C++ Process*](#4-poco-c-process)
  - [5. *libuv – uv\_spawn() (C)*](#5-libuv--uv_spawn-c)
  - [6. *POSIX `posix_spawn()`*](#6-posix-posix_spawn)
  - [Summary of Advantages of *libclirunner*](#summary-of-advantages-of-libclirunner)
- [When Should You Use *libclirunner*?](#when-should-you-use-libclirunner)
- [Known Limitations](#known-limitations)


# Disclaimer
This software is licensed under the terms of the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at:

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.


# Introduction
*libclirunner* is a compact and robust C library that provides *Minimal*, *Safe*, *Dependency-Free* C Library for Managing External Processes.

*libclirunner* uses `pipes`, `poll()` and `threads` and provides the following capabilities:

- Capture stdout/stderr
- Optional stdin payload
- Timeout handling
- Interactive sessions with callbacks
- No shell invocation

It supports both:

- **One-shot commands**: run, capture output, wait for termination
- **Interactive foreground sessions**: bidirectional communication via stdin/stdout/stderr, with callbacks and non-blocking `poll()`

It does not support (yet) long-running background processes (e.g daemons), which are planned for future releases.

The library is designed for applications that need **fine-grained control** over child processes without pulling large frameworks, event loops, or heavyweight C++ ecosystems.


# Key Features
*libclirunner* provides the following features:
- **Pure C, Zero Dependencies, Portable**: the library uses only POSIX system calls: `fork()`, `execvp()`, `pipe()`, `poll()`, `dup2()`, and `pthread` for streaming threads. It does not depends on external libraries.

- **Shell-Free Execution (Security by Design)**: commands are executed via `execvp()` with explicit `argv[]`, avoiding the risks of shell interpolation or command injection.

- **Unified Abstraction for One-Shot or Long-Running Commands**: the library provides disticnt function calls for:
  - **One-Shot API**: run → capture stdout/stderr → join → exit code.
  - **Session API**: spawn in a dedicated thread → stream stdout/stderr via callbacks → write to stdin → stop gracefully.

- **Fully Non-Blocking Streaming with `poll()`**: output is consumed continuously (avoiding pipe buffer deadlocks), using POSIX `poll()` semantics.

- **Small, Single-File, Embeddable**: ideal for embedding into complex systems, microservices, brokers, or C/C++ applications that need lightweight but robust process orchestration.


# How to Build and Install *libclirunner*
The library can be cloned from the GitHub repository with the following command:

      git clone https://github.com/Roberto-Mameli/libclirunner.git
      cd libclirunner

To build it, run the following command:

    make all

This command compiles the library and produces in the *libclirunner/lib* directory both the static and the shared libraries (respectively *libclirunner.a* and *libclirunner.so.*).

After that, the dynamic library and the related header can be installed through:

    sudo make install

(be aware that *sudo* is not needed if logged as *root*).

This command installs the libraries in the destination folders, respectively */usr/local/lib* for the dynamic library (*libclirunner.so.*) and */usr/local/include* for the header file *clirunner.h*. Be aware that, in order to use shared libraries, this path shall be either configured in */etc/ld.so.conf* or in environment variable **$LD_LIBRARY_PATH**.

The first time you install the libraries, a further command might be needed to configure dynamic linker run-time bindings:

    ldconfig

Finally, the following commands can be used respectively to clean and to uninstall libraries:

    make clean
    sudo make uninstall


# How to Use *libclirunner*
After compiling and installing the *libclirunner* library (see previous section), it can be linked statically or dynamically to C/C++ code.

This is obtained as follows:

− include the *clirunner.h* header file

    #include "clirunner.h"

− link the executable by including either the shared or the static library libclirunner


To compile a generic example file (let's say **_example.c_** ), simply type:

- for shared library linking:

      gcc -g -c -O2 -Wall -v –I/usr/local/include example.c
      gcc -g -o example example.c - lclirunner

- for static linking:

      gcc -static example.c - I/usr/local/include -L. -lclirunner - o example


In the previous command **_- L._** means that the **_libclirunner.a_** file is available in the same directory of the source code **_example.c_** ; if this is not the case just replace the dot after **_L_** with the path to the library file.


# Comparison with Other C/C++ Libraries
Below is a synthetic, high-level comparison with the most widely used libraries.

## 1. *[Boost.Process](https://www.boost.org/doc/libs/latest/libs/process/doc/html/index.html) (C++)*

**Description:** A full-featured, cross-platform C++ library supporting synchronous and asynchronous process management, integrated with Boost.Asio.

**Pros**:
- Very rich API (env, cwd, pipelines, customizable stdio redirection).
- First-class support for asynchronous pipes via *Boost.Asio*
- High portability (Windows/Linux/macOS).

**Cons**:
- Heavy dependency (requires Boost).
- C++-only; unsuitable for C projects.
- Much more complex than needed for small/embedded use cases.

**Compared to *libclirunner***:
- **Heavier**, **less minimal**, more “enterprise-scale”.
- *libclirunner* is **pure C**, zero dependencies, much simpler.


## 2. *[Qt – QProcess](https://doc.qt.io/qt-6/qprocess.html) (C++)*

**Description:** A QtCore class offering easy process spawning integrated with Qt’s event loop.

**Pros**:
- Very simple asynchronous I/O using Qt’s signals and slots.
- Treats processes like `QIODevice`, with ready-read signals.
- Excellent when you already depend on Qt.

**Cons**:
- Requires *QtCore*, which is a large framework
- Not suitable for C-only projects
- Tied to Qt’s event loop
- 
**Compared to *libclirunner***:
- QProcess is great if you’re writing Qt applications; otherwise it's too large.
- *libclirunner* has **no event loop**, no Qt runtime, and remains minimal and portable.


## 3. *[GLib/GIO – GSubprocess](https://gnome.pages.gitlab.gnome.org/libsoup/gio/GSubprocess.html) (C)*

**Description:** Advanced GLib/GIO API for spawning, streaming, communicating with child processes.

**Pros**:
- Very complete API (`communicate()`, async I/O, pipes, signals).
- Automatic FD sanitation and correct cleanup.
- Excellent integration with GLib’s main loop.

**Cons**:
- Requires GLib/GIO runtime (heavy dependency).
- Programming model strongly tied to GObject/GLib abstractions.

**Compared to *libclirunner***:
- GSubprocess is larger, more complex, and depends on GLib.
- *libclirunner* keeps a **pure POSIX**, dependency-free approach.


## 4. *[POCO C++ Process](https://docs.pocoproject.org/current/Poco.Process.html)*

**Description:** High-level API for spawning processes using POCO Foundation.

**Pros**:
- Good integration with POCO streams and IPC.
- Multi-platform.

**Cons**:
- Requires the full POCO Foundation library.
- C++ only.
- Pipes require POCO stream wrappers.

**Compared to *libclirunner***:
- Heavier and bound to a specific ecosystem.
- *libclirunner* is **tiny, embeddable, standalone C**.


## 5. *[libuv – uv\_spawn()](https://docs.libuv.org/en/v1.x/guide/processes.html) (C)*

**Description:** Cross-platform event-loop-centric library.

**Pros**:
- Cross-platform abstraction of spawn, pipes, async I/O.
- Fits perfectly in libuv-based event loops.

**Cons**:
- Requires adopting the libuv event model.
- More complex than pure POSIX usage.
- Non-trivial integration into existing systems not using libuv.

**Compared to *libclirunner***:
- *libclirunner* does not require an event loop.
- Much simpler; ideal if you do not use libuv.



## 6. *[POSIX `posix_spawn()`](https://www.man7.org/linux/man-pages/man3/posix_spawn.3.html)*

**Description:** Standard API on POSIX systems for light-weight spawning.

**Pros**:
- Lighter than fork/exec in constrained systems.
- Standardized and portable.

**Cons**:
- Limited flexibility compared to `fork()+exec()`.
- Piping requires manual setup of file actions.

**Compared to *libclirunner***:
- *libclirunner* provides **pipe setup, streaming threads, callbacks**, and higher-level abstractions missing in raw POSIX APIs.



## Summary of Advantages of *libclirunner*
| Feature                                          | *libclirunner* | Others                                                                      |
| ------------------------------------------------ | ------------- | --------------------------------------------------------------------------- |
| *Zero dependencies*                           |             |  Boost, Qt, GLib, POCO all require heavy runtimes                          |
| *Pure C*                                      |             |  Boost/Qt/POCO are C++                                                     |
| *Tiny, embeddable*                            |             |  Heavy frameworks                                                          |
| *Non-blocking streaming via poll()*           |             | Implemented differently or requires event loop (Qt, GLib, libuv)            |
| *Thread-based streaming callbacks*            |             | Usually event-loop based or more complex                                    |
| *Explicit separation: one-shot vs interactive* |             | Most libraries provide only unified API                                     |



# When Should You Use *libclirunner*?
Use this library if you want:

- A **minimal and portable C solution** for spawning and interacting with external programs
- A **robust, safe**, shell-free execution environment
- Continuous capture of stdout/stderr without relying on frameworks
- An easy separation between short-lived and interactive commands
- A library that “just works” on any POSIX system with no build dependencies


# Known Limitations
Support for long-running background processes (e.g daemons) is not yet available in the current version. It will be implemented in a future release.