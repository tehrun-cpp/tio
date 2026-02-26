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
#include <sys/uio.h>

#include <tio/unix/unix_stream.hpp>

namespace tio::unix_ {

auto unix_stream::connect(const detail::unix_addr& addr) -> result<unix_stream> {
  const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
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

  return unix_stream{std::move(guard)};
}

auto unix_stream::pair() -> result<std::pair<unix_stream, unix_stream>> {
  int fds[2]{};
  if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, fds) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return std::pair{unix_stream{detail::fd_guard{fds[0]}}, unix_stream{detail::fd_guard{fds[1]}}};
}

auto unix_stream::read(std::span<std::byte> buf) const -> result<std::size_t> {
  const ssize_t n = ::recv(fd_.raw_fd(), buf.data(), buf.size(), 0);
  if (n < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return static_cast<std::size_t>(n);
}

auto unix_stream::write(std::span<const std::byte> buf) const -> result<std::size_t> {
  const ssize_t n = ::send(fd_.raw_fd(), buf.data(), buf.size(), MSG_NOSIGNAL);
  if (n < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return static_cast<std::size_t>(n);
}

auto unix_stream::peek(std::span<std::byte> buf) const -> result<std::size_t> {
  const ssize_t n = ::recv(fd_.raw_fd(), buf.data(), buf.size(), MSG_PEEK);
  if (n < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return static_cast<std::size_t>(n);
}

auto unix_stream::shutdown(int how) const -> void_result {
  if (::shutdown(fd_.raw_fd(), how) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return {};
}

auto unix_stream::peer_addr() const -> result<detail::unix_addr> {
  detail::unix_addr addr;
  socklen_t len = sizeof(sockaddr_un);
  if (::getpeername(fd_.raw_fd(), addr.as_sockaddr_mut(), &len) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  *addr.len_mut() = len;
  return addr;
}

auto unix_stream::local_addr() const -> result<detail::unix_addr> {
  detail::unix_addr addr;
  socklen_t len = sizeof(sockaddr_un);
  if (::getsockname(fd_.raw_fd(), addr.as_sockaddr_mut(), &len) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  *addr.len_mut() = len;
  return addr;
}

auto unix_stream::take_error() const -> result<error> {
  int val = 0;
  socklen_t len = sizeof(val);
  if (::getsockopt(fd_.raw_fd(), SOL_SOCKET, SO_ERROR, &val, &len) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return error{val};
}

auto unix_stream::read_vectored(std::span<iovec> bufs) const -> result<std::size_t> {
  const ssize_t n = ::readv(fd_.raw_fd(), bufs.data(), static_cast<int>(bufs.size()));
  if (n < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return static_cast<std::size_t>(n);
}

auto unix_stream::write_vectored(std::span<const iovec> bufs) const -> result<std::size_t> {
  const ssize_t n = ::writev(fd_.raw_fd(), bufs.data(), static_cast<int>(bufs.size()));
  if (n < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return static_cast<std::size_t>(n);
}

}
