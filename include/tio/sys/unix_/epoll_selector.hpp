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

#include <chrono>
#include <optional>

#include <sys/epoll.h>

#include <tio/error.hpp>
#include <tio/interest.hpp>
#include <tio/sys/detail/fd_guard.hpp>
#include <tio/token.hpp>

namespace tio::sys::unix {

class epoll_selector {
public:
  using raw_event = ::epoll_event;

  [[nodiscard]] static auto create() -> result<epoll_selector>;

  epoll_selector(epoll_selector&&) noexcept = default;
  auto operator=(epoll_selector&&) noexcept -> epoll_selector& = default;
  epoll_selector(const epoll_selector&) = delete;
  auto operator=(const epoll_selector&) -> epoll_selector& = delete;

  [[nodiscard]] auto select(
    raw_event* events,
    int max_events,
    std::optional<std::chrono::milliseconds> timeout
  ) const -> result<int>;

  [[nodiscard]] auto register_fd(int fd, token tok, interest interest) const
      -> void_result;
  [[nodiscard]] auto reregister_fd(int fd, token tok, interest interest) const
      -> void_result;
  [[nodiscard]] auto deregister_fd(int fd) const -> void_result;
  [[nodiscard]] auto try_clone() const -> result<epoll_selector>;
  [[nodiscard]] auto raw_fd() const noexcept -> int { return epoll_fd_.raw_fd(); }

private:
  explicit epoll_selector(detail::fd_guard epoll_fd) noexcept;
  [[nodiscard]] static auto interest_to_epoll(interest interest) noexcept -> std::uint32_t;

  detail::fd_guard epoll_fd_;
};

struct epoll_event_traits {
  using raw_event = epoll_selector::raw_event;

  [[nodiscard]] static auto tok(const raw_event& ev) noexcept -> token {
    return token{static_cast<std::size_t>(ev.data.u64)};
  }

  [[nodiscard]] static auto is_readable(const raw_event& ev) noexcept -> bool {
    return (ev.events & EPOLLIN) != 0;
  }

  [[nodiscard]] static auto is_writable(const raw_event& ev) noexcept -> bool {
    return (ev.events & EPOLLOUT) != 0;
  }

  [[nodiscard]] static auto is_error(const raw_event& ev) noexcept -> bool {
    return (ev.events & EPOLLERR) != 0;
  }

  [[nodiscard]] static auto is_read_closed(const raw_event& ev) noexcept -> bool {
    return (ev.events & (EPOLLHUP | EPOLLRDHUP)) != 0;
  }

  [[nodiscard]] static auto is_write_closed(const raw_event& ev) noexcept -> bool {
    return (ev.events & (EPOLLHUP | EPOLLERR)) != 0;
  }

  [[nodiscard]] static auto is_priority(const raw_event& ev) noexcept -> bool {
    return (ev.events & EPOLLPRI) != 0;
  }
};

}
