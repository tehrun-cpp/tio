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

#include <array>
#include <cstring>

#include <sys/uio.h>

#include <tio/poll.hpp>
#include <tio/unix/unix_stream.hpp>

#include <gtest/gtest.h>

using tio::events;
using tio::interest;
using tio::poll;
using tio::token;
using tio::unix_::unix_stream;

namespace {

constexpr auto k_a_token = token{1};
constexpr auto k_b_token = token{2};

}

TEST(unix_stream_test, pair_and_write_read) {
  auto [a, b] = unix_stream::pair().value();

  auto p = poll::create().value();
  auto reg = p.get_registry();
  reg.register_source(a, k_a_token, interest::readable() | interest::writable()).value();
  reg.register_source(b, k_b_token, interest::readable() | interest::writable()).value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  const char* msg = "hello pair";
  a.write(std::as_bytes(std::span{msg, std::strlen(msg)})).value();

  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  std::array<std::byte, 128> buf{};
  auto n = b.read(buf).value();
  EXPECT_EQ(n, std::strlen(msg));

  std::string received(reinterpret_cast<const char*>(buf.data()), n);
  EXPECT_EQ(received, "hello pair");
}

TEST(unix_stream_test, bidirectional) {
  auto [a, b] = unix_stream::pair().value();

  auto p = poll::create().value();
  auto reg = p.get_registry();
  reg.register_source(a, k_a_token, interest::readable() | interest::writable()).value();
  reg.register_source(b, k_b_token, interest::readable() | interest::writable()).value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  const char* msg1 = "ping";
  a.write(std::as_bytes(std::span{msg1, std::strlen(msg1)})).value();

  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  std::array<std::byte, 128> buf{};
  auto n1 = b.read(buf).value();
  EXPECT_EQ(std::string(reinterpret_cast<const char*>(buf.data()), n1), "ping");

  const char* msg2 = "pong";
  b.write(std::as_bytes(std::span{msg2, std::strlen(msg2)})).value();

  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  auto n2 = a.read(buf).value();
  EXPECT_EQ(std::string(reinterpret_cast<const char*>(buf.data()), n2), "pong");
}

TEST(unix_stream_test, move_construct) {
  auto [a, b] = unix_stream::pair().value();
  auto c = std::move(a);

  auto p = poll::create().value();
  auto reg = p.get_registry();
  reg.register_source(c, k_a_token, interest::readable() | interest::writable()).value();
  reg.register_source(b, k_b_token, interest::readable()).value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  const char* msg = "moved";
  c.write(std::as_bytes(std::span{msg, std::strlen(msg)})).value();

  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  std::array<std::byte, 128> buf{};
  auto n = b.read(buf).value();
  EXPECT_EQ(std::string(reinterpret_cast<const char*>(buf.data()), n), "moved");
}

TEST(unix_stream_test, source_concept) {
  static_assert(tio::source<unix_stream>);
}

TEST(unix_stream_test, take_error) {
  auto [a, b] = unix_stream::pair().value();
  auto err = a.take_error().value();
  EXPECT_EQ(err.code(), 0);
}

TEST(unix_stream_test, would_block_on_empty_read) {
  auto [a, b] = unix_stream::pair().value();

  std::array<std::byte, 128> buf{};
  auto r = b.read(buf);
  EXPECT_FALSE(r.has_value());
  EXPECT_TRUE(r.error().is_would_block());
}

TEST(unix_stream_test, write_vectored_and_read_vectored) {
  auto [a, b] = unix_stream::pair().value();

  auto p = poll::create().value();
  auto reg = p.get_registry();
  reg.register_source(a, k_a_token, interest::readable() | interest::writable()).value();
  reg.register_source(b, k_b_token, interest::readable() | interest::writable()).value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  char part1[] = "hello";
  char part2[] = " unix";
  iovec write_bufs[2] = {
      {part1, 5},
      {part2, 5},
  };
  auto written = a.write_vectored(write_bufs).value();
  EXPECT_EQ(written, 10u);

  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  std::array<std::byte, 64> buf1{};
  iovec read_bufs[1] = {
      {buf1.data(), buf1.size()},
  };
  auto n = b.read_vectored(read_bufs).value();
  EXPECT_EQ(n, 10u);

  std::string received(reinterpret_cast<const char*>(buf1.data()), n);
  EXPECT_EQ(received, "hello unix");
}

TEST(unix_stream_test, from_raw_fd) {
  auto [a, b] = unix_stream::pair().value();
  const int fd = a.into_raw_fd();
  EXPECT_GE(fd, 0);

  auto a2 = unix_stream::from_raw_fd(fd);
  EXPECT_EQ(a2.raw_fd(), fd);
}
