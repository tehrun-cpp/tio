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
#include <tio/token.hpp>

namespace tio::unix_ {

class pipe_sender {
public:
  explicit pipe_sender(detail::fd_guard fd) noexcept : fd_{std::move(fd)} {}

  [[nodiscard]] static auto from_raw_fd(int fd) noexcept -> pipe_sender {
    return pipe_sender{detail::fd_guard{fd}};
  }

  pipe_sender(pipe_sender&&) noexcept = default;
  auto operator=(pipe_sender&&) noexcept -> pipe_sender& = default;

  pipe_sender(const pipe_sender&) = delete;
  auto operator=(const pipe_sender&) -> pipe_sender& = delete;

  [[nodiscard]] auto write(std::span<const std::byte> buf) const -> result<std::size_t>;

  [[nodiscard]] auto set_nonblocking(bool enable) const -> void_result;

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

class pipe_receiver {
public:
  explicit pipe_receiver(detail::fd_guard fd) noexcept : fd_{std::move(fd)} {}

  [[nodiscard]] static auto from_raw_fd(int fd) noexcept -> pipe_receiver {
    return pipe_receiver{detail::fd_guard{fd}};
  }

  pipe_receiver(pipe_receiver&&) noexcept = default;
  auto operator=(pipe_receiver&&) noexcept -> pipe_receiver& = default;

  pipe_receiver(const pipe_receiver&) = delete;
  auto operator=(const pipe_receiver&) -> pipe_receiver& = delete;

  [[nodiscard]] auto read(std::span<std::byte> buf) const -> result<std::size_t>;

  [[nodiscard]] auto set_nonblocking(bool enable) const -> void_result;

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

[[nodiscard]] auto make_pipe() -> result<std::pair<pipe_sender, pipe_receiver>>;

static_assert(source<pipe_sender>);
static_assert(source<pipe_receiver>);

}
