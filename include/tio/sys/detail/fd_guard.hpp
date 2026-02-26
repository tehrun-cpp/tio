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

#include <unistd.h>

namespace tio::detail {

class fd_guard {
public:
  fd_guard() noexcept : fd_{-1} {}
  explicit fd_guard(const int fd) noexcept : fd_{fd} {}

  ~fd_guard() {
    if (fd_ >= 0) {
      close(fd_);
    }
  }

  fd_guard(fd_guard&& other) noexcept : fd_{std::exchange(other.fd_, -1)} {}

  auto operator=(fd_guard&& other) noexcept -> fd_guard& {
    if (this != &other) {
      if (fd_ >= 0) {
        close(fd_);
      }
      fd_ = std::exchange(other.fd_, -1);
    }
    return *this;
  }

  fd_guard(const fd_guard&) = delete;
  auto operator=(const fd_guard&) -> fd_guard& = delete;
  [[nodiscard]] auto raw_fd() const noexcept -> int { return fd_; }
  [[nodiscard]] explicit operator bool() const noexcept { return fd_ >= 0; }
  auto release() noexcept -> int { return std::exchange(fd_, -1); }

  void reset(const int fd = -1) noexcept {
    if (fd_ >= 0) {
      close(fd_);
    }
    fd_ = fd;
  }

private:
  int fd_;
};

}
