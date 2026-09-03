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

#include <tio/sys/selector.hpp>

#include <fcntl.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

using tio::interest;
using tio::token;
using tio::sys::event_traits;
using tio::sys::raw_event;
using tio::sys::selector;

namespace {

struct pipe_fds {
  int read_end;
  int write_end;

  pipe_fds() {
    int fds[2]{-1, -1};
    EXPECT_EQ(::pipe(fds), 0);
    read_end = fds[0];
    write_end = fds[1];
  }

  ~pipe_fds() {
    if (read_end >= 0) {
      ::close(read_end);
    }
    if (write_end >= 0) {
      ::close(write_end);
    }
  }

  pipe_fds(const pipe_fds&) = delete;
  auto operator=(const pipe_fds&) -> pipe_fds& = delete;
};

struct socket_pair {
  int a;
  int b;

  socket_pair() {
    int fds[2]{-1, -1};
    EXPECT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
    a = fds[0];
    b = fds[1];
  }

  ~socket_pair() {
    if (a >= 0) {
      ::close(a);
    }
    if (b >= 0) {
      ::close(b);
    }
  }

  socket_pair(const socket_pair&) = delete;
  auto operator=(const socket_pair&) -> socket_pair& = delete;
};

auto token_of(const raw_event& ev) -> std::size_t { return event_traits::tok(ev).value(); }

}

TEST(kqueue_selector_test, create_succeeds) {
  auto sel = selector::create();
  ASSERT_TRUE(sel.has_value());
  EXPECT_GE(sel->raw_fd(), 0);
}

TEST(kqueue_selector_test, register_and_select_readable) {
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

  EXPECT_EQ(token_of(events[0]), 1u);
  EXPECT_TRUE(event_traits::is_readable(events[0]));
}

TEST(kqueue_selector_test, register_writable) {
  auto sel = selector::create().value();
  pipe_fds p;

  auto reg = sel.register_fd(p.write_end, token{42}, interest::writable());
  ASSERT_TRUE(reg.has_value());

  raw_event events[8];
  auto n = sel.select(events, 8, std::chrono::milliseconds{100});
  ASSERT_TRUE(n.has_value());
  EXPECT_EQ(n.value(), 1);
  EXPECT_EQ(token_of(events[0]), 42u);
  EXPECT_TRUE(event_traits::is_writable(events[0]));
}

TEST(kqueue_selector_test, multiple_fds) {
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
    if (token_of(events[i]) == 10u)
      found_10 = true;
    if (token_of(events[i]) == 20u)
      found_20 = true;
  }
  EXPECT_TRUE(found_10);
  EXPECT_TRUE(found_20);
}

TEST(kqueue_selector_test, reregister_changes_interest) {
  auto sel = selector::create().value();
  socket_pair s;

  sel.register_fd(s.a, token{1}, interest::writable()).value();

  sel.reregister_fd(s.a, token{1}, interest::readable()).value();

  raw_event events[8];
  auto n = sel.select(events, 8, std::chrono::milliseconds{50});
  ASSERT_TRUE(n.has_value());
  EXPECT_EQ(n.value(), 0);
}

TEST(kqueue_selector_test, deregister_stops_events) {
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

TEST(kqueue_selector_test, select_timeout_no_events) {
  auto sel = selector::create().value();
  pipe_fds p;

  sel.register_fd(p.read_end, token{1}, interest::readable()).value();

  raw_event events[8];
  auto n = sel.select(events, 8, std::chrono::milliseconds{10});
  ASSERT_TRUE(n.has_value());
  EXPECT_EQ(n.value(), 0);
}

TEST(kqueue_selector_test, register_duplicate_fd_replaces_registration) {
  auto sel = selector::create().value();
  pipe_fds p;

  sel.register_fd(p.read_end, token{1}, interest::readable()).value();

  auto r = sel.register_fd(p.read_end, token{2}, interest::readable());
  ASSERT_TRUE(r.has_value());

  char buf[] = "x";
  ::write(p.write_end, buf, 1);

  raw_event events[8];
  auto n = sel.select(events, 8, std::chrono::milliseconds{100});
  ASSERT_TRUE(n.has_value());
  EXPECT_EQ(n.value(), 1);
  EXPECT_EQ(token_of(events[0]), 2u);
}

TEST(kqueue_selector_test, deregister_unregistered_fd_fails) {
  auto sel = selector::create().value();
  pipe_fds p;

  auto r = sel.deregister_fd(p.read_end);
  EXPECT_FALSE(r.has_value());
  EXPECT_EQ(r.error().code(), ENOENT);
}

TEST(kqueue_selector_test, reregister_changes_token) {
  auto sel = selector::create().value();
  pipe_fds p;

  sel.register_fd(p.write_end, token{1}, interest::writable()).value();
  sel.reregister_fd(p.write_end, token{99}, interest::writable()).value();

  raw_event events[8];
  auto n = sel.select(events, 8, std::chrono::milliseconds{100});
  ASSERT_TRUE(n.has_value());
  EXPECT_EQ(n.value(), 1);
  EXPECT_EQ(token_of(events[0]), 99u);
}

TEST(kqueue_selector_test, readable_and_writable_are_separate_events) {
  auto sel = selector::create().value();
  socket_pair s;

  sel.register_fd(s.a, token{5}, interest::readable() | interest::writable()).value();

  char buf[] = "x";
  ::write(s.b, buf, 1);

  raw_event events[8];
  auto n = sel.select(events, 8, std::chrono::milliseconds{100});
  ASSERT_TRUE(n.has_value());
  EXPECT_EQ(n.value(), 2);

  bool readable = false;
  bool writable = false;
  for (int i = 0; i < n.value(); ++i) {
    EXPECT_EQ(token_of(events[i]), 5u);
    readable = readable || event_traits::is_readable(events[i]);
    writable = writable || event_traits::is_writable(events[i]);
  }
  EXPECT_TRUE(readable);
  EXPECT_TRUE(writable);
}

TEST(kqueue_selector_test, read_closed_on_peer_shutdown) {
  auto sel = selector::create().value();
  socket_pair s;

  sel.register_fd(s.a, token{7}, interest::readable()).value();
  ::close(s.b);
  s.b = ::dup(s.a);

  raw_event events[8];
  auto n = sel.select(events, 8, std::chrono::milliseconds{100});
  ASSERT_TRUE(n.has_value());
  ASSERT_EQ(n.value(), 1);
  EXPECT_TRUE(event_traits::is_read_closed(events[0]));
  EXPECT_FALSE(event_traits::is_error(events[0]));
}

TEST(kqueue_selector_test, error_on_refused_connect) {
  auto sel = selector::create().value();

  const int probe = ::socket(AF_INET, SOCK_STREAM, 0);
  ASSERT_GE(probe, 0);
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  ASSERT_EQ(::bind(probe, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0);
  socklen_t addr_len = sizeof(addr);
  ASSERT_EQ(::getsockname(probe, reinterpret_cast<sockaddr*>(&addr), &addr_len), 0);
  ::close(probe);

  const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  ASSERT_GE(fd, 0);
  ASSERT_EQ(::fcntl(fd, F_SETFL, O_NONBLOCK), 0);
  sel.register_fd(fd, token{9}, interest::readable() | interest::writable()).value();
  ::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

  raw_event events[8];
  auto n = sel.select(events, 8, std::chrono::milliseconds{500});
  ASSERT_TRUE(n.has_value());
  ASSERT_GT(n.value(), 0);

  bool saw_error = false;
  for (int i = 0; i < n.value(); ++i) {
    EXPECT_EQ(token_of(events[i]), 9u);
    saw_error = saw_error || event_traits::is_error(events[i]);
  }
  EXPECT_TRUE(saw_error);

  ::close(fd);
}

TEST(kqueue_selector_test, user_event_triggers_select) {
  auto sel = selector::create().value();

  sel.register_user_event(token{0xABC}).value();

  raw_event events[8];
  auto none = sel.select(events, 8, std::chrono::milliseconds{10});
  ASSERT_TRUE(none.has_value());
  EXPECT_EQ(none.value(), 0);

  sel.trigger_user_event(token{0xABC}).value();

  auto n = sel.select(events, 8, std::chrono::milliseconds{100});
  ASSERT_TRUE(n.has_value());
  ASSERT_EQ(n.value(), 1);
  EXPECT_EQ(token_of(events[0]), 0xABCu);
  EXPECT_TRUE(event_traits::is_readable(events[0]));

  auto after = sel.select(events, 8, std::chrono::milliseconds{10});
  ASSERT_TRUE(after.has_value());
  EXPECT_EQ(after.value(), 0);
}

TEST(kqueue_selector_test, try_clone_shares_the_queue) {
  auto sel = selector::create().value();
  pipe_fds p;

  sel.register_fd(p.read_end, token{3}, interest::readable()).value();

  auto clone = sel.try_clone().value();
  EXPECT_NE(clone.raw_fd(), sel.raw_fd());

  char buf[] = "x";
  ::write(p.write_end, buf, 1);

  raw_event events[8];
  auto n = clone.select(events, 8, std::chrono::milliseconds{100});
  ASSERT_TRUE(n.has_value());
  EXPECT_EQ(n.value(), 1);
  EXPECT_EQ(token_of(events[0]), 3u);
}

TEST(kqueue_selector_test, move_construct) {
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
  EXPECT_EQ(token_of(events[0]), 1u);
}
