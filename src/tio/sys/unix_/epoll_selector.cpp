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

#include <sys/epoll.h>
#include <unistd.h>
#include <tio/sys/unix_/epoll_selector.hpp>

namespace tio::sys::unix {

epoll_selector::epoll_selector(
  detail::fd_guard epoll_fd
) noexcept : epoll_fd_{std::move(epoll_fd)} {}

auto epoll_selector::create() -> result<epoll_selector> {
  const int fd = ::epoll_create1(EPOLL_CLOEXEC);

  if (fd < 0) {
    return std::unexpected{error::last_os_error()};
  }

  return epoll_selector{detail::fd_guard{fd}};
}

auto epoll_selector::select(
  raw_event* events,
  const int max_events,
  const std::optional<std::chrono::milliseconds> timeout
) const -> result<int> {
  int timeout_ms = -1;
  if (timeout.has_value()) {
    timeout_ms = static_cast<int>(timeout->count());
  }

  int n = 0;
  do {
    n = epoll_wait(epoll_fd_.raw_fd(), events, max_events, timeout_ms);
  } while (n < 0 && errno == EINTR);

  if (n < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return n;
}

auto epoll_selector::register_fd(
  const int fd,
  const token tok,
  const interest interest
) const -> void_result {
  epoll_event ev{};
  ev.events = interest_to_epoll(interest);
  ev.data.u64 = tok.value();

  if (epoll_ctl(epoll_fd_.raw_fd(), EPOLL_CTL_ADD, fd, &ev) < 0) {
    return std::unexpected{error::last_os_error()};
  }

  return {};
}

auto epoll_selector::reregister_fd(
  const int fd,
  const token tok,
  const interest interest
) const -> void_result {
  epoll_event ev{};
  ev.events = interest_to_epoll(interest);
  ev.data.u64 = tok.value();

  if (epoll_ctl(epoll_fd_.raw_fd(), EPOLL_CTL_MOD, fd, &ev) < 0) {
    return std::unexpected{error::last_os_error()};
  }

  return {};
}

auto epoll_selector::deregister_fd(const int fd) const -> void_result {

  if (epoll_ctl(epoll_fd_.raw_fd(), EPOLL_CTL_DEL, fd, nullptr) < 0) {
    return std::unexpected{error::last_os_error()};
  }

  return {};
}

auto epoll_selector::try_clone() const -> result<epoll_selector> {
  const int new_fd = dup(epoll_fd_.raw_fd());
  if (new_fd < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return epoll_selector{detail::fd_guard{new_fd}};
}

auto epoll_selector::interest_to_epoll(interest interest) noexcept -> std::uint32_t {
  std::uint32_t flags = EPOLLET;

  if (interest.is_readable()) {
    flags |= EPOLLIN | EPOLLRDHUP;
  }

  if (interest.is_writable()) {
    flags |= EPOLLOUT;
  }

  if (interest.is_priority()) {
    flags |= EPOLLPRI;
  }

  return flags;
}

}  // namespace tio::sys::unix
