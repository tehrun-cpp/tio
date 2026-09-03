/*
 * Copyright (c) 2025 tio project
 *
 * This is the source code of the tio project.
 * It is licensed under the MIT License; you should have received a copy
 * of the license in this archive (see LICENSE).
 *
 * Author: Abolfazl Abbasi
 *
 */

#pragma once

#include <tio/sys/selector.hpp>

#if defined(TIO_BACKEND_EPOLL)
  #include <tio/sys/unix_/eventfd_waker.hpp>
#elif defined(TIO_BACKEND_KQUEUE)
  #include <tio/sys/unix_/kqueue_waker.hpp>
#elif defined(TIO_BACKEND_IO_URING)
  #error "io_uring backend not yet implemented"
#else
  #error "No TIO_BACKEND_* defined. Set TIO_BACKEND in CMake."
#endif

namespace tio::sys {

#if defined(TIO_BACKEND_EPOLL)
using waker = unix::eventfd_waker;
#elif defined(TIO_BACKEND_KQUEUE)
using waker = unix::kqueue_waker;
#endif

}
