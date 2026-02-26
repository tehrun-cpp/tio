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
#include <format>

namespace tio {

class interest {
public:
  constexpr interest() noexcept : bits_{0} {}

  [[nodiscard]] static constexpr auto readable() noexcept -> interest {
    return interest{k_readable};
  }
  [[nodiscard]] static constexpr auto writable() noexcept -> interest {
    return interest{k_writable};
  }
  [[nodiscard]] static constexpr auto priority() noexcept -> interest {
    return interest{k_priority};
  }

  [[nodiscard]] constexpr auto is_readable() const noexcept -> bool {
    return (bits_ & k_readable) != 0;
  }

  [[nodiscard]] constexpr auto is_writable() const noexcept -> bool {
    return (bits_ & k_writable) != 0;
  }

  [[nodiscard]] constexpr auto is_priority() const noexcept -> bool {
    return (bits_ & k_priority) != 0;
  }

  [[nodiscard]] constexpr auto is_empty() const noexcept -> bool { return bits_ == 0; }

  [[nodiscard]] constexpr auto raw() const noexcept -> std::uint8_t { return bits_; }

  constexpr auto operator|(interest other) const noexcept -> interest {
    return interest{static_cast<std::uint8_t>(bits_ | other.bits_)};
  }

  constexpr auto operator|=(interest other) noexcept -> interest& {
    bits_ |= other.bits_;
    return *this;
  }

  [[nodiscard]] constexpr auto remove(interest other) const noexcept -> interest {
    const auto result = static_cast<std::uint8_t>(bits_ & ~other.bits_);
    return interest{result};
  }

  constexpr auto operator==(const interest&) const noexcept -> bool = default;

private:
  static constexpr std::uint8_t k_readable = 0b001;
  static constexpr std::uint8_t k_writable = 0b010;
  static constexpr std::uint8_t k_priority = 0b100;

  constexpr explicit interest(const std::uint8_t bits) noexcept : bits_{bits} {}

  std::uint8_t bits_;
};

}

template <> struct std::formatter<tio::interest> {
  static constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

  static auto format(const tio::interest& i, std::format_context& ctx) {
    auto out = ctx.out();
    out = std::format_to(out, "interest(");
    bool first = true;
    if (i.is_readable()) {
      out = std::format_to(out, "READABLE");
      first = false;
    }
    if (i.is_writable()) {
      if (!first)
        out = std::format_to(out, "|");
      out = std::format_to(out, "WRITABLE");
      first = false;
    }
    if (i.is_priority()) {
      if (!first)
        out = std::format_to(out, "|");
      out = std::format_to(out, "PRIORITY");
    }
    if (i.is_empty()) {
      out = std::format_to(out, "NONE");
    }
    return std::format_to(out, ")");
  }
};
