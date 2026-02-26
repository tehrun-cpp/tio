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

#include <thread>

#include <tio/poll.hpp>

#include <gtest/gtest.h>
#include <unistd.h>

using tio::events;
using tio::interest;
using tio::poll;
using tio::registry;
using tio::token;
using tio::void_result;

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

class test_source {
public:
  explicit test_source(int fd) noexcept : fd_{fd} {}

  auto mio_register(const registry& reg, token tok, interest intr) -> void_result {
    return reg.register_fd(fd_, tok, intr);
  }

  auto mio_reregister(const registry& reg, token tok, interest intr) -> void_result {
    return reg.reregister_fd(fd_, tok, intr);
  }

  auto mio_deregister(const registry& reg) -> void_result { return reg.deregister_fd(fd_); }

private:
  int fd_;
};

static_assert(tio::source<test_source>);

}

TEST(poll_test, create_succeeds) {
  auto p = poll::create();
  ASSERT_TRUE(p.has_value());
}

TEST(poll_test, poll_timeout_no_events) {
  auto p = poll::create().value();
  events evs{64};

  auto r = p.do_poll(evs, std::chrono::milliseconds{10});
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(evs.size(), 0u);
  EXPECT_TRUE(evs.is_empty());
}

TEST(poll_test, register_fd_and_poll_readable) {
  auto p = poll::create().value();
  pipe_fds pipe;

  auto reg = p.get_registry();
  auto r = reg.register_fd(pipe.read_end, token{1}, interest::readable());
  ASSERT_TRUE(r.has_value());

  char buf[] = "hello";
  ::write(pipe.write_end, buf, sizeof(buf));

  events evs{64};
  auto pr = p.do_poll(evs, std::chrono::milliseconds{100});
  ASSERT_TRUE(pr.has_value());
  EXPECT_EQ(evs.size(), 1u);
  EXPECT_EQ(evs[0].tok(), token{1});
  EXPECT_TRUE(evs[0].is_readable());
}

TEST(poll_test, register_source_and_poll) {
  auto p = poll::create().value();
  pipe_fds pipe;

  test_source src{pipe.read_end};
  auto reg = p.get_registry();
  auto r = reg.register_source(src, token{42}, interest::readable());
  ASSERT_TRUE(r.has_value());

  char buf[] = "x";
  ::write(pipe.write_end, buf, 1);

  events evs{64};
  auto pr = p.do_poll(evs, std::chrono::milliseconds{100});
  ASSERT_TRUE(pr.has_value());
  EXPECT_EQ(evs.size(), 1u);
  EXPECT_EQ(evs[0].tok(), token{42});
  EXPECT_TRUE(evs[0].is_readable());
}

TEST(poll_test, reregister_source) {
  auto p = poll::create().value();
  pipe_fds pipe;

  test_source src{pipe.write_end};
  auto reg = p.get_registry();

  reg.register_source(src, token{1}, interest::writable()).value();

  reg.reregister_source(src, token{1}, interest::readable()).value();

  events evs{64};
  auto pr = p.do_poll(evs, std::chrono::milliseconds{50});
  ASSERT_TRUE(pr.has_value());
  EXPECT_EQ(evs.size(), 0u);
}

TEST(poll_test, deregister_source) {
  auto p = poll::create().value();
  pipe_fds pipe;

  test_source src{pipe.read_end};
  auto reg = p.get_registry();
  reg.register_source(src, token{1}, interest::readable()).value();

  char buf[] = "x";
  ::write(pipe.write_end, buf, 1);

  reg.deregister_source(src).value();

  events evs{64};
  auto pr = p.do_poll(evs, std::chrono::milliseconds{50});
  ASSERT_TRUE(pr.has_value());
  EXPECT_EQ(evs.size(), 0u);
}

TEST(poll_test, multiple_sources) {
  auto p = poll::create().value();
  pipe_fds p1;
  pipe_fds p2;

  test_source s1{p1.read_end};
  test_source s2{p2.read_end};
  auto reg = p.get_registry();

  reg.register_source(s1, token{10}, interest::readable()).value();
  reg.register_source(s2, token{20}, interest::readable()).value();

  char buf[] = "x";
  ::write(p1.write_end, buf, 1);
  ::write(p2.write_end, buf, 1);

  events evs{64};
  auto pr = p.do_poll(evs, std::chrono::milliseconds{100});
  ASSERT_TRUE(pr.has_value());
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

TEST(poll_test, poll_clears_previous_events) {
  auto p = poll::create().value();
  pipe_fds pipe;

  auto reg = p.get_registry();
  reg.register_fd(pipe.read_end, token{1}, interest::readable()).value();

  char buf[] = "x";
  ::write(pipe.write_end, buf, 1);

  events evs{64};

  p.do_poll(evs, std::chrono::milliseconds{100}).value();
  EXPECT_EQ(evs.size(), 1u);

  char drain[16];
  ::read(pipe.read_end, drain, sizeof(drain));

  p.do_poll(evs, std::chrono::milliseconds{50}).value();
  EXPECT_EQ(evs.size(), 0u);
}

TEST(poll_test, move_construct) {
  auto p1 = poll::create().value();
  pipe_fds pipe;

  p1.get_registry().register_fd(pipe.read_end, token{1}, interest::readable()).value();

  auto p2 = std::move(p1);

  char buf[] = "x";
  ::write(pipe.write_end, buf, 1);

  events evs{64};
  auto pr = p2.do_poll(evs, std::chrono::milliseconds{100});
  ASSERT_TRUE(pr.has_value());
  EXPECT_EQ(evs.size(), 1u);
  EXPECT_EQ(evs[0].tok(), token{1});
}
