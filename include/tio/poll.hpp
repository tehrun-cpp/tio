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

#include <chrono>
#include <optional>

#include <tio/error.hpp>
#include <tio/event.hpp>
#include <tio/interest.hpp>
#include <tio/source.hpp>
#include <tio/sys/selector.hpp>
#include <tio/token.hpp>

namespace tio {

class registry {
public:
  [[nodiscard]] auto register_fd(int fd, token tok, interest interest) const -> void_result;
  [[nodiscard]] auto reregister_fd(int fd, token tok, interest interest) const -> void_result;
  [[nodiscard]] auto deregister_fd(int fd) const -> void_result;

  template <typename s_t> requires source<s_t>
  [[nodiscard]] auto register_source(s_t& s, token tok, interest interest) const -> void_result {
    return s.mio_register(*this, tok, interest);
  }

  template <typename s_t> requires source<s_t>
  [[nodiscard]] auto reregister_source(s_t& s, token tok, interest interest) const -> void_result {
    return s.mio_reregister(*this, tok, interest);
  }

  template <typename s_t> requires source<s_t>
  [[nodiscard]] auto deregister_source(s_t& s) const -> void_result {
    return s.mio_deregister(*this);
  }

  [[nodiscard]] auto try_clone() const -> result<registry>;

private:
  friend class poll;
  explicit registry(sys::selector* sel) noexcept : sel_{sel} {}

  sys::selector* sel_;
};

class poll {
public:
  [[nodiscard]] static auto create() -> result<poll>;

  poll(poll&&) noexcept = default;
  auto operator=(poll&&) noexcept -> poll& = default;

  poll(const poll&) = delete;
  auto operator=(const poll&) -> poll& = delete;

  [[nodiscard]] auto do_poll(
    events& evs,
    std::optional<std::chrono::milliseconds> timeout
  ) -> void_result;

  [[nodiscard]] auto get_registry() -> registry { return registry{&sel_}; }

private:
  explicit poll(sys::selector sel) noexcept;

  sys::selector sel_;
};

}
