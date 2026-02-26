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
#include <memory>

#include <tio/sys/selector.hpp>
#include <tio/token.hpp>

namespace tio {

class event {
public:
  explicit event(const sys::raw_event& raw) noexcept : raw_{&raw} {}

  [[nodiscard]] auto tok() const noexcept -> token { return token{raw_->data.u64}; }

  [[nodiscard]] auto is_readable() const noexcept -> bool { return (raw_->events & EPOLLIN) != 0; }

  [[nodiscard]] auto is_writable() const noexcept -> bool { return (raw_->events & EPOLLOUT) != 0; }

  [[nodiscard]] auto is_error() const noexcept -> bool { return (raw_->events & EPOLLERR) != 0; }

  [[nodiscard]] auto is_read_closed() const noexcept -> bool {
    return (raw_->events & (EPOLLHUP | EPOLLRDHUP)) != 0;
  }

  [[nodiscard]] auto is_write_closed() const noexcept -> bool {
    return (raw_->events & (EPOLLHUP | EPOLLERR)) != 0;
  }

  [[nodiscard]] auto is_priority() const noexcept -> bool { return (raw_->events & EPOLLPRI) != 0; }

  [[nodiscard]] auto raw() const noexcept -> const sys::raw_event& { return *raw_; }

private:
  const sys::raw_event* raw_;
};

class event_iterator {
public:
  using difference_type = std::ptrdiff_t;
  using value_type = event;

  event_iterator() noexcept : ptr_{nullptr} {}
  explicit event_iterator(const sys::raw_event* ptr) noexcept : ptr_{ptr} {}

  auto operator*() const noexcept -> event { return event{*ptr_}; }

  auto operator++() noexcept -> event_iterator& {
    ++ptr_;
    return *this;
  }

  auto operator++(int) noexcept -> event_iterator {
    auto tmp = *this;
    ++ptr_;
    return tmp;
  }

  auto operator==(const event_iterator& other) const noexcept -> bool { return ptr_ == other.ptr_; }

private:
  const sys::raw_event* ptr_;
};

class events {
public:
  explicit events(std::size_t capacity)
    : buf_{std::make_unique<sys::raw_event[]>(capacity)}, capacity_{capacity}, len_{0} {}

  [[nodiscard]] auto size() const noexcept -> std::size_t { return len_; }

  [[nodiscard]] auto capacity() const noexcept -> std::size_t { return capacity_; }

  [[nodiscard]] auto is_empty() const noexcept -> bool { return len_ == 0; }

  [[nodiscard]] auto begin() const noexcept -> event_iterator { return event_iterator{buf_.get()}; }

  [[nodiscard]] auto end() const noexcept -> event_iterator {
    return event_iterator{buf_.get() + len_};
  }

  [[nodiscard]] auto operator[](std::size_t i) const noexcept -> event { return event{buf_[i]}; }

  [[nodiscard]] auto raw_buf() noexcept -> sys::raw_event* { return buf_.get(); }

  [[nodiscard]] auto raw_capacity() const noexcept -> int { return static_cast<int>(capacity_); }

  void set_len(std::size_t len) noexcept { len_ = len; }

  void clear() noexcept { len_ = 0; }

private:
  std::unique_ptr<sys::raw_event[]> buf_;
  std::size_t capacity_;
  std::size_t len_;
};

}
