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

#include <memory>

#include <tio/error.hpp>
#include <tio/poll.hpp>
#include <tio/sys/unix_/eventfd_waker.hpp>
#include <tio/token.hpp>

namespace tio {

class waker {
public:
  [[nodiscard]] static auto create(registry reg, token tok) -> result<waker>;

  [[nodiscard]] auto wake() const noexcept -> void_result { return inner_->waker_.wake(); }

  void drain() const noexcept { inner_->waker_.drain(); }

private:
  struct inner {
    sys::unix::eventfd_waker waker_;

    explicit inner(sys::unix::eventfd_waker w) noexcept : waker_{std::move(w)} {}
  };

  explicit waker(std::shared_ptr<inner> p) noexcept : inner_{std::move(p)} {}

  std::shared_ptr<inner> inner_;
};

}
