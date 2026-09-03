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

#include <cerrno>

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace tio::detail {

#if defined(MSG_NOSIGNAL)
inline constexpr int k_send_flags = MSG_NOSIGNAL;
#else
inline constexpr int k_send_flags = 0;
#endif

inline void close_preserving_errno(const int fd) noexcept {
  const int saved = errno;
  ::close(fd);
  errno = saved;
}

inline void close_pair_preserving_errno(int fds[2]) noexcept {
  const int saved = errno;
  ::close(fds[0]);
  ::close(fds[1]);
  errno = saved;
}

[[nodiscard]] inline auto set_nonblock_cloexec(const int fd) noexcept -> bool {
  const int fl = ::fcntl(fd, F_GETFL);
  if (fl < 0 || ::fcntl(fd, F_SETFL, fl | O_NONBLOCK) < 0) {
    return false;
  }

  const int fd_fl = ::fcntl(fd, F_GETFD);
  if (fd_fl < 0 || ::fcntl(fd, F_SETFD, fd_fl | FD_CLOEXEC) < 0) {
    return false;
  }

  return true;
}

inline void suppress_sigpipe([[maybe_unused]] const int fd) noexcept {
#if defined(SO_NOSIGPIPE)
  constexpr int on = 1;
  ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#endif
}

inline void suppress_sigpipe_fd([[maybe_unused]] const int fd) noexcept {
#if defined(F_SETNOSIGPIPE)
  ::fcntl(fd, F_SETNOSIGPIPE, 1);
#endif
}

[[nodiscard]] inline auto make_socket(const int domain, const int type, const int protocol) noexcept
    -> int {
#if defined(SOCK_NONBLOCK) && defined(SOCK_CLOEXEC)
  const int fd = ::socket(domain, type | SOCK_NONBLOCK | SOCK_CLOEXEC, protocol);
#else
  const int fd = ::socket(domain, type, protocol);
#endif
  if (fd < 0) {
    return -1;
  }
#if !defined(SOCK_NONBLOCK) || !defined(SOCK_CLOEXEC)
  if (!set_nonblock_cloexec(fd)) {
    close_preserving_errno(fd);
    return -1;
  }
#endif
  suppress_sigpipe(fd);
  return fd;
}

[[nodiscard]] inline auto make_socket_pair(
  const int domain,
  const int type,
  const int protocol,
  int fds[2]
) noexcept -> int {
#if defined(SOCK_NONBLOCK) && defined(SOCK_CLOEXEC)
  if (::socketpair(domain, type | SOCK_NONBLOCK | SOCK_CLOEXEC, protocol, fds) < 0) {
    return -1;
  }
#else
  if (::socketpair(domain, type, protocol, fds) < 0) {
    return -1;
  }
  for (int i = 0; i < 2; ++i) {
    if (!set_nonblock_cloexec(fds[i])) {
      close_pair_preserving_errno(fds);
      return -1;
    }
  }
#endif
  suppress_sigpipe(fds[0]);
  suppress_sigpipe(fds[1]);
  return 0;
}

[[nodiscard]] inline auto accept_socket(const int fd, sockaddr* addr, socklen_t* len) noexcept
    -> int {
#if defined(__linux__)
  const int new_fd = ::accept4(fd, addr, len, SOCK_NONBLOCK | SOCK_CLOEXEC);
#else
  const int new_fd = ::accept(fd, addr, len);
#endif
  if (new_fd < 0) {
    return -1;
  }
#if !defined(__linux__)
  if (!set_nonblock_cloexec(new_fd)) {
    close_preserving_errno(new_fd);
    return -1;
  }
#endif
  suppress_sigpipe(new_fd);
  return new_fd;
}

[[nodiscard]] inline auto make_pipe_fds(int fds[2]) noexcept -> int {
#if defined(__linux__)
  if (::pipe2(fds, O_NONBLOCK | O_CLOEXEC) < 0) {
    return -1;
  }
#else
  if (::pipe(fds) < 0) {
    return -1;
  }
  for (int i = 0; i < 2; ++i) {
    if (!set_nonblock_cloexec(fds[i])) {
      close_pair_preserving_errno(fds);
      return -1;
    }
  }
#endif
  suppress_sigpipe_fd(fds[0]);
  suppress_sigpipe_fd(fds[1]);
  return 0;
}

}
