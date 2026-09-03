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

#include <tio/waker.hpp>

namespace tio {

auto waker::create(registry reg, token tok) -> result<waker> {
  auto w = sys::waker::create(*reg.sel_, tok);
  if (!w.has_value()) {
    return std::unexpected{w.error()};
  }

  auto p = std::make_shared<inner>(std::move(w.value()));
  return waker{std::move(p)};
}

}
