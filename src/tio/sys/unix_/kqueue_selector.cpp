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

#include <cerrno>

#include <fcntl.h>
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>

#include <tio/sys/unix_/kqueue_selector.hpp>

namespace tio::sys::unix {

namespace {

constexpr int k_max_changes = 3;

constexpr std::uint16_t k_add_flags = EV_ADD | EV_ENABLE | EV_CLEAR | EV_RECEIPT;
constexpr std::uint16_t k_del_flags = EV_DELETE | EV_RECEIPT;
constexpr std::uint16_t k_placeholder_flags = EV_ADD | EV_DISABLE | EV_CLEAR | EV_RECEIPT;

}

kqueue_selector::kqueue_selector(detail::fd_guard kq_fd) noexcept : kq_fd_{std::move(kq_fd)} {}

auto kqueue_selector::create() -> result<kqueue_selector> {
  const int fd = ::kqueue();

  if (fd < 0) {
    return std::unexpected{error::last_os_error()};
  }

  detail::fd_guard guard{fd};

  if (::fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) {
    return std::unexpected{error::last_os_error()};
  }

  return kqueue_selector{std::move(guard)};
}

auto kqueue_selector::submit(
  raw_event* changes,
  const int count,
  const int ignored_errno
) const noexcept -> result<int> {
  int n = 0;
  const timespec no_wait{};
  do {
    n = ::kevent(kq_fd_.raw_fd(), changes, count, changes, count, &no_wait);
  } while (n < 0 && errno == EINTR);

  if (n < 0) {
    return std::unexpected{error::last_os_error()};
  }

  for (int i = 0; i < n; ++i) {
    if ((changes[i].flags & EV_ERROR) == 0) {
      continue;
    }
    const auto err = static_cast<int>(changes[i].data);
    if (err == 0 || err == ignored_errno) {
      continue;
    }
    return std::unexpected{error{err}};
  }

  return n;
}

auto kqueue_selector::select(
  raw_event* events,
  const int max_events,
  const std::optional<std::chrono::milliseconds> timeout
) const -> result<int> {
  timespec ts{};
  const timespec* ts_ptr = nullptr;

  if (timeout.has_value() && timeout->count() >= 0) {
    ts.tv_sec = static_cast<time_t>(timeout->count() / 1000);
    ts.tv_nsec = static_cast<long>((timeout->count() % 1000) * 1'000'000);
    ts_ptr = &ts;
  }

  int n = 0;
  do {
    n = ::kevent(kq_fd_.raw_fd(), nullptr, 0, events, max_events, ts_ptr);
  } while (n < 0 && errno == EINTR);

  if (n < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return n;
}

auto kqueue_selector::register_fd(
  const int fd,
  const token tok,
  const interest interest
) const -> void_result {
  raw_event changes[k_max_changes]{};
  int count = 0;
  const auto ident = static_cast<std::uintptr_t>(fd);
  void* const udata = to_udata(tok);

  if (interest.is_readable()) {
    EV_SET(&changes[count], ident, EVFILT_READ, k_add_flags, 0, 0, udata);
    ++count;
  }

  if (interest.is_writable()) {
    EV_SET(&changes[count], ident, EVFILT_WRITE, k_add_flags, 0, 0, udata);
    ++count;
  }

#if defined(EVFILT_EXCEPT)
  if (interest.is_priority()) {
    EV_SET(&changes[count], ident, EVFILT_EXCEPT, k_add_flags, NOTE_OOB, 0, udata);
    ++count;
  }
#endif

  if (count == 0) {
    EV_SET(&changes[count], ident, EVFILT_READ, k_placeholder_flags, 0, 0, udata);
    ++count;
  }

  const auto r = submit(changes, count, 0);
  if (!r.has_value()) {
    (void)deregister_fd(fd);
    return std::unexpected{r.error()};
  }

  return {};
}

auto kqueue_selector::reregister_fd(
  const int fd,
  const token tok,
  const interest interest
) const -> void_result {
  raw_event changes[k_max_changes]{};
  int count = 0;
  const auto ident = static_cast<std::uintptr_t>(fd);
  void* const udata = to_udata(tok);

  const std::uint16_t read_flags = interest.is_readable() ? k_add_flags
                                   : interest.is_empty()  ? k_placeholder_flags
                                                          : k_del_flags;

  EV_SET(&changes[count], ident, EVFILT_READ, read_flags, 0, 0, udata);
  ++count;

  EV_SET(
    &changes[count],
    ident,
    EVFILT_WRITE,
    interest.is_writable() ? k_add_flags : k_del_flags,
    0,
    0,
    udata
  );
  ++count;

#if defined(EVFILT_EXCEPT)
  EV_SET(
    &changes[count],
    ident,
    EVFILT_EXCEPT,
    interest.is_priority() ? k_add_flags : k_del_flags,
    interest.is_priority() ? NOTE_OOB : 0,
    0,
    udata
  );
  ++count;
#endif

  return submit(changes, count, ENOENT).transform([](int) {});
}

auto kqueue_selector::deregister_fd(const int fd) const -> void_result {
  raw_event changes[k_max_changes]{};
  int count = 0;
  const auto ident = static_cast<std::uintptr_t>(fd);

  EV_SET(&changes[count], ident, EVFILT_READ, k_del_flags, 0, 0, nullptr);
  ++count;
  EV_SET(&changes[count], ident, EVFILT_WRITE, k_del_flags, 0, 0, nullptr);
  ++count;
#if defined(EVFILT_EXCEPT)
  EV_SET(&changes[count], ident, EVFILT_EXCEPT, k_del_flags, 0, 0, nullptr);
  ++count;
#endif

  const auto n = submit(changes, count, ENOENT);
  if (!n.has_value()) {
    return std::unexpected{n.error()};
  }

  for (int i = 0; i < n.value(); ++i) {
    if ((changes[i].flags & EV_ERROR) == 0 || changes[i].data == 0) {
      return {};
    }
  }

  return std::unexpected{error{ENOENT}};
}

auto kqueue_selector::try_clone() const -> result<kqueue_selector> {
  const int new_fd = ::fcntl(kq_fd_.raw_fd(), F_DUPFD_CLOEXEC, 0);
  if (new_fd < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return kqueue_selector{detail::fd_guard{new_fd}};
}

auto kqueue_selector::register_user_event(const token tok) const -> void_result {
  raw_event change{};
  EV_SET(
    &change,
    static_cast<std::uintptr_t>(tok.value()),
    EVFILT_USER,
    k_add_flags,
    0,
    0,
    to_udata(tok)
  );
  return submit(&change, 1, 0).transform([](int) {});
}

auto kqueue_selector::trigger_user_event(const token tok) const noexcept -> void_result {
  raw_event change{};
  EV_SET(
    &change,
    static_cast<std::uintptr_t>(tok.value()),
    EVFILT_USER,
    k_add_flags,
    NOTE_TRIGGER,
    0,
    to_udata(tok)
  );
  return submit(&change, 1, 0).transform([](int) {});
}

auto kqueue_selector::deregister_user_event(const token tok) const noexcept -> void_result {
  raw_event change{};
  EV_SET(
    &change,
    static_cast<std::uintptr_t>(tok.value()),
    EVFILT_USER,
    k_del_flags,
    0,
    0,
    nullptr
  );
  return submit(&change, 1, ENOENT).transform([](int) {});
}

}
