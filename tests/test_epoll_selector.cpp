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

#include <cstring>
#include <thread>

#include <tio/sys/selector.hpp>

#include <gtest/gtest.h>
#include <unistd.h>

using tio::interest;
using tio::token;
using tio::sys::raw_event;
using tio::sys::selector;

namespace {

struct pipe_fds {
  int read_end;
  int write_end;

  pipe_fds() {
    int fds[2];
    EXPECT_EQ(::pipe(fds), 0);
    read_end = fds[0];
    write_end = fds[1];
  }

  ~pipe_fds() {
    ::close(read_end);
    ::close(write_end);
  }

  pipe_fds(const pipe_fds&) = delete;
  auto operator=(const pipe_fds&) -> pipe_fds& = delete;
};

}

TEST(epoll_selector_test, create_succeeds) {
  auto sel = selector::create();
  ASSERT_TRUE(sel.has_value());
  EXPECT_GE(sel->raw_fd(), 0);
}

TEST(epoll_selector_test, register_and_select_readable) {
  auto sel = selector::create().value();
  pipe_fds p;

  auto reg = sel.register_fd(p.read_end, token{1}, interest::readable());
  ASSERT_TRUE(reg.has_value());

  char buf[] = "hello";
  ASSERT_EQ(::write(p.write_end, buf, sizeof(buf)), static_cast<ssize_t>(sizeof(buf)));

  raw_event events[8];
  auto n = sel.select(events, 8, std::chrono::milliseconds{100});
  ASSERT_TRUE(n.has_value());
  EXPECT_EQ(n.value(), 1);

  EXPECT_EQ(events[0].data.u64, 1u);
  EXPECT_TRUE((events[0].events & EPOLLIN) != 0);
}

TEST(epoll_selector_test, register_writable) {
  auto sel = selector::create().value();
  pipe_fds p;

  auto reg = sel.register_fd(p.write_end, token{42}, interest::writable());
  ASSERT_TRUE(reg.has_value());

  raw_event events[8];
  auto n = sel.select(events, 8, std::chrono::milliseconds{100});
  ASSERT_TRUE(n.has_value());
  EXPECT_EQ(n.value(), 1);
  EXPECT_EQ(events[0].data.u64, 42u);
  EXPECT_TRUE((events[0].events & EPOLLOUT) != 0);
}

TEST(epoll_selector_test, multiple_fds) {
  auto sel = selector::create().value();
  pipe_fds p1;
  pipe_fds p2;

  sel.register_fd(p1.read_end, token{10}, interest::readable()).value();
  sel.register_fd(p2.read_end, token{20}, interest::readable()).value();

  char buf[] = "x";
  ::write(p1.write_end, buf, 1);
  ::write(p2.write_end, buf, 1);

  raw_event events[8];
  auto n = sel.select(events, 8, std::chrono::milliseconds{100});
  ASSERT_TRUE(n.has_value());
  EXPECT_EQ(n.value(), 2);

  bool found_10 = false;
  bool found_20 = false;
  for (int i = 0; i < n.value(); ++i) {
    if (events[i].data.u64 == 10u)
      found_10 = true;
    if (events[i].data.u64 == 20u)
      found_20 = true;
  }
  EXPECT_TRUE(found_10);
  EXPECT_TRUE(found_20);
}

TEST(epoll_selector_test, reregister_changes_interest) {
  auto sel = selector::create().value();
  pipe_fds p;

  sel.register_fd(p.write_end, token{1}, interest::writable()).value();

  sel.reregister_fd(p.write_end, token{1}, interest::readable()).value();

  raw_event events[8];
  auto n = sel.select(events, 8, std::chrono::milliseconds{50});
  ASSERT_TRUE(n.has_value());
  EXPECT_EQ(n.value(), 0);
}

TEST(epoll_selector_test, deregister_stops_events) {
  auto sel = selector::create().value();
  pipe_fds p;

  sel.register_fd(p.read_end, token{1}, interest::readable()).value();

  char buf[] = "x";
  ::write(p.write_end, buf, 1);

  sel.deregister_fd(p.read_end).value();

  raw_event events[8];
  auto n = sel.select(events, 8, std::chrono::milliseconds{50});
  ASSERT_TRUE(n.has_value());
  EXPECT_EQ(n.value(), 0);
}

TEST(epoll_selector_test, select_timeout_no_events) {
  auto sel = selector::create().value();
  pipe_fds p;

  sel.register_fd(p.read_end, token{1}, interest::readable()).value();

  raw_event events[8];
  auto n = sel.select(events, 8, std::chrono::milliseconds{10});
  ASSERT_TRUE(n.has_value());
  EXPECT_EQ(n.value(), 0);
}

TEST(epoll_selector_test, register_duplicate_fd_fails) {
  auto sel = selector::create().value();
  pipe_fds p;

  sel.register_fd(p.read_end, token{1}, interest::readable()).value();

  auto r = sel.register_fd(p.read_end, token{2}, interest::readable());
  EXPECT_FALSE(r.has_value());
  EXPECT_TRUE(r.error().is_already_exists());
}

TEST(epoll_selector_test, deregister_unregistered_fd_fails) {
  auto sel = selector::create().value();
  pipe_fds p;

  auto r = sel.deregister_fd(p.read_end);
  EXPECT_FALSE(r.has_value());
  EXPECT_EQ(r.error().code(), ENOENT);
}

TEST(epoll_selector_test, reregister_changes_token) {
  auto sel = selector::create().value();
  pipe_fds p;

  sel.register_fd(p.write_end, token{1}, interest::writable()).value();
  sel.reregister_fd(p.write_end, token{99}, interest::writable()).value();

  raw_event events[8];
  auto n = sel.select(events, 8, std::chrono::milliseconds{100});
  ASSERT_TRUE(n.has_value());
  EXPECT_EQ(n.value(), 1);
  EXPECT_EQ(events[0].data.u64, 99u);
}

TEST(epoll_selector_test, readable_and_writable) {
  auto sel = selector::create().value();
  pipe_fds p;

  sel.register_fd(p.read_end, token{5}, interest::readable() | interest::writable()).value();

  char buf[] = "x";
  ::write(p.write_end, buf, 1);

  raw_event events[8];
  auto n = sel.select(events, 8, std::chrono::milliseconds{100});
  ASSERT_TRUE(n.has_value());
  EXPECT_GE(n.value(), 1);
  EXPECT_EQ(events[0].data.u64, 5u);
  EXPECT_TRUE((events[0].events & EPOLLIN) != 0);
}

TEST(epoll_selector_test, move_construct) {
  auto sel1 = selector::create().value();
  pipe_fds p;
  sel1.register_fd(p.read_end, token{1}, interest::readable()).value();

  auto sel2 = std::move(sel1);

  char buf[] = "x";
  ::write(p.write_end, buf, 1);

  raw_event events[8];
  auto n = sel2.select(events, 8, std::chrono::milliseconds{100});
  ASSERT_TRUE(n.has_value());
  EXPECT_EQ(n.value(), 1);
  EXPECT_EQ(events[0].data.u64, 1u);
}
