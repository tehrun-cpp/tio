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
#include <span>
#include <utility>

#include <tio/error.hpp>
#include <tio/interest.hpp>
#include <tio/poll.hpp>
#include <tio/source.hpp>
#include <tio/sys/detail/fd_guard.hpp>
#include <tio/sys/detail/unix_addr.hpp>
#include <tio/token.hpp>

namespace tio::unix_ {

class unix_datagram {
public:
  [[nodiscard]] static auto bind(const detail::unix_addr& addr) -> result<unix_datagram>;

  [[nodiscard]] static auto unbound() -> result<unix_datagram>;

  [[nodiscard]] static auto pair() -> result<std::pair<unix_datagram, unix_datagram>>;

  [[nodiscard]] static auto from_raw_fd(int fd) noexcept -> unix_datagram {
    return unix_datagram{detail::fd_guard{fd}};
  }

  unix_datagram(unix_datagram&&) noexcept = default;
  auto operator=(unix_datagram&&) noexcept -> unix_datagram& = default;

  unix_datagram(const unix_datagram&) = delete;
  auto operator=(const unix_datagram&) -> unix_datagram& = delete;

  [[nodiscard]] auto connect(const detail::unix_addr& addr) const -> void_result;

  [[nodiscard]] auto send_to(std::span<const std::byte> buf, const detail::unix_addr& addr) const
      -> result<std::size_t>;

  [[nodiscard]] auto recv_from(std::span<std::byte> buf) const
      -> result<std::pair<std::size_t, detail::unix_addr>>;

  [[nodiscard]] auto send(std::span<const std::byte> buf) const -> result<std::size_t>;

  [[nodiscard]] auto recv(std::span<std::byte> buf) const -> result<std::size_t>;

  [[nodiscard]] auto peer_addr() const -> result<detail::unix_addr>;

  [[nodiscard]] auto local_addr() const -> result<detail::unix_addr>;

  [[nodiscard]] auto shutdown(int how) const -> void_result;

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
  explicit unix_datagram(detail::fd_guard fd) noexcept : fd_{std::move(fd)} {}

  detail::fd_guard fd_;
};

static_assert(source<unix_datagram>);

}
