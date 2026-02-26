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

#include <netinet/in.h>
#include <sys/socket.h>
#include <tio/net/tcp_listener.hpp>

namespace tio::net {

auto tcp_listener::bind(const detail::socket_addr& addr) -> result<tcp_listener> {
  const int fd = ::socket(addr.family(), SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (fd < 0) {
    return std::unexpected{error::last_os_error()};
  }

  detail::fd_guard guard{fd};

  constexpr int opt_val = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val));

  if (::bind(fd, addr.as_sockaddr(), addr.len()) < 0) {
    return std::unexpected{error::last_os_error()};
  }

  if (::listen(fd, SOMAXCONN) < 0) {
    return std::unexpected{error::last_os_error()};
  }

  return tcp_listener{std::move(guard)};
}

auto tcp_listener::accept() const -> result<std::pair<tcp_stream, detail::socket_addr>> {
  sockaddr_storage storage{};
  socklen_t len = sizeof(storage);

  const int fd = ::accept4(
    fd_.raw_fd(),
    reinterpret_cast<sockaddr*>(&storage),
    &len,
    SOCK_NONBLOCK | SOCK_CLOEXEC
  );

  if (fd < 0) {
    return std::unexpected{error::last_os_error()};
  }

  const auto peer = detail::socket_addr::from_raw(reinterpret_cast<sockaddr*>(&storage), len);
  return std::pair{tcp_stream{detail::fd_guard{fd}}, peer};
}

auto tcp_listener::local_addr() const -> result<detail::socket_addr> {
  detail::socket_addr addr;
  socklen_t len = addr.len();
  if (::getsockname(fd_.raw_fd(), addr.as_sockaddr_mut(), &len) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return addr;
}

auto tcp_listener::set_reuseaddr(const bool enable) const -> void_result {
  const int val = enable ? 1 : 0;
  if (::setsockopt(fd_.raw_fd(), SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return {};
}

auto tcp_listener::set_reuse_port(const bool enable) const -> void_result {
  const int val = enable ? 1 : 0;

  if (::setsockopt(fd_.raw_fd(), SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val)) < 0) {
    return std::unexpected{error::last_os_error()};
  }

  return {};
}

auto tcp_listener::set_ttl(const std::uint32_t ttl) const -> void_result {
  const int val = static_cast<int>(ttl);
  if (::setsockopt(fd_.raw_fd(), IPPROTO_IP, IP_TTL, &val, sizeof(val)) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return {};
}

auto tcp_listener::ttl() const -> result<std::uint32_t> {
  int val = 0;
  socklen_t len = sizeof(val);
  if (::getsockopt(fd_.raw_fd(), IPPROTO_IP, IP_TTL, &val, &len) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return static_cast<std::uint32_t>(val);
}

auto tcp_listener::take_error() const -> result<error> {
  int val = 0;
  socklen_t len = sizeof(val);
  if (::getsockopt(fd_.raw_fd(), SOL_SOCKET, SO_ERROR, &val, &len) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return error{val};
}

}
