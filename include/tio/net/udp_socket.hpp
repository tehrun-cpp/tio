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

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

#include <netinet/in.h>

#include <tio/error.hpp>
#include <tio/interest.hpp>
#include <tio/poll.hpp>
#include <tio/source.hpp>
#include <tio/sys/detail/fd_guard.hpp>
#include <tio/sys/detail/socket_addr.hpp>
#include <tio/token.hpp>

namespace tio::net {

class udp_socket {
public:
  [[nodiscard]] static auto bind(const detail::socket_addr& addr) -> result<udp_socket>;

  [[nodiscard]] static auto from_raw_fd(int fd) noexcept -> udp_socket {
    return udp_socket{detail::fd_guard{fd}};
  }

  udp_socket(udp_socket&&) noexcept = default;
  auto operator=(udp_socket&&) noexcept -> udp_socket& = default;

  udp_socket(const udp_socket&) = delete;
  auto operator=(const udp_socket&) -> udp_socket& = delete;

  [[nodiscard]] auto send_to(std::span<const std::byte> buf, const detail::socket_addr& addr) const
      -> result<std::size_t>;

  [[nodiscard]] auto recv_from(std::span<std::byte> buf) const
      -> result<std::pair<std::size_t, detail::socket_addr>>;

  [[nodiscard]] auto connect(const detail::socket_addr& addr) const -> void_result;

  [[nodiscard]] auto send(std::span<const std::byte> buf) const -> result<std::size_t>;

  [[nodiscard]] auto recv(std::span<std::byte> buf) const -> result<std::size_t>;

  [[nodiscard]] auto peek(std::span<std::byte> buf) const -> result<std::size_t>;

  [[nodiscard]] auto peek_from(std::span<std::byte> buf) const
      -> result<std::pair<std::size_t, detail::socket_addr>>;

  [[nodiscard]] auto local_addr() const -> result<detail::socket_addr>;

  [[nodiscard]] auto set_broadcast(bool enable) const -> void_result;

  [[nodiscard]] auto broadcast() const -> result<bool>;

  [[nodiscard]] auto peer_addr() const -> result<detail::socket_addr>;

  [[nodiscard]] auto set_ttl(std::uint32_t ttl) const -> void_result;

  [[nodiscard]] auto ttl() const -> result<std::uint32_t>;

  [[nodiscard]] auto only_v6() const -> result<bool>;

  [[nodiscard]] auto take_error() const -> result<error>;

  [[nodiscard]] auto join_multicast_v4(in_addr group, in_addr iface) const -> void_result;

  [[nodiscard]] auto leave_multicast_v4(in_addr group, in_addr iface) const -> void_result;

  [[nodiscard]] auto set_multicast_ttl_v4(std::uint32_t ttl) const -> void_result;

  [[nodiscard]] auto multicast_ttl_v4() const -> result<std::uint32_t>;

  [[nodiscard]] auto set_multicast_loop_v4(bool enable) const -> void_result;

  [[nodiscard]] auto multicast_loop_v4() const -> result<bool>;

  [[nodiscard]] auto join_multicast_v6(in6_addr group, std::uint32_t iface) const -> void_result;

  [[nodiscard]] auto leave_multicast_v6(in6_addr group, std::uint32_t iface) const -> void_result;

  [[nodiscard]] auto set_multicast_loop_v6(bool enable) const -> void_result;

  [[nodiscard]] auto multicast_loop_v6() const -> result<bool>;

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
  explicit udp_socket(detail::fd_guard fd) noexcept : fd_{std::move(fd)} {}

  detail::fd_guard fd_;
};

static_assert(source<udp_socket>);

}
