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

#include <algorithm>
#include <vector>

#include <tio/event.hpp>

#include <gtest/gtest.h>

using tio::event;
using tio::events;
using tio::token;
using tio::sys::raw_event;

namespace {

#if defined(TIO_BACKEND_EPOLL)

auto raw_with(std::uint64_t tok, std::uint32_t flags) -> raw_event {
  raw_event ev{};
  ev.data.u64 = tok;
  ev.events = flags;
  return ev;
}

auto raw_token_only(std::uint64_t tok) -> raw_event { return raw_with(tok, 0); }
auto raw_readable(std::uint64_t tok) -> raw_event { return raw_with(tok, EPOLLIN); }
auto raw_writable(std::uint64_t tok) -> raw_event { return raw_with(tok, EPOLLOUT); }
auto raw_error(std::uint64_t tok) -> raw_event { return raw_with(tok, EPOLLERR); }
auto raw_read_closed(std::uint64_t tok) -> raw_event { return raw_with(tok, EPOLLRDHUP); }
auto raw_write_closed(std::uint64_t tok) -> raw_event { return raw_with(tok, EPOLLHUP); }
auto raw_priority(std::uint64_t tok) -> raw_event { return raw_with(tok, EPOLLPRI); }

#elif defined(TIO_BACKEND_KQUEUE)

auto raw_with(std::uint64_t tok, std::int16_t filter, std::uint16_t flags, std::uint32_t fflags)
    -> raw_event {
  raw_event ev{};
  EV_SET(
    &ev,
    0,
    filter,
    flags,
    fflags,
    0,
    reinterpret_cast<void*>(static_cast<std::uintptr_t>(tok))
  );
  return ev;
}

auto raw_token_only(std::uint64_t tok) -> raw_event { return raw_with(tok, EVFILT_READ, 0, 0); }
auto raw_readable(std::uint64_t tok) -> raw_event { return raw_with(tok, EVFILT_READ, 0, 0); }
auto raw_writable(std::uint64_t tok) -> raw_event { return raw_with(tok, EVFILT_WRITE, 0, 0); }
auto raw_error(std::uint64_t tok) -> raw_event { return raw_with(tok, EVFILT_READ, EV_ERROR, 0); }

auto raw_read_closed(std::uint64_t tok) -> raw_event {
  return raw_with(tok, EVFILT_READ, EV_EOF, 0);
}

auto raw_write_closed(std::uint64_t tok) -> raw_event {
  return raw_with(tok, EVFILT_WRITE, EV_EOF, 0);
}

auto raw_priority(std::uint64_t tok) -> raw_event {
  return raw_with(tok, EVFILT_EXCEPT, 0, NOTE_OOB);
}

#endif

}

TEST(event_test, token) {
  auto raw = raw_token_only(42);
  event ev{raw};
  EXPECT_EQ(ev.tok(), token{42});
}

TEST(event_test, is_readable) {
  auto raw = raw_readable(1);
  event ev{raw};
  EXPECT_TRUE(ev.is_readable());
  EXPECT_FALSE(ev.is_writable());
}

TEST(event_test, is_writable) {
  auto raw = raw_writable(1);
  event ev{raw};
  EXPECT_TRUE(ev.is_writable());
  EXPECT_FALSE(ev.is_readable());
}

TEST(event_test, is_error) {
  auto raw = raw_error(1);
  event ev{raw};
  EXPECT_TRUE(ev.is_error());
}

TEST(event_test, is_read_closed) {
  auto raw = raw_read_closed(1);
  event ev{raw};
  EXPECT_TRUE(ev.is_read_closed());
}

TEST(event_test, is_write_closed) {
  auto raw = raw_write_closed(1);
  event ev{raw};
  EXPECT_TRUE(ev.is_write_closed());
}

TEST(event_test, is_priority) {
  auto raw = raw_priority(1);
  event ev{raw};
  EXPECT_TRUE(ev.is_priority());
  EXPECT_FALSE(ev.is_readable());
}

#if defined(TIO_BACKEND_EPOLL)

TEST(event_test, is_read_closed_hup) {
  auto raw = raw_with(1, EPOLLHUP);
  event ev{raw};
  EXPECT_TRUE(ev.is_read_closed());
}

TEST(event_test, is_write_closed_err) {
  auto raw = raw_with(1, EPOLLERR);
  event ev{raw};
  EXPECT_TRUE(ev.is_write_closed());
}

TEST(event_test, combined_flags) {
  auto raw = raw_with(1, EPOLLIN | EPOLLOUT);
  event ev{raw};
  EXPECT_TRUE(ev.is_readable());
  EXPECT_TRUE(ev.is_writable());
  EXPECT_FALSE(ev.is_error());
}

TEST(event_test, raw_access) {
  auto raw = raw_with(7, EPOLLIN);
  event ev{raw};
  EXPECT_EQ(ev.raw().data.u64, 7u);
  EXPECT_EQ(ev.raw().events, EPOLLIN);
}

#elif defined(TIO_BACKEND_KQUEUE)

TEST(event_test, raw_access) {
  auto raw = raw_readable(7);
  event ev{raw};
  EXPECT_EQ(reinterpret_cast<std::uintptr_t>(ev.raw().udata), 7u);
  EXPECT_EQ(ev.raw().filter, EVFILT_READ);
}

#endif

TEST(events_test, initial_state) {
  events evs{128};
  EXPECT_EQ(evs.capacity(), 128u);
  EXPECT_EQ(evs.size(), 0u);
  EXPECT_TRUE(evs.is_empty());
}

TEST(events_test, set_len_and_access) {
  events evs{8};

  evs.raw_buf()[0] = raw_readable(10);
  evs.raw_buf()[1] = raw_writable(20);
  evs.set_len(2);

  EXPECT_EQ(evs.size(), 2u);
  EXPECT_FALSE(evs.is_empty());
  EXPECT_EQ(evs[0].tok(), token{10});
  EXPECT_TRUE(evs[0].is_readable());
  EXPECT_EQ(evs[1].tok(), token{20});
  EXPECT_TRUE(evs[1].is_writable());
}

TEST(events_test, clear) {
  events evs{8};
  evs.raw_buf()[0] = raw_readable(1);
  evs.set_len(1);

  evs.clear();
  EXPECT_EQ(evs.size(), 0u);
  EXPECT_TRUE(evs.is_empty());
}

TEST(events_test, range_for_iteration) {
  events evs{8};
  evs.raw_buf()[0] = raw_readable(1);
  evs.raw_buf()[1] = raw_writable(2);
  evs.raw_buf()[2] = raw_readable(3);
  evs.set_len(3);

  std::vector<std::size_t> tokens;
  for (const auto& ev : evs) {
    tokens.push_back(ev.tok().value());
  }

  EXPECT_EQ(tokens.size(), 3u);
  EXPECT_EQ(tokens[0], 1u);
  EXPECT_EQ(tokens[1], 2u);
  EXPECT_EQ(tokens[2], 3u);
}

TEST(events_test, raw_capacity) {
  events evs{256};
  EXPECT_EQ(evs.raw_capacity(), 256);
}

TEST(events_test, begin_equals_end_when_empty) {
  events evs{8};
  EXPECT_EQ(evs.begin(), evs.end());
}

TEST(events_test, std_algorithm_compatible) {
  events evs{8};
  evs.raw_buf()[0] = raw_readable(5);
  evs.raw_buf()[1] = raw_writable(10);
  evs.raw_buf()[2] = raw_readable(15);
  evs.set_len(3);

  auto count =
      std::count_if(evs.begin(), evs.end(), [](const event& ev) { return ev.is_readable(); });
  EXPECT_EQ(count, 2);
}
