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

#include <cerrno>
#include <cstring>
#include <expected>
#include <format>
#include <string>
#include <string_view>

namespace tio {

class error {
public:
  constexpr error() noexcept : code_{0} {}
  constexpr explicit error(int code) noexcept : code_{code} {}

  [[nodiscard]] static auto last_os_error() noexcept -> error { return error{errno}; }

  [[nodiscard]] constexpr auto code() const noexcept -> int { return code_; }

  [[nodiscard]] auto message() const -> std::string { return std::strerror(code_); }

  [[nodiscard]] constexpr auto is_would_block() const noexcept -> bool {
    return code_ == EAGAIN || code_ == EWOULDBLOCK;
  }

  [[nodiscard]] constexpr auto is_interrupted() const noexcept -> bool { return code_ == EINTR; }

  [[nodiscard]] constexpr auto is_connection_refused() const noexcept -> bool {
    return code_ == ECONNREFUSED;
  }

  [[nodiscard]] constexpr auto is_connection_reset() const noexcept -> bool {
    return code_ == ECONNRESET;
  }

  [[nodiscard]] constexpr auto is_connection_aborted() const noexcept -> bool {
    return code_ == ECONNABORTED;
  }

  [[nodiscard]] constexpr auto is_not_connected() const noexcept -> bool {
    return code_ == ENOTCONN;
  }

  [[nodiscard]] constexpr auto is_addr_in_use() const noexcept -> bool {
    return code_ == EADDRINUSE;
  }

  [[nodiscard]] constexpr auto is_broken_pipe() const noexcept -> bool { return code_ == EPIPE; }

  [[nodiscard]] constexpr auto is_already_exists() const noexcept -> bool {
    return code_ == EEXIST;
  }

  [[nodiscard]] constexpr auto is_in_progress() const noexcept -> bool {
    return code_ == EINPROGRESS;
  }

  constexpr auto operator<=>(const error&) const noexcept = default;

private:
  int code_;
};

template <typename t_t> using result = std::expected<t_t, error>;

using void_result = std::expected<void, error>;

}

template <> struct std::formatter<tio::error> {
  static constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

  static auto format(const tio::error& e, std::format_context& ctx) {
    return std::format_to(ctx.out(), "error({}): {}", e.code(), e.message());
  }
};
