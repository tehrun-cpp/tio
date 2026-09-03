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
#include <cstdint>
#include <optional>

#include <sys/event.h>
#include <sys/types.h>

#include <tio/error.hpp>
#include <tio/interest.hpp>
#include <tio/sys/detail/fd_guard.hpp>
#include <tio/token.hpp>

namespace tio::sys::unix {

class kqueue_selector {
public:
  using raw_event = struct ::kevent;

  [[nodiscard]] static auto create() -> result<kqueue_selector>;

  kqueue_selector(kqueue_selector&&) noexcept = default;
  auto operator=(kqueue_selector&&) noexcept -> kqueue_selector& = default;
  kqueue_selector(const kqueue_selector&) = delete;
  auto operator=(const kqueue_selector&) -> kqueue_selector& = delete;

  [[nodiscard]] auto select(
    raw_event* events,
    int max_events,
    std::optional<std::chrono::milliseconds> timeout
  ) const -> result<int>;

  [[nodiscard]] auto register_fd(int fd, token tok, interest interest) const -> void_result;
  [[nodiscard]] auto reregister_fd(int fd, token tok, interest interest) const -> void_result;
  [[nodiscard]] auto deregister_fd(int fd) const -> void_result;
  [[nodiscard]] auto try_clone() const -> result<kqueue_selector>;
  [[nodiscard]] auto raw_fd() const noexcept -> int { return kq_fd_.raw_fd(); }

  [[nodiscard]] auto register_user_event(token tok) const -> void_result;
  [[nodiscard]] auto trigger_user_event(token tok) const noexcept -> void_result;
  [[nodiscard]] auto deregister_user_event(token tok) const noexcept -> void_result;

private:
  explicit kqueue_selector(detail::fd_guard kq_fd) noexcept;

  [[nodiscard]] auto submit(raw_event* changes, int count, int ignored_errno) const noexcept
      -> result<int>;

  [[nodiscard]] static auto to_udata(token tok) noexcept -> void* {
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(tok.value()));
  }

  detail::fd_guard kq_fd_;
};

struct kqueue_event_traits {
  using raw_event = kqueue_selector::raw_event;

  [[nodiscard]] static auto tok(const raw_event& ev) noexcept -> token {
    return token{static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(ev.udata))};
  }

  [[nodiscard]] static auto is_readable(const raw_event& ev) noexcept -> bool {
    return ev.filter == EVFILT_READ || ev.filter == EVFILT_USER;
  }

  [[nodiscard]] static auto is_writable(const raw_event& ev) noexcept -> bool {
    return ev.filter == EVFILT_WRITE;
  }

  [[nodiscard]] static auto is_error(const raw_event& ev) noexcept -> bool {
    return (ev.flags & EV_ERROR) != 0 || ((ev.flags & EV_EOF) != 0 && ev.fflags != 0);
  }

  [[nodiscard]] static auto is_read_closed(const raw_event& ev) noexcept -> bool {
    if ((ev.flags & EV_EOF) == 0) {
      return false;
    }
#if defined(EVFILT_EXCEPT)
    return ev.filter == EVFILT_READ || ev.filter == EVFILT_EXCEPT;
#else
    return ev.filter == EVFILT_READ;
#endif
  }

  [[nodiscard]] static auto is_write_closed(const raw_event& ev) noexcept -> bool {
    return ev.filter == EVFILT_WRITE && (ev.flags & EV_EOF) != 0;
  }

  [[nodiscard]] static auto is_priority([[maybe_unused]] const raw_event& ev) noexcept -> bool {
#if defined(EVFILT_EXCEPT)
    return ev.filter == EVFILT_EXCEPT && (ev.fflags & NOTE_OOB) != 0;
#else
    return false;
#endif
  }
};

}
