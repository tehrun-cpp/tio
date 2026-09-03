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

#include <utility>

#include <tio/error.hpp>
#include <tio/sys/unix_/kqueue_selector.hpp>
#include <tio/token.hpp>

namespace tio::sys::unix {

class kqueue_waker {
public:
  [[nodiscard]] static auto create(const kqueue_selector& sel, token tok) -> result<kqueue_waker>;

  ~kqueue_waker() {
    if (kq_.raw_fd() >= 0) {
      (void)kq_.deregister_user_event(tok_);
    }
  }

  kqueue_waker(kqueue_waker&& other) noexcept
    : kq_{std::move(other.kq_)}, tok_{other.tok_} {}

  auto operator=(kqueue_waker&& other) noexcept -> kqueue_waker& {
    if (this != &other) {
      if (kq_.raw_fd() >= 0) {
        (void)kq_.deregister_user_event(tok_);
      }
      kq_ = std::move(other.kq_);
      tok_ = other.tok_;
    }
    return *this;
  }

  kqueue_waker(const kqueue_waker&) = delete;
  auto operator=(const kqueue_waker&) -> kqueue_waker& = delete;

  [[nodiscard]] auto wake() const noexcept -> void_result { return kq_.trigger_user_event(tok_); }

  void drain() const noexcept {}

private:
  kqueue_waker(kqueue_selector kq, const token tok) noexcept
    : kq_{std::move(kq)}, tok_{tok} {}

  kqueue_selector kq_;
  token tok_;
};

}
