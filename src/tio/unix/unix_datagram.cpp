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

#include <tio/unix/unix_datagram.hpp>

namespace tio::unix_ {

auto unix_datagram::bind(const detail::unix_addr& addr) -> result<unix_datagram> {
  const int fd = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (fd < 0) {
    return std::unexpected{error::last_os_error()};
  }

  detail::fd_guard guard{fd};

  if (::bind(fd, addr.as_sockaddr(), addr.len()) < 0) {
    return std::unexpected{error::last_os_error()};
  }

  return unix_datagram{std::move(guard)};
}

auto unix_datagram::unbound() -> result<unix_datagram> {
  const int fd = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (fd < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return unix_datagram{detail::fd_guard{fd}};
}

auto unix_datagram::pair() -> result<std::pair<unix_datagram, unix_datagram>> {
  int fds[2]{};
  if (::socketpair(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, fds) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return std::pair{unix_datagram{detail::fd_guard{fds[0]}}, unix_datagram{detail::fd_guard{fds[1]}}};
}

auto unix_datagram::connect(const detail::unix_addr& addr) const -> void_result {
  if (::connect(fd_.raw_fd(), addr.as_sockaddr(), addr.len()) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return {};
}

auto unix_datagram::send_to(std::span<const std::byte> buf, const detail::unix_addr& addr) const
    -> result<std::size_t> {
  const ssize_t n =
      ::sendto(fd_.raw_fd(), buf.data(), buf.size(), MSG_NOSIGNAL, addr.as_sockaddr(), addr.len());
  if (n < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return static_cast<std::size_t>(n);
}

auto unix_datagram::recv_from(std::span<std::byte> buf) const
    -> result<std::pair<std::size_t, detail::unix_addr>> {
  sockaddr_un storage{};
  socklen_t len = sizeof(storage);

  const ssize_t n = ::recvfrom(fd_.raw_fd(),
                         buf.data(),
                         buf.size(),
                         0,
                         reinterpret_cast<sockaddr*>(&storage),
                         &len);
  if (n < 0) {
    return std::unexpected{error::last_os_error()};
  }

  auto sender = detail::unix_addr::from_raw(reinterpret_cast<sockaddr*>(&storage), len);
  return std::pair{static_cast<std::size_t>(n), sender};
}

auto unix_datagram::send(std::span<const std::byte> buf) const -> result<std::size_t> {
  const ssize_t n = ::send(fd_.raw_fd(), buf.data(), buf.size(), MSG_NOSIGNAL);
  if (n < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return static_cast<std::size_t>(n);
}

auto unix_datagram::recv(std::span<std::byte> buf) const -> result<std::size_t> {
  const ssize_t n = ::recv(fd_.raw_fd(), buf.data(), buf.size(), 0);
  if (n < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return static_cast<std::size_t>(n);
}

auto unix_datagram::peer_addr() const -> result<detail::unix_addr> {
  detail::unix_addr addr;
  socklen_t len = sizeof(sockaddr_un);
  if (::getpeername(fd_.raw_fd(), addr.as_sockaddr_mut(), &len) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  *addr.len_mut() = len;
  return addr;
}

auto unix_datagram::local_addr() const -> result<detail::unix_addr> {
  detail::unix_addr addr;
  socklen_t len = sizeof(sockaddr_un);
  if (::getsockname(fd_.raw_fd(), addr.as_sockaddr_mut(), &len) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  *addr.len_mut() = len;
  return addr;
}

auto unix_datagram::shutdown(int how) const -> void_result {
  if (::shutdown(fd_.raw_fd(), how) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return {};
}

auto unix_datagram::take_error() const -> result<error> {
  int val = 0;
  socklen_t len = sizeof(val);
  if (::getsockopt(fd_.raw_fd(), SOL_SOCKET, SO_ERROR, &val, &len) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return error{val};
}

}
