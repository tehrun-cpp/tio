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

#include <tio/net/udp_socket.hpp>
#include <tio/poll.hpp>

#include <gtest/gtest.h>

using tio::events;
using tio::interest;
using tio::poll;
using tio::token;
using tio::detail::socket_addr;
using tio::net::udp_socket;

namespace {

constexpr auto k_sock_a = token{1};
constexpr auto k_sock_b = token{2};

auto bind_udp() -> std::pair<udp_socket, socket_addr> {
  auto addr = socket_addr::ipv4_loopback(0);
  auto sock = udp_socket::bind(addr).value();
  auto local = sock.local_addr().value();
  return {std::move(sock), local};
}

}

TEST(udp_test, bind_and_local_addr) {
  auto [sock, addr] = bind_udp();
  EXPECT_TRUE(addr.is_ipv4());
  EXPECT_GT(addr.port(), 0);
}

TEST(udp_test, send_to_and_recv_from) {
  auto [a, addr_a] = bind_udp();
  auto [b, addr_b] = bind_udp();

  auto p = poll::create().value();
  auto reg = p.get_registry();
  reg.register_source(a, k_sock_a, interest::writable()).value();
  reg.register_source(b, k_sock_b, interest::readable()).value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  const char* msg = "hello udp";
  auto data = std::as_bytes(std::span{msg, std::strlen(msg)});
  auto sent = a.send_to(data, addr_b).value();
  EXPECT_EQ(sent, std::strlen(msg));

  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  bool b_readable = false;
  for (const auto& ev : evs) {
    if (ev.tok() == k_sock_b && ev.is_readable()) {
      b_readable = true;
    }
  }
  EXPECT_TRUE(b_readable);

  std::array<std::byte, 128> buf{};
  auto [n, sender] = b.recv_from(buf).value();
  EXPECT_EQ(n, std::strlen(msg));
  EXPECT_EQ(sender.port(), addr_a.port());

  std::string received(reinterpret_cast<const char*>(buf.data()), n);
  EXPECT_EQ(received, "hello udp");
}

TEST(udp_test, connected_send_recv) {
  auto [a, addr_a] = bind_udp();
  auto [b, addr_b] = bind_udp();

  a.connect(addr_b).value();
  b.connect(addr_a).value();

  auto p = poll::create().value();
  auto reg = p.get_registry();
  reg.register_source(a, k_sock_a, interest::writable()).value();
  reg.register_source(b, k_sock_b, interest::readable()).value();

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

TEST(udp_test, bidirectional) {
  auto [a, addr_a] = bind_udp();
  auto [b, addr_b] = bind_udp();

  auto p = poll::create().value();
  auto reg = p.get_registry();
  reg.register_source(a, k_sock_a, interest::readable() | interest::writable()).value();
  reg.register_source(b, k_sock_b, interest::readable() | interest::writable()).value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  const char* msg1 = "ping";
  a.send_to(std::as_bytes(std::span{msg1, std::strlen(msg1)}), addr_b).value();

  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  std::array<std::byte, 128> buf{};
  auto [n1, sender1] = b.recv_from(buf).value();
  EXPECT_EQ(std::string(reinterpret_cast<const char*>(buf.data()), n1), "ping");

  const char* msg2 = "pong";
  b.send_to(std::as_bytes(std::span{msg2, std::strlen(msg2)}), addr_a).value();

  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  auto [n2, sender2] = a.recv_from(buf).value();
  EXPECT_EQ(std::string(reinterpret_cast<const char*>(buf.data()), n2), "pong");
}

TEST(udp_test, would_block_on_empty_recv) {
  auto [sock, addr] = bind_udp();

  std::array<std::byte, 128> buf{};
  auto r = sock.recv_from(buf);
  EXPECT_FALSE(r.has_value());
  EXPECT_TRUE(r.error().is_would_block());
}

TEST(udp_test, source_concept) {
  static_assert(tio::source<udp_socket>);
}

TEST(udp_test, set_ttl) {
  auto [sock, addr] = bind_udp();

  sock.set_ttl(128).value();
  EXPECT_EQ(sock.ttl().value(), 128u);

  sock.set_ttl(64).value();
  EXPECT_EQ(sock.ttl().value(), 64u);
}

TEST(udp_test, broadcast_get_set) {
  auto [sock, addr] = bind_udp();

  sock.set_broadcast(true).value();
  EXPECT_TRUE(sock.broadcast().value());

  sock.set_broadcast(false).value();
  EXPECT_FALSE(sock.broadcast().value());
}

TEST(udp_test, take_error) {
  auto [sock, addr] = bind_udp();
  auto err = sock.take_error().value();
  EXPECT_EQ(err.code(), 0);
}

TEST(udp_test, peer_addr_after_connect) {
  auto [a, addr_a] = bind_udp();
  auto [b, addr_b] = bind_udp();

  a.connect(addr_b).value();
  auto peer = a.peer_addr().value();
  EXPECT_EQ(peer.port(), addr_b.port());
}

TEST(udp_test, multicast_loop_v4) {
  auto [sock, addr] = bind_udp();

  sock.set_multicast_loop_v4(true).value();
  EXPECT_TRUE(sock.multicast_loop_v4().value());

  sock.set_multicast_loop_v4(false).value();
  EXPECT_FALSE(sock.multicast_loop_v4().value());
}

TEST(udp_test, multicast_ttl_v4) {
  auto [sock, addr] = bind_udp();

  sock.set_multicast_ttl_v4(10).value();
  EXPECT_EQ(sock.multicast_ttl_v4().value(), 10u);
}

TEST(udp_test, only_v6) {
  auto addr = socket_addr::ipv6_any(0);
  auto sock = udp_socket::bind(addr).value();
  auto v6only = sock.only_v6();
  EXPECT_TRUE(v6only.has_value());
}

TEST(udp_test, multicast_loop_v6) {
  auto addr = socket_addr::ipv6_any(0);
  auto sock = udp_socket::bind(addr).value();

  sock.set_multicast_loop_v6(true).value();
  EXPECT_TRUE(sock.multicast_loop_v6().value());

  sock.set_multicast_loop_v6(false).value();
  EXPECT_FALSE(sock.multicast_loop_v6().value());
}

TEST(udp_test, peek) {
  auto [a, addr_a] = bind_udp();
  auto [b, addr_b] = bind_udp();

  a.connect(addr_b).value();
  b.connect(addr_a).value();

  auto p = poll::create().value();
  auto reg = p.get_registry();
  reg.register_source(a, k_sock_a, interest::writable()).value();
  reg.register_source(b, k_sock_b, interest::readable()).value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  const char* msg = "peek";
  a.send(std::as_bytes(std::span{msg, std::strlen(msg)})).value();

  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  std::array<std::byte, 128> buf{};
  auto n1 = b.peek(buf).value();
  EXPECT_EQ(n1, 4u);

  auto n2 = b.recv(buf).value();
  EXPECT_EQ(n2, 4u);
}

TEST(udp_test, peek_from) {
  auto [a, addr_a] = bind_udp();
  auto [b, addr_b] = bind_udp();

  auto p = poll::create().value();
  auto reg = p.get_registry();
  reg.register_source(a, k_sock_a, interest::writable()).value();
  reg.register_source(b, k_sock_b, interest::readable()).value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  const char* msg = "peekfrom";
  a.send_to(std::as_bytes(std::span{msg, std::strlen(msg)}), addr_b).value();

  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  std::array<std::byte, 128> buf{};
  auto [n1, sender1] = b.peek_from(buf).value();
  EXPECT_EQ(n1, 8u);
  EXPECT_EQ(sender1.port(), addr_a.port());

  auto [n2, sender2] = b.recv_from(buf).value();
  EXPECT_EQ(n2, 8u);
}

TEST(udp_test, from_raw_fd) {
  auto [sock, addr] = bind_udp();
  const int fd = sock.into_raw_fd();
  EXPECT_GE(fd, 0);

  auto sock2 = udp_socket::from_raw_fd(fd);
  EXPECT_EQ(sock2.raw_fd(), fd);
}
