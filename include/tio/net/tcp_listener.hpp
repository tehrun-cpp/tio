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

#include <cstdint>
#include <utility>

#include <tio/error.hpp>
#include <tio/interest.hpp>
#include <tio/net/tcp_stream.hpp>
#include <tio/poll.hpp>
#include <tio/source.hpp>
#include <tio/sys/detail/fd_guard.hpp>
#include <tio/sys/detail/socket_addr.hpp>
#include <tio/token.hpp>

namespace tio::net {

class tcp_listener {
public:
  [[nodiscard]] static auto bind(const detail::socket_addr& addr) -> result<tcp_listener>;

  [[nodiscard]] static auto from_raw_fd(int fd) noexcept -> tcp_listener {
    return tcp_listener{detail::fd_guard{fd}};
  }

  tcp_listener(tcp_listener&&) noexcept = default;
  auto operator=(tcp_listener&&) noexcept -> tcp_listener& = default;

  tcp_listener(const tcp_listener&) = delete;
  auto operator=(const tcp_listener&) -> tcp_listener& = delete;

  [[nodiscard]] auto accept() const -> result<std::pair<tcp_stream, detail::socket_addr>>;

  [[nodiscard]] auto local_addr() const -> result<detail::socket_addr>;

  [[nodiscard]] auto set_reuseaddr(bool enable) const -> void_result;

  [[nodiscard]] auto set_reuse_port(bool enable) const -> void_result;

  [[nodiscard]] auto set_ttl(std::uint32_t ttl) const -> void_result;

  [[nodiscard]] auto ttl() const -> result<std::uint32_t>;

  [[nodiscard]] auto take_error() const -> result<error>;

  [[nodiscard]] auto raw_fd() const noexcept -> int { return fd_.raw_fd(); }

  auto into_raw_fd() noexcept -> int { return fd_.release(); }

  [[nodiscard]] auto mio_register(const registry& reg, token tok, interest intr) -> void_result {
    return reg.register_fd(fd_.raw_fd(), tok, intr);
  }

  [[nodiscard]] auto mio_reregister(const registry& reg, token tok, interest intr) -> void_result {
    return reg.reregister_fd(fd_.raw_fd(), tok, intr);
  }

  [[nodiscard]] auto mio_deregister(const registry& reg) -> void_result {
    return reg.deregister_fd(fd_.raw_fd());
  }

private:
  explicit tcp_listener(detail::fd_guard fd) noexcept : fd_{std::move(fd)} {}

  detail::fd_guard fd_;
};

static_assert(source<tcp_listener>);

}
