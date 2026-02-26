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

#include <tio/error.hpp>
#include <tio/sys/detail/fd_guard.hpp>

namespace tio::sys::unix {

class eventfd_waker {
public:
  [[nodiscard]] static auto create() -> result<eventfd_waker>;

  eventfd_waker(eventfd_waker&&) noexcept = default;
  auto operator=(eventfd_waker&&) noexcept -> eventfd_waker& = default;

  eventfd_waker(const eventfd_waker&) = delete;
  auto operator=(const eventfd_waker&) -> eventfd_waker& = delete;

  [[nodiscard]] auto wake() const noexcept -> void_result;

  void drain() const noexcept;

  [[nodiscard]] auto raw_fd() const noexcept -> int { return fd_.raw_fd(); }

private:
  explicit eventfd_waker(detail::fd_guard fd) noexcept;

  detail::fd_guard fd_;
};

}
