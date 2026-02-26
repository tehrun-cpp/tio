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

#include <tio/poll.hpp>
#include <tio/unix/pipe.hpp>

#include <gtest/gtest.h>

using tio::events;
using tio::interest;
using tio::poll;
using tio::token;
using tio::unix_::make_pipe;
using tio::unix_::pipe_receiver;
using tio::unix_::pipe_sender;

namespace {

constexpr auto k_sender_token = token{1};
constexpr auto k_receiver_token = token{2};

}

TEST(pipe_test, write_and_read) {
  auto [sender, receiver] = make_pipe().value();

  auto p = poll::create().value();
  auto reg = p.get_registry();
  reg.register_source(sender, k_sender_token, interest::writable()).value();
  reg.register_source(receiver, k_receiver_token, interest::readable()).value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  const char* msg = "hello pipe";
  auto data = std::as_bytes(std::span{msg, std::strlen(msg)});
  auto written = sender.write(data).value();
  EXPECT_EQ(written, std::strlen(msg));

  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  bool recv_readable = false;
  for (const auto& ev : evs) {
    if (ev.tok() == k_receiver_token && ev.is_readable()) {
      recv_readable = true;
    }
  }
  EXPECT_TRUE(recv_readable);

  std::array<std::byte, 128> buf{};
  auto n = receiver.read(buf).value();
  EXPECT_EQ(n, std::strlen(msg));

  std::string received(reinterpret_cast<const char*>(buf.data()), n);
  EXPECT_EQ(received, "hello pipe");
}

TEST(pipe_test, would_block_on_empty_read) {
  auto [sender, receiver] = make_pipe().value();

  std::array<std::byte, 128> buf{};
  auto r = receiver.read(buf);
  EXPECT_FALSE(r.has_value());
  EXPECT_TRUE(r.error().is_would_block());
}

TEST(pipe_test, close_sender_eof) {
  auto [sender, receiver] = make_pipe().value();

  auto p = poll::create().value();
  auto reg = p.get_registry();
  reg.register_source(receiver, k_receiver_token, interest::readable()).value();

  {
    auto s = std::move(sender);
  }

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  std::array<std::byte, 128> buf{};
  auto n = receiver.read(buf).value();
  EXPECT_EQ(n, 0u);
}

TEST(pipe_test, move_semantics) {
  auto [sender, receiver] = make_pipe().value();

  auto sender2 = std::move(sender);
  auto receiver2 = std::move(receiver);

  const char* msg = "moved";
  sender2.write(std::as_bytes(std::span{msg, std::strlen(msg)})).value();

  auto p = poll::create().value();
  auto reg = p.get_registry();
  reg.register_source(receiver2, k_receiver_token, interest::readable()).value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  std::array<std::byte, 128> buf{};
  auto n = receiver2.read(buf).value();
  EXPECT_EQ(std::string(reinterpret_cast<const char*>(buf.data()), n), "moved");
}

TEST(pipe_test, source_concept) {
  static_assert(tio::source<pipe_sender>);
  static_assert(tio::source<pipe_receiver>);
}

TEST(pipe_test, set_nonblocking) {
  auto [sender, receiver] = make_pipe().value();

  sender.set_nonblocking(false).value();
  sender.set_nonblocking(true).value();

  receiver.set_nonblocking(false).value();
  receiver.set_nonblocking(true).value();
}

TEST(pipe_test, from_raw_fd) {
  auto [sender, receiver] = make_pipe().value();
  const int sfd = sender.into_raw_fd();
  const int rfd = receiver.into_raw_fd();
  EXPECT_GE(sfd, 0);
  EXPECT_GE(rfd, 0);

  auto sender2 = pipe_sender::from_raw_fd(sfd);
  auto receiver2 = pipe_receiver::from_raw_fd(rfd);

  const char* msg = "raw";
  sender2.write(std::as_bytes(std::span{msg, std::strlen(msg)})).value();

  auto p = poll::create().value();
  auto reg = p.get_registry();
  reg.register_source(receiver2, k_receiver_token, interest::readable()).value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  std::array<std::byte, 128> buf{};
  auto n = receiver2.read(buf).value();
  EXPECT_EQ(std::string(reinterpret_cast<const char*>(buf.data()), n), "raw");
}
