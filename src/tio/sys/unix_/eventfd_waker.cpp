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

#include <cstdint>

#include <sys/eventfd.h>

#include <tio/sys/unix_/eventfd_waker.hpp>

#include <unistd.h>

namespace tio::sys::unix {

eventfd_waker::eventfd_waker(detail::fd_guard fd) noexcept : fd_{std::move(fd)} {}

auto eventfd_waker::create() -> result<eventfd_waker> {
  int const fd = ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  if (fd < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return eventfd_waker{detail::fd_guard{fd}};
}

auto eventfd_waker::wake() const noexcept -> void_result {
  constexpr std::uint64_t val = 1;
  if (ssize_t const n = ::write(fd_.raw_fd(), &val, sizeof(val)); n < 0) {
    auto e = error::last_os_error();
    if (e.is_would_block()) {
      return {};
    }
    return std::unexpected{e};
  }
  return {};
}

void eventfd_waker::drain() const noexcept {
  std::uint64_t val;
  [[maybe_unused]] const ssize_t n = ::read(fd_.raw_fd(), &val, sizeof(val));
}

}
