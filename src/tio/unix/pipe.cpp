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

#include <fcntl.h>
#include <unistd.h>

#include <tio/unix/pipe.hpp>

namespace tio::unix_ {

auto pipe_sender::write(std::span<const std::byte> buf) const -> result<std::size_t> {
  const ssize_t n = ::write(fd_.raw_fd(), buf.data(), buf.size());
  if (n < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return static_cast<std::size_t>(n);
}

auto pipe_sender::set_nonblocking(bool enable) const -> void_result {
  const int flags = ::fcntl(fd_.raw_fd(), F_GETFL);
  if (flags < 0) {
    return std::unexpected{error::last_os_error()};
  }
  const int new_flags = enable ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
  if (::fcntl(fd_.raw_fd(), F_SETFL, new_flags) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return {};
}

auto pipe_receiver::read(std::span<std::byte> buf) const -> result<std::size_t> {
  const ssize_t n = ::read(fd_.raw_fd(), buf.data(), buf.size());
  if (n < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return static_cast<std::size_t>(n);
}

auto pipe_receiver::set_nonblocking(bool enable) const -> void_result {
  const int flags = ::fcntl(fd_.raw_fd(), F_GETFL);
  if (flags < 0) {
    return std::unexpected{error::last_os_error()};
  }
  const int new_flags = enable ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
  if (::fcntl(fd_.raw_fd(), F_SETFL, new_flags) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return {};
}

auto make_pipe() -> result<std::pair<pipe_sender, pipe_receiver>> {
  int fds[2]{};
  if (::pipe2(fds, O_NONBLOCK | O_CLOEXEC) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return std::pair{pipe_sender{detail::fd_guard{fds[1]}}, pipe_receiver{detail::fd_guard{fds[0]}}};
}

}
