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

#include <sys/uio.h>

#include <tio/error.hpp>
#include <tio/interest.hpp>
#include <tio/poll.hpp>
#include <tio/source.hpp>
#include <tio/sys/detail/fd_guard.hpp>
#include <tio/sys/detail/socket_addr.hpp>
#include <tio/token.hpp>

namespace tio::net {

class tcp_stream {
public:
  [[nodiscard]] static auto connect(const detail::socket_addr& addr) -> result<tcp_stream>;

  [[nodiscard]] static auto from_raw_fd(int fd) noexcept -> tcp_stream {
    return tcp_stream{detail::fd_guard{fd}};
  }

  explicit tcp_stream(detail::fd_guard fd) noexcept : fd_{std::move(fd)} {}

  tcp_stream(tcp_stream&&) noexcept = default;
  auto operator=(tcp_stream&&) noexcept -> tcp_stream& = default;

  tcp_stream(const tcp_stream&) = delete;
  auto operator=(const tcp_stream&) -> tcp_stream& = delete;

  [[nodiscard]] auto read(std::span<std::byte> buf) const -> result<std::size_t>;

  [[nodiscard]] auto write(std::span<const std::byte> buf) const -> result<std::size_t>;

  [[nodiscard]] auto peek(std::span<std::byte> buf) const -> result<std::size_t>;

  [[nodiscard]] auto shutdown(int how) const -> void_result;

  [[nodiscard]] auto set_nodelay(bool enable) const -> void_result;

  [[nodiscard]] auto nodelay() const -> result<bool>;

  [[nodiscard]] auto peer_addr() const -> result<detail::socket_addr>;

  [[nodiscard]] auto local_addr() const -> result<detail::socket_addr>;

  [[nodiscard]] auto set_ttl(std::uint32_t ttl) const -> void_result;

  [[nodiscard]] auto ttl() const -> result<std::uint32_t>;

  [[nodiscard]] auto take_error() const -> result<error>;

  [[nodiscard]] auto read_vectored(std::span<iovec> bufs) const -> result<std::size_t>;

  [[nodiscard]] auto write_vectored(std::span<const iovec> bufs) const -> result<std::size_t>;

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
  detail::fd_guard fd_;
};

static_assert(source<tcp_stream>);

}
