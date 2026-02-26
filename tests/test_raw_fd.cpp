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

#include <cstdint>

#include <sys/timerfd.h>

#include <tio/raw_fd.hpp>

#include <fcntl.h>
#include <gtest/gtest.h>
#include <unistd.h>

using tio::events;
using tio::interest;
using tio::poll;
using tio::raw_fd;
using tio::token;

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

TEST(raw_fd_test, source_concept_satisfied) {
  static_assert(tio::source<raw_fd>);
}

TEST(raw_fd_test, fd_accessor) {
  raw_fd r{42};
  EXPECT_EQ(r.fd(), 42);
}

TEST(raw_fd_test, register_pipe_and_poll_readable) {
  auto p = poll::create().value();
  pipe_fds pipe;

  raw_fd src{pipe.read_end};
  auto reg = p.get_registry();
  reg.register_source(src, token{1}, interest::readable()).value();

  char buf[] = "hello";
  ::write(pipe.write_end, buf, sizeof(buf));

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{100}).value();
  EXPECT_EQ(evs.size(), 1u);
  EXPECT_EQ(evs[0].tok(), token{1});
  EXPECT_TRUE(evs[0].is_readable());
}

TEST(raw_fd_test, register_pipe_writable) {
  auto p = poll::create().value();
  pipe_fds pipe;

  raw_fd src{pipe.write_end};
  auto reg = p.get_registry();
  reg.register_source(src, token{2}, interest::writable()).value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{100}).value();
  EXPECT_EQ(evs.size(), 1u);
  EXPECT_EQ(evs[0].tok(), token{2});
  EXPECT_TRUE(evs[0].is_writable());
}

TEST(raw_fd_test, reregister_changes_interest) {
  auto p = poll::create().value();
  pipe_fds pipe;

  raw_fd src{pipe.write_end};
  auto reg = p.get_registry();

  reg.register_source(src, token{1}, interest::writable()).value();

  reg.reregister_source(src, token{1}, interest::readable()).value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{50}).value();
  EXPECT_EQ(evs.size(), 0u);
}

TEST(raw_fd_test, deregister_stops_events) {
  auto p = poll::create().value();
  pipe_fds pipe;

  raw_fd src{pipe.read_end};
  auto reg = p.get_registry();
  reg.register_source(src, token{1}, interest::readable()).value();

  char buf[] = "x";
  ::write(pipe.write_end, buf, 1);

  reg.deregister_source(src).value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{50}).value();
  EXPECT_EQ(evs.size(), 0u);
}

TEST(raw_fd_test, timerfd_integration) {
  auto p = poll::create().value();

  int tfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
  ASSERT_GE(tfd, 0);

  struct itimerspec ts{};
  ts.it_value.tv_nsec = 10'000'000;
  ASSERT_EQ(::timerfd_settime(tfd, 0, &ts, nullptr), 0);

  raw_fd src{tfd};
  auto reg = p.get_registry();
  reg.register_source(src, token{100}, interest::readable()).value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{500}).value();
  EXPECT_EQ(evs.size(), 1u);
  EXPECT_EQ(evs[0].tok(), token{100});
  EXPECT_TRUE(evs[0].is_readable());

  std::uint64_t exp;
  ::read(tfd, &exp, sizeof(exp));
  EXPECT_EQ(exp, 1u);

  ::close(tfd);
}

TEST(raw_fd_test, multiple_raw_fds) {
  auto p = poll::create().value();
  pipe_fds p1;
  pipe_fds p2;

  raw_fd s1{p1.read_end};
  raw_fd s2{p2.read_end};
  auto reg = p.get_registry();

  reg.register_source(s1, token{10}, interest::readable()).value();
  reg.register_source(s2, token{20}, interest::readable()).value();

  char buf[] = "x";
  ::write(p1.write_end, buf, 1);
  ::write(p2.write_end, buf, 1);

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{100}).value();
  EXPECT_EQ(evs.size(), 2u);

  bool found_10 = false;
  bool found_20 = false;
  for (const auto& ev : evs) {
    if (ev.tok() == token{10})
      found_10 = true;
    if (ev.tok() == token{20})
      found_20 = true;
  }
  EXPECT_TRUE(found_10);
  EXPECT_TRUE(found_20);
}

TEST(raw_fd_test, does_not_own_fd) {
  pipe_fds pipe;
  {
    raw_fd src{pipe.read_end};
  }
  EXPECT_NE(::fcntl(pipe.read_end, F_GETFD), -1);
}
