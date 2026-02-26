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

#include <tio/interest.hpp>
#include <tio/waker.hpp>

namespace tio {

auto waker::create(registry reg, token tok) -> result<waker> {
  auto ew = sys::unix::eventfd_waker::create();
  if (!ew.has_value()) {
    return std::unexpected{ew.error()};
  }

  const auto r = reg.register_fd(ew->raw_fd(), tok, interest::readable());
  if (!r.has_value()) {
    return std::unexpected{r.error()};
  }

  auto p = std::make_shared<inner>(std::move(ew.value()));
  return waker{std::move(p)};
}

}
