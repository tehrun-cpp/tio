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

#include <compare>
#include <cstddef>
#include <format>
#include <functional>

namespace tio {

class token {
public:
  constexpr token() noexcept : value_{0} {}
  constexpr explicit token(std::size_t value) noexcept : value_{value} {}

  [[nodiscard]] constexpr auto value() const noexcept -> std::size_t { return value_; }

  constexpr auto operator<=>(const token&) const noexcept = default;

private:
  std::size_t value_;
};

}

template <> struct std::hash<tio::token> {
  static constexpr auto operator()(const tio::token& t) noexcept -> std::size_t {
    return std::hash<std::size_t>{}(t.value());
  }
};

template <> struct std::formatter<tio::token> {
  static constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

  static auto format(const tio::token& t, std::format_context& ctx) {
    return std::format_to(ctx.out(), "token({})", t.value());
  }
};
