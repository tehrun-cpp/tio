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

auto make_raw(std::uint64_t tok, std::uint32_t flags) -> raw_event {
  raw_event ev{};
  ev.data.u64 = tok;
  ev.events = flags;
  return ev;
}

}

TEST(event_test, token) {
  auto raw = make_raw(42, 0);
  event ev{raw};
  EXPECT_EQ(ev.tok(), token{42});
}

TEST(event_test, is_readable) {
  auto raw = make_raw(1, EPOLLIN);
  event ev{raw};
  EXPECT_TRUE(ev.is_readable());
  EXPECT_FALSE(ev.is_writable());
}

TEST(event_test, is_writable) {
  auto raw = make_raw(1, EPOLLOUT);
  event ev{raw};
  EXPECT_TRUE(ev.is_writable());
  EXPECT_FALSE(ev.is_readable());
}

TEST(event_test, is_error) {
  auto raw = make_raw(1, EPOLLERR);
  event ev{raw};
  EXPECT_TRUE(ev.is_error());
}

TEST(event_test, is_read_closed_hup) {
  auto raw = make_raw(1, EPOLLHUP);
  event ev{raw};
  EXPECT_TRUE(ev.is_read_closed());
}

TEST(event_test, is_read_closed_rdhup) {
  auto raw = make_raw(1, EPOLLRDHUP);
  event ev{raw};
  EXPECT_TRUE(ev.is_read_closed());
}

TEST(event_test, is_write_closed_hup) {
  auto raw = make_raw(1, EPOLLHUP);
  event ev{raw};
  EXPECT_TRUE(ev.is_write_closed());
}

TEST(event_test, is_write_closed_err) {
  auto raw = make_raw(1, EPOLLERR);
  event ev{raw};
  EXPECT_TRUE(ev.is_write_closed());
}

TEST(event_test, is_priority) {
  auto raw = make_raw(1, EPOLLPRI);
  event ev{raw};
  EXPECT_TRUE(ev.is_priority());
  EXPECT_FALSE(ev.is_readable());
}

TEST(event_test, combined_flags) {
  auto raw = make_raw(1, EPOLLIN | EPOLLOUT);
  event ev{raw};
  EXPECT_TRUE(ev.is_readable());
  EXPECT_TRUE(ev.is_writable());
  EXPECT_FALSE(ev.is_error());
}

TEST(event_test, raw_access) {
  auto raw = make_raw(7, EPOLLIN);
  event ev{raw};
  EXPECT_EQ(ev.raw().data.u64, 7u);
  EXPECT_EQ(ev.raw().events, EPOLLIN);
}

TEST(events_test, initial_state) {
  events evs{128};
  EXPECT_EQ(evs.capacity(), 128u);
  EXPECT_EQ(evs.size(), 0u);
  EXPECT_TRUE(evs.is_empty());
}

TEST(events_test, set_len_and_access) {
  events evs{8};

  evs.raw_buf()[0] = make_raw(10, EPOLLIN);
  evs.raw_buf()[1] = make_raw(20, EPOLLOUT);
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
  evs.raw_buf()[0] = make_raw(1, EPOLLIN);
  evs.set_len(1);

  evs.clear();
  EXPECT_EQ(evs.size(), 0u);
  EXPECT_TRUE(evs.is_empty());
}

TEST(events_test, range_for_iteration) {
  events evs{8};
  evs.raw_buf()[0] = make_raw(1, EPOLLIN);
  evs.raw_buf()[1] = make_raw(2, EPOLLOUT);
  evs.raw_buf()[2] = make_raw(3, EPOLLIN | EPOLLOUT);
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
  evs.raw_buf()[0] = make_raw(5, EPOLLIN);
  evs.raw_buf()[1] = make_raw(10, EPOLLOUT);
  evs.raw_buf()[2] = make_raw(15, EPOLLIN);
  evs.set_len(3);

  auto count =
      std::count_if(evs.begin(), evs.end(), [](const event& ev) { return ev.is_readable(); });
  EXPECT_EQ(count, 2);
}
