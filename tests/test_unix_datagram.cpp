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
#include <filesystem>
#include <string>

#include <tio/poll.hpp>
#include <tio/unix/unix_datagram.hpp>

#include <gtest/gtest.h>

using tio::events;
using tio::interest;
using tio::poll;
using tio::token;
using tio::detail::unix_addr;
using tio::unix_::unix_datagram;

namespace {

constexpr auto k_a_token = token{1};
constexpr auto k_b_token = token{2};

class unix_datagram_test : public ::testing::Test {
protected:
  void SetUp() override {
    char tmpl[] = "/tmp/tio_test_XXXXXX";
    ASSERT_NE(::mkdtemp(tmpl), nullptr);
    dir_ = tmpl;
    path_a_ = dir_ + "/a.sock";
    path_b_ = dir_ + "/b.sock";
  }

  void TearDown() override {
    std::filesystem::remove_all(dir_);
  }

  auto addr_a() const -> unix_addr { return unix_addr::from_pathname(path_a_); }
  auto addr_b() const -> unix_addr { return unix_addr::from_pathname(path_b_); }

  std::string dir_;
  std::string path_a_;
  std::string path_b_;
};

}

TEST_F(unix_datagram_test, bind_and_local_addr) {
  auto sock = unix_datagram::bind(addr_a()).value();
  auto local = sock.local_addr().value();
  EXPECT_FALSE(local.is_unnamed());
  EXPECT_EQ(local.as_pathname(), path_a_);
}

TEST_F(unix_datagram_test, send_to_and_recv_from) {
  auto a = unix_datagram::bind(addr_a()).value();
  auto b = unix_datagram::bind(addr_b()).value();

  auto p = poll::create().value();
  auto reg = p.get_registry();
  reg.register_source(a, k_a_token, interest::writable()).value();
  reg.register_source(b, k_b_token, interest::readable()).value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  const char* msg = "hello dgram";
  auto data = std::as_bytes(std::span{msg, std::strlen(msg)});
  auto sent = a.send_to(data, addr_b()).value();
  EXPECT_EQ(sent, std::strlen(msg));

  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  std::array<std::byte, 128> buf{};
  auto [n, sender] = b.recv_from(buf).value();
  EXPECT_EQ(n, std::strlen(msg));

  std::string received(reinterpret_cast<const char*>(buf.data()), n);
  EXPECT_EQ(received, "hello dgram");
}

TEST_F(unix_datagram_test, connected_send_recv) {
  auto a = unix_datagram::bind(addr_a()).value();
  auto b = unix_datagram::bind(addr_b()).value();

  a.connect(addr_b()).value();
  b.connect(addr_a()).value();

  auto p = poll::create().value();
  auto reg = p.get_registry();
  reg.register_source(a, k_a_token, interest::writable()).value();
  reg.register_source(b, k_b_token, interest::readable()).value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  const char* msg = "connected";
  auto data = std::as_bytes(std::span{msg, std::strlen(msg)});
  a.send(data).value();

  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  std::array<std::byte, 128> buf{};
  auto n = b.recv(buf).value();
  EXPECT_EQ(n, std::strlen(msg));

  std::string received(reinterpret_cast<const char*>(buf.data()), n);
  EXPECT_EQ(received, "connected");
}

TEST_F(unix_datagram_test, bidirectional) {
  auto a = unix_datagram::bind(addr_a()).value();
  auto b = unix_datagram::bind(addr_b()).value();

  auto p = poll::create().value();
  auto reg = p.get_registry();
  reg.register_source(a, k_a_token, interest::readable() | interest::writable()).value();
  reg.register_source(b, k_b_token, interest::readable() | interest::writable()).value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  const char* msg1 = "ping";
  a.send_to(std::as_bytes(std::span{msg1, std::strlen(msg1)}), addr_b()).value();

  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  std::array<std::byte, 128> buf{};
  auto [n1, sender1] = b.recv_from(buf).value();
  EXPECT_EQ(std::string(reinterpret_cast<const char*>(buf.data()), n1), "ping");

  const char* msg2 = "pong";
  b.send_to(std::as_bytes(std::span{msg2, std::strlen(msg2)}), addr_a()).value();

  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  auto [n2, sender2] = a.recv_from(buf).value();
  EXPECT_EQ(std::string(reinterpret_cast<const char*>(buf.data()), n2), "pong");
}

TEST_F(unix_datagram_test, pair_send_recv) {
  auto [a, b] = unix_datagram::pair().value();

  auto p = poll::create().value();
  auto reg = p.get_registry();
  reg.register_source(a, k_a_token, interest::readable() | interest::writable()).value();
  reg.register_source(b, k_b_token, interest::readable() | interest::writable()).value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  const char* msg = "pair dgram";
  a.send(std::as_bytes(std::span{msg, std::strlen(msg)})).value();

  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  std::array<std::byte, 128> buf{};
  auto n = b.recv(buf).value();
  EXPECT_EQ(std::string(reinterpret_cast<const char*>(buf.data()), n), "pair dgram");
}

TEST_F(unix_datagram_test, would_block) {
  auto sock = unix_datagram::bind(addr_a()).value();

  std::array<std::byte, 128> buf{};
  auto r = sock.recv_from(buf);
  EXPECT_FALSE(r.has_value());
  EXPECT_TRUE(r.error().is_would_block());
}

TEST_F(unix_datagram_test, source_concept) {
  static_assert(tio::source<unix_datagram>);
}

TEST_F(unix_datagram_test, take_error) {
  auto sock = unix_datagram::bind(addr_a()).value();
  auto err = sock.take_error().value();
  EXPECT_EQ(err.code(), 0);
}

TEST_F(unix_datagram_test, unbound_connect_send) {
  auto b = unix_datagram::bind(addr_b()).value();
  auto a = unix_datagram::unbound().value();

  a.connect(addr_b()).value();

  auto p = poll::create().value();
  auto reg = p.get_registry();
  reg.register_source(a, k_a_token, interest::writable()).value();
  reg.register_source(b, k_b_token, interest::readable()).value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  const char* msg = "unbound";
  a.send(std::as_bytes(std::span{msg, std::strlen(msg)})).value();

  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  std::array<std::byte, 128> buf{};
  auto n = b.recv(buf).value();
  EXPECT_EQ(std::string(reinterpret_cast<const char*>(buf.data()), n), "unbound");
}

TEST_F(unix_datagram_test, from_raw_fd) {
  auto sock = unix_datagram::bind(addr_a()).value();
  const int fd = sock.into_raw_fd();
  EXPECT_GE(fd, 0);

  auto sock2 = unix_datagram::from_raw_fd(fd);
  EXPECT_EQ(sock2.raw_fd(), fd);
}
