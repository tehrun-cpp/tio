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

#include <sys/socket.h>

#include <tio/unix/unix_listener.hpp>
#include <tio/unix/unix_stream.hpp>

namespace tio::unix_ {

auto unix_listener::bind(const detail::unix_addr& addr) -> result<unix_listener> {
  const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (fd < 0) {
    return std::unexpected{error::last_os_error()};
  }

  detail::fd_guard guard{fd};

  if (::bind(fd, addr.as_sockaddr(), addr.len()) < 0) {
    return std::unexpected{error::last_os_error()};
  }

  if (::listen(fd, SOMAXCONN) < 0) {
    return std::unexpected{error::last_os_error()};
  }

  return unix_listener{std::move(guard)};
}

auto unix_listener::accept() const -> result<std::pair<unix_stream, detail::unix_addr>> {
  sockaddr_un storage{};
  socklen_t len = sizeof(storage);

  const int fd = ::accept4(fd_.raw_fd(),
                     reinterpret_cast<sockaddr*>(&storage),
                     &len,
                     SOCK_NONBLOCK | SOCK_CLOEXEC);
  if (fd < 0) {
    return std::unexpected{error::last_os_error()};
  }

  auto peer = detail::unix_addr::from_raw(reinterpret_cast<sockaddr*>(&storage), len);
  return std::pair{unix_stream{detail::fd_guard{fd}}, peer};
}

auto unix_listener::local_addr() const -> result<detail::unix_addr> {
  detail::unix_addr addr;
  socklen_t len = sizeof(sockaddr_un);
  if (::getsockname(fd_.raw_fd(), addr.as_sockaddr_mut(), &len) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  *addr.len_mut() = len;
  return addr;
}

auto unix_listener::take_error() const -> result<error> {
  int val = 0;
  socklen_t len = sizeof(val);
  if (::getsockopt(fd_.raw_fd(), SOL_SOCKET, SO_ERROR, &val, &len) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return error{val};
}

}
