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

#include <tio/sys/unix_/kqueue_waker.hpp>

namespace tio::sys::unix {

auto kqueue_waker::create(const kqueue_selector& sel, const token tok) -> result<kqueue_waker> {
  auto kq = sel.try_clone();
  if (!kq.has_value()) {
    return std::unexpected{kq.error()};
  }

  const auto r = kq->register_user_event(tok);
  if (!r.has_value()) {
    return std::unexpected{r.error()};
  }

  return kqueue_waker{std::move(kq.value()), tok};
}

}
