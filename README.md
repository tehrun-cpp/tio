# tio

[![Build](https://github.com/tehrun-cpp/tio/actions/workflows/build.yml/badge.svg)](https://github.com/tehrun-cpp/tio/actions/workflows/build.yml)
[![Platforms](https://img.shields.io/badge/platforms-Linux%20%7C%20macOS%20%7C%20iOS%20%7C%20Android-lightgrey.svg)]()
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-0.2.0-green.svg)]()

Low-level, non-blocking I/O for C++26. Edge-triggered, zero-cost abstractions over epoll and kqueue ready for production use in Linux and macOS, Android and iOS.

Part of the [tehrun](https://github.com/tehrun-cpp) project.

## What is tio?

tio is a portable event-driven I/O library inspired by Rust's [mio](https://github.com/tokio-rs/mio). It gives you:

- **Non-blocking TCP, UDP, Unix sockets, and pipes** with a unified registration API
- **Edge-triggered epoll and kqueue** behind one clean, type-safe interface
- **`std::expected`-based error handling** — no exceptions, no RTTI
- **Zero-cost strong types** for tokens, interests, and events
- **Move-only RAII** for all file descriptors — no leaks by design

tio is the I/O foundation layer. It does not provide async/await, task scheduling, or timers — those belong in higher-level runtime libraries built on top of
tio.

## Requirements

- Linux or Android API 21+ (epoll backend), or macOS 13.3+ / iOS 16.3+ / tvOS 16.4+ /
  watchOS 9.4+ / visionOS (kqueue backend)
- C++26 (`-std=c++2c`)
- Clang 19+ (primary) or GCC 14+ (secondary); Android needs NDK 30+
- CMake 3.30+

The Apple deployment-target floors come from libc++, not from tio: `std::format` needs
`std::to_chars` for floating point, which is unavailable in earlier system libraries.

The Android NDK floor is libc++ too: the NDK 28 copy rejects every user-defined
`std::formatter` specialization, so tio's formatters for `error`, `token`, `interest` and the
address types do not compile there. The library itself builds on NDK 28; only formatting a tio
type does not.

## Build

```bash
cmake -B build -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Run tests (requires [Google Test](https://github.com/google/googletest)):

```bash
cmake --build build --target test
```

### CMake options

| Option               | Default           | Description                                         |
|----------------------|-------------------|-----------------------------------------------------|
| `TIO_BUILD_TESTS`    | `ON`              | Build unit tests                                    |
| `TIO_BUILD_EXAMPLES` | `ON`              | Build example programs                              |
| `TIO_BACKEND`        | per platform      | I/O backend (`epoll`, `kqueue`, `io_uring` planned) |

`TIO_BACKEND` defaults to `epoll` on Linux and `kqueue` on Apple platforms.

### Cross-compiling for iOS

```bash
cmake -B build-ios -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_DEPLOYMENT_TARGET=16.3 \
  -DTIO_BUILD_TESTS=OFF -DTIO_BUILD_EXAMPLES=OFF
cmake --build build-ios
```

### Cross-compiling for Android

```bash
cmake -B build-android \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-21 \
  -DTIO_BUILD_TESTS=OFF -DTIO_BUILD_EXAMPLES=OFF
cmake --build build-android
```

Android uses the same epoll backend as Linux — bionic provides `epoll`, `eventfd`, `accept4`
and `pipe2`. Note that an app can only create filesystem-path Unix sockets inside its own data
directory; SELinux denies `sock_file` creation elsewhere. `unix_stream::pair()` and
`unix_datagram::pair()` use `socketpair` and are unaffected.

### Use as a subdirectory

```cmake
add_subdirectory(tio)
target_link_libraries(your_target PRIVATE tio::tio)
```

## Quick start

### TCP echo server

```cpp
#include <array>
#include <print>

#include <tio/tio.hpp>

using namespace tio;
using namespace tio::net;

int main() {
    auto poll     = poll::create().value();
    auto listener = tcp_listener::bind(detail::socket_addr::ipv4_any(9000)).value();
    auto reg      = poll.get_registry();

    // register the listener for readable events
    auto listener_tok = token{0};
    reg.register_source(listener, listener_tok, interest::readable()).value();

    events evs{1024};
    std::size_t next_id = 1;

    while (true) {
        poll.do_poll(evs, std::nullopt).value();

        for (const auto& ev : evs) {
            if (ev.tok() == listener_tok) {
                // accept new connections until EAGAIN
                while (true) {
                    auto result = listener.accept();
                    if (!result) {
                        if (result.error().is_would_block()) break;
                        std::println(stderr, "accept: {}", result.error());
                        break;
                    }
                    auto& [stream, peer] = *result;
                    auto tok = token{next_id++};
                    std::println("accepted {} as {}", peer, tok);
                    reg.register_source(stream, tok, interest::readable()).value();
                    // ... store stream somewhere
                }
            }
        }
    }
}
```

### UDP send and receive

```cpp
#include <array>
#include <print>

#include <tio/tio.hpp>

using namespace tio;
using namespace tio::net;

int main() {
    auto sender   = udp_socket::bind(detail::socket_addr::ipv4_any(0)).value();
    auto receiver = udp_socket::bind(detail::socket_addr::ipv4_loopback(4000)).value();

    auto msg = std::array<std::byte, 5>{
        std::byte{'h'}, std::byte{'e'}, std::byte{'l'}, std::byte{'l'}, std::byte{'o'}
    };

    auto dest = detail::socket_addr::ipv4_loopback(4000);
    sender.send_to(msg, dest).value();

    std::array<std::byte, 1024> buf{};
    auto [n, from] = receiver.recv_from(buf).value();
    std::println("received {} bytes from {}", n, from);
}
```

### Polling multiple sources

```cpp
#include <tio/tio.hpp>

using namespace tio;

int main() {
    auto poll = poll::create().value();
    auto reg  = poll.get_registry();

    // any type satisfying the `source` concept can register:
    //   tcp_listener, tcp_stream, udp_socket,
    //   unix_listener, unix_stream, unix_datagram,
    //   pipe_sender, pipe_receiver, raw_fd

    auto listener = net::tcp_listener::bind(detail::socket_addr::ipv4_any(8080)).value();
    reg.register_source(listener, token{0}, interest::readable()).value();

    auto [tx, rx] = unix_::make_pipe().value();
    reg.register_source(rx, token{1}, interest::readable()).value();

    events evs{256};

    while (true) {
        // blocks until at least one event fires (or timeout)
        poll.do_poll(evs, std::chrono::milliseconds{1000}).value();

        for (const auto& ev : evs) {
            if (ev.is_readable())    { /* drain the source */ }
            if (ev.is_read_closed()) { /* peer hung up */ }
            if (ev.is_error())       { /* check take_error() */ }

            /* flush pending writes, on any wakeup, until is_would_block() */
        }
    }
}
```

### Cross-thread wakeup

```cpp
#include <thread>
#include <tio/tio.hpp>

using namespace tio;

int main() {
    auto poll = poll::create().value();
    auto waker = waker::create(poll.get_registry(), token{42}).value();

    std::thread worker([waker] {
        // do some work ...
        waker.wake().value();  // wake the poll loop from another thread
    });

    events evs{64};
    poll.do_poll(evs, std::nullopt).value();

    for (const auto& ev : evs) {
        if (ev.tok() == token{42}) {
            // woken up by the worker thread
        }
    }

    worker.join();
}
```

## API overview

### Core types

| Type        | Header               | Description                                                 |
|-------------|----------------------|-------------------------------------------------------------|
| `poll`      | `<tio/poll.hpp>`     | Event loop — wraps the platform selector                    |
| `registry`  | `<tio/poll.hpp>`     | Registers/deregisters sources with a poll                   |
| `events`    | `<tio/event.hpp>`    | Reusable event buffer, iterable                             |
| `event`     | `<tio/event.hpp>`    | Single event — query `is_readable()`, `is_writable()`, etc. |
| `token`     | `<tio/token.hpp>`    | Strong-typed `size_t` identifying an event source           |
| `interest`  | `<tio/interest.hpp>` | Bitmask: `readable()`, `writable()`, `priority()`           |
| `error`     | `<tio/error.hpp>`    | Errno wrapper with named predicates                         |
| `result<T>` | `<tio/error.hpp>`    | Alias for `std::expected<T, error>`                         |
| `waker`     | `<tio/waker.hpp>`    | Thread-safe poll wakeup (eventfd / `EVFILT_USER`)           |

### Network types

| Type           | Header                       | Description                            |
|----------------|------------------------------|----------------------------------------|
| `tcp_listener` | `<tio/net/tcp_listener.hpp>` | Non-blocking TCP server socket         |
| `tcp_stream`   | `<tio/net/tcp_stream.hpp>`   | Non-blocking TCP connection            |
| `udp_socket`   | `<tio/net/udp_socket.hpp>`   | Non-blocking UDP socket with multicast |

### Unix types

| Type                            | Header                         | Description                   |
|---------------------------------|--------------------------------|-------------------------------|
| `unix_listener`                 | `<tio/unix/unix_listener.hpp>` | Unix domain stream listener   |
| `unix_stream`                   | `<tio/unix/unix_stream.hpp>`   | Unix domain stream connection |
| `unix_datagram`                 | `<tio/unix/unix_datagram.hpp>` | Unix domain datagram socket   |
| `pipe_sender` / `pipe_receiver` | `<tio/unix/pipe.hpp>`          | Unidirectional pipe pair      |

### Utilities

| Type          | Header                             | Description                                     |
|---------------|------------------------------------|-------------------------------------------------|
| `raw_fd`      | `<tio/raw_fd.hpp>`                 | Wrap any fd (timerfd, serial, etc.) as a source |
| `fd_guard`    | `<tio/sys/detail/fd_guard.hpp>`    | RAII fd wrapper — closes on destruction         |
| `socket_addr` | `<tio/sys/detail/socket_addr.hpp>` | IPv4/IPv6 address helper                        |

### The `source` concept

Any type that implements three methods can register with a poll:

```cpp
auto tio_register(const registry&, token, interest)   -> void_result;
auto tio_reregister(const registry&, token, interest)  -> void_result;
auto tio_deregister(const registry&)                   -> void_result;
```

All built-in types (`tcp_listener`, `udp_socket`, `pipe_receiver`, `raw_fd`, ...) satisfy this concept.

## Design principles

- **No exceptions, no RTTI.** Compiles with `-fno-exceptions -fno-rtti`.
- **Edge-triggered only.** Registrations use `EPOLLET` on epoll and `EV_CLEAR` on kqueue. You must drain reads/writes until `EAGAIN`.
- **Non-blocking by default.** All descriptors tio creates are non-blocking and close-on-exec.
- **Move-only ownership.** File descriptors cannot be copied. They close on destruction.
- **Compile-time backend selection.** No virtual dispatch. The backend (epoll, kqueue, io_uring) is chosen via CMake and resolved with `#if`/`using`.

## Writing portable event loops

The two backends report readiness differently, and a loop that only ever ran on epoll can
quietly depend on the difference:

- **One event per filter.** kqueue reports readable and writable as *separate* events for the
  same token; epoll coalesces them into one. Do not assume an event that is readable will also
  carry the writable flag — handle each independently, and flush pending writes whenever you
  have them rather than only on a writable event.
- **Tear down once, at the end of the batch.** The split above applies to the close and error
  predicates too: one peer close can produce three events for the same token, with
  `is_write_closed()` on the first and `is_read_closed()` on the last. Destroying the source
  from inside the loop leaves later events in the *same* batch pointing at a closed fd — mark
  the token dead and reap it after the `for` loop.
- **A wakeup is not a guarantee.** Spurious readiness is allowed on both backends, and kqueue
  re-arms its write filter whenever send-buffer space frees up. Always treat `EAGAIN` /
  `is_would_block()` as normal and loop.
- **Close predicates follow the registered filters on kqueue.** epoll reports `EPOLLHUP`
  whether or not you asked for it, so `is_write_closed()` can fire for a source registered
  `readable()` only. kqueue has no such filter to report on, so it cannot. Key teardown on
  `is_read_closed()` / `is_error()`, or register both directions.
- **Priority.** `interest::priority()` maps to `EPOLLPRI` on epoll and to `EVFILT_EXCEPT` with
  `NOTE_OOB` on kqueue, so it means out-of-band data there.
- **Registration errors.** `reregister_source` on a source that is not registered returns
  `ENOENT` on epoll; kqueue has no per-fd registry to consult and silently registers it
  instead. Do not use the error as a membership test.
- **Adopted descriptors.** Descriptors tio creates are non-blocking, close-on-exec, and — on
  platforms with `SO_NOSIGPIPE` / `F_SETNOSIGPIPE`, which includes Apple — protected against
  SIGPIPE for every write path. On Linux only `send`/`sendto` carry `MSG_NOSIGNAL`, so
  `write_vectored` still raises SIGPIPE there; ignore the signal process-wide if you use it.
  An fd passed to `from_raw_fd` keeps whatever configuration you gave it.

## License

MIT
