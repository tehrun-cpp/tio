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

#include <tio/poll.hpp>

namespace tio {

auto registry::register_fd(int fd, token tok, interest interest) const -> void_result {
  return sel_->register_fd(fd, tok, interest);
}

auto registry::reregister_fd(int fd, token tok, interest intr) const -> void_result {
  return sel_->reregister_fd(fd, tok, intr);
}

auto registry::deregister_fd(int fd) const -> void_result {
  return sel_->deregister_fd(fd);
}

auto registry::try_clone() const -> result<registry> {
  auto cloned = sel_->try_clone();
  if (!cloned.has_value()) {
    return std::unexpected{cloned.error()};
  }
  return registry{sel_};
}

poll::poll(sys::selector sel) noexcept : sel_{std::move(sel)} {}

auto poll::create() -> result<poll> {
  auto sel = sys::selector::create();

  if (!sel.has_value()) {
    return std::unexpected{sel.error()};
  }

  return poll{std::move(sel.value())};
}

auto poll::do_poll(
  events& evs,
  const std::optional<std::chrono::milliseconds> timeout
) -> void_result {
  evs.clear();

  auto n = sel_.select(evs.raw_buf(), evs.raw_capacity(), timeout);
  if (!n.has_value()) {
    return std::unexpected{n.error()};
  }

  evs.set_len(static_cast<std::size_t>(n.value()));
  return {};
}

}
