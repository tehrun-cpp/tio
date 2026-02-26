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
#include <tio/interest.hpp>
#include <tio/poll.hpp>
#include <tio/source.hpp>
#include <tio/token.hpp>

namespace tio {

class raw_fd {
public:
  explicit raw_fd(int fd) noexcept : fd_{fd} {}

  [[nodiscard]] auto fd() const noexcept -> int { return fd_; }

  [[nodiscard]] auto tio_register(const registry& reg, token tok, interest intr) -> void_result {
    return reg.register_fd(fd_, tok, intr);
  }

  [[nodiscard]] auto tio_reregister(const registry& reg, token tok, interest intr) -> void_result {
    return reg.reregister_fd(fd_, tok, intr);
  }

  [[nodiscard]] auto tio_deregister(const registry& reg) -> void_result {
    return reg.deregister_fd(fd_);
  }

private:
  int fd_;
};

static_assert(source<raw_fd>);

}
