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

#include <tio/sys/detail/fd_guard.hpp>

#include <fcntl.h>
#include <gtest/gtest.h>
#include <unistd.h>

using tio::detail::fd_guard;

namespace {

auto make_pipe() -> std::pair<int, int> {
  int fds[2];
  EXPECT_EQ(::pipe(fds), 0);
  return {fds[0], fds[1]};
}

auto is_fd_open(int fd) -> bool {
  return ::fcntl(fd, F_GETFD) != -1;
}

}

TEST(fd_guard_test, default_is_invalid) {
  fd_guard g;
  EXPECT_EQ(g.raw_fd(), -1);
  EXPECT_FALSE(static_cast<bool>(g));
}

TEST(fd_guard_test, wraps_valid_fd) {
  auto [r, w] = make_pipe();
  fd_guard gr{r};
  fd_guard gw{w};
  EXPECT_EQ(gr.raw_fd(), r);
  EXPECT_TRUE(static_cast<bool>(gr));
  EXPECT_TRUE(is_fd_open(r));
}

TEST(fd_guard_test, closes_on_destruction) {
  auto [r, w] = make_pipe();
  int saved_r = r;
  int saved_w = w;
  {
    fd_guard gr{r};
    fd_guard gw{w};
    EXPECT_TRUE(is_fd_open(saved_r));
    EXPECT_TRUE(is_fd_open(saved_w));
  }
  EXPECT_FALSE(is_fd_open(saved_r));
  EXPECT_FALSE(is_fd_open(saved_w));
}

TEST(fd_guard_test, move_construct) {
  auto [r, w] = make_pipe();
  fd_guard g1{r};
  fd_guard g2{std::move(g1)};

  EXPECT_EQ(g1.raw_fd(), -1);
  EXPECT_EQ(g2.raw_fd(), r);
  EXPECT_TRUE(is_fd_open(r));

  ::close(w);
}

TEST(fd_guard_test, move_assign) {
  auto [r1, w1] = make_pipe();
  auto [r2, w2] = make_pipe();
  int saved_r1 = r1;

  fd_guard g1{r1};
  fd_guard g2{r2};

  g2 = std::move(g1);

  EXPECT_EQ(g1.raw_fd(), -1);
  EXPECT_EQ(g2.raw_fd(), saved_r1);
  EXPECT_FALSE(is_fd_open(r2));
  EXPECT_TRUE(is_fd_open(saved_r1));

  ::close(w1);
  ::close(w2);
}

TEST(fd_guard_test, release) {
  auto [r, w] = make_pipe();
  fd_guard g{r};

  int released = g.release();
  EXPECT_EQ(released, r);
  EXPECT_EQ(g.raw_fd(), -1);
  EXPECT_TRUE(is_fd_open(r));

  ::close(r);
  ::close(w);
}

TEST(fd_guard_test, reset) {
  auto [r1, w1] = make_pipe();
  auto [r2, w2] = make_pipe();

  fd_guard g{r1};
  g.reset(r2);

  EXPECT_EQ(g.raw_fd(), r2);
  EXPECT_FALSE(is_fd_open(r1));
  EXPECT_TRUE(is_fd_open(r2));

  ::close(w1);
  ::close(w2);
}

TEST(fd_guard_test, reset_to_invalid) {
  auto [r, w] = make_pipe();
  fd_guard g{r};

  g.reset();
  EXPECT_EQ(g.raw_fd(), -1);
  EXPECT_FALSE(is_fd_open(r));

  ::close(w);
}

TEST(fd_guard_test, self_move_assign) {
  auto [r, w] = make_pipe();
  fd_guard g{r};

  auto* ptr = &g;
  *ptr = std::move(g);

  EXPECT_EQ(g.raw_fd(), r);
  EXPECT_TRUE(is_fd_open(r));

  ::close(w);
}
