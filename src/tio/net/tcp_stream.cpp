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
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <tio/net/tcp_stream.hpp>

#include <unistd.h>

namespace tio::net {

auto tcp_stream::connect(const detail::socket_addr& addr) -> result<tcp_stream> {
  const int fd = ::socket(addr.family(), SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (fd < 0) {
    return std::unexpected{error::last_os_error()};
  }

  detail::fd_guard guard{fd};

  const int rc = ::connect(fd, addr.as_sockaddr(), addr.len());
  if (rc < 0) {
    const auto e = error::last_os_error();
    if (!e.is_in_progress()) {
      return std::unexpected{e};
    }
  }

  return tcp_stream{std::move(guard)};
}

auto tcp_stream::read(std::span<std::byte> buf) const -> result<std::size_t> {
  const ssize_t n = ::recv(fd_.raw_fd(), buf.data(), buf.size(), 0);
  if (n < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return static_cast<std::size_t>(n);
}

auto tcp_stream::write(const std::span<const std::byte> buf) const -> result<std::size_t> {
  const ssize_t n = send(fd_.raw_fd(), buf.data(), buf.size(), MSG_NOSIGNAL);
  if (n < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return static_cast<std::size_t>(n);
}

auto tcp_stream::peek(std::span<std::byte> buf) const -> result<std::size_t> {
  const ssize_t n = ::recv(fd_.raw_fd(), buf.data(), buf.size(), MSG_PEEK);
  if (n < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return static_cast<std::size_t>(n);
}

auto tcp_stream::shutdown(int how) const -> void_result {
  if (::shutdown(fd_.raw_fd(), how) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return {};
}

auto tcp_stream::set_nodelay(bool enable) const -> void_result {
  const int val = enable ? 1 : 0;
  if (::setsockopt(fd_.raw_fd(), IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val)) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return {};
}

auto tcp_stream::nodelay() const -> result<bool> {
  int val = 0;
  socklen_t len = sizeof(val);
  if (::getsockopt(fd_.raw_fd(), IPPROTO_TCP, TCP_NODELAY, &val, &len) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return val != 0;
}

auto tcp_stream::peer_addr() const -> result<detail::socket_addr> {
  detail::socket_addr addr;
  socklen_t len = addr.len();
  if (::getpeername(fd_.raw_fd(), addr.as_sockaddr_mut(), &len) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return addr;
}

auto tcp_stream::local_addr() const -> result<detail::socket_addr> {
  detail::socket_addr addr;
  socklen_t len = addr.len();
  if (::getsockname(fd_.raw_fd(), addr.as_sockaddr_mut(), &len) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return addr;
}

auto tcp_stream::set_ttl(std::uint32_t ttl) const -> void_result {
  const int val = static_cast<int>(ttl);
  if (::setsockopt(fd_.raw_fd(), IPPROTO_IP, IP_TTL, &val, sizeof(val)) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return {};
}

auto tcp_stream::ttl() const -> result<std::uint32_t> {
  int val = 0;
  socklen_t len = sizeof(val);
  if (::getsockopt(fd_.raw_fd(), IPPROTO_IP, IP_TTL, &val, &len) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return static_cast<std::uint32_t>(val);
}

auto tcp_stream::take_error() const -> result<error> {
  int val = 0;
  socklen_t len = sizeof(val);
  if (::getsockopt(fd_.raw_fd(), SOL_SOCKET, SO_ERROR, &val, &len) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return error{val};
}

auto tcp_stream::read_vectored(std::span<iovec> bufs) const -> result<std::size_t> {
  const ssize_t n = ::readv(fd_.raw_fd(), bufs.data(), static_cast<int>(bufs.size()));
  if (n < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return static_cast<std::size_t>(n);
}

auto tcp_stream::write_vectored(std::span<const iovec> bufs) const -> result<std::size_t> {
  const ssize_t n = ::writev(fd_.raw_fd(), bufs.data(), static_cast<int>(bufs.size()));
  if (n < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return static_cast<std::size_t>(n);
}

}
