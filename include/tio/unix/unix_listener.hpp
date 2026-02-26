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
#include <tio/interest.hpp>
#include <tio/poll.hpp>
#include <tio/source.hpp>
#include <tio/sys/detail/fd_guard.hpp>
#include <tio/sys/detail/unix_addr.hpp>
#include <tio/token.hpp>

namespace tio::unix_ {

class unix_stream;

class unix_listener {
public:
  [[nodiscard]] static auto bind(const detail::unix_addr& addr) -> result<unix_listener>;

  [[nodiscard]] static auto from_raw_fd(int fd) noexcept -> unix_listener {
    return unix_listener{detail::fd_guard{fd}};
  }

  unix_listener(unix_listener&&) noexcept = default;
  auto operator=(unix_listener&&) noexcept -> unix_listener& = default;

  unix_listener(const unix_listener&) = delete;
  auto operator=(const unix_listener&) -> unix_listener& = delete;

  [[nodiscard]] auto accept() const -> result<std::pair<unix_stream, detail::unix_addr>>;

  [[nodiscard]] auto local_addr() const -> result<detail::unix_addr>;

  [[nodiscard]] auto take_error() const -> result<error>;

  [[nodiscard]] auto raw_fd() const noexcept -> int { return fd_.raw_fd(); }

  auto into_raw_fd() noexcept -> int { return fd_.release(); }

  [[nodiscard]] auto tio_register(const registry& reg, token tok, interest intr) -> void_result {
    return reg.register_fd(fd_.raw_fd(), tok, intr);
  }

  [[nodiscard]] auto tio_reregister(const registry& reg, token tok, interest intr) -> void_result {
    return reg.reregister_fd(fd_.raw_fd(), tok, intr);
  }

  [[nodiscard]] auto tio_deregister(const registry& reg) -> void_result {
    return reg.deregister_fd(fd_.raw_fd());
  }

private:
  explicit unix_listener(detail::fd_guard fd) noexcept : fd_{std::move(fd)} {}

  detail::fd_guard fd_;
};

static_assert(source<unix_listener>);

}
