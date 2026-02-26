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

#include <sys/socket.h>
#include <sys/uio.h>

#include <tio/net/tcp_listener.hpp>
#include <tio/net/tcp_stream.hpp>
#include <tio/poll.hpp>

#include <gtest/gtest.h>

using tio::events;
using tio::interest;
using tio::poll;
using tio::token;
using tio::detail::socket_addr;
using tio::net::tcp_listener;
using tio::net::tcp_stream;

namespace {

constexpr auto k_listener_token = token{0};
constexpr auto k_client_token = token{1};
constexpr auto k_server_token = token{2};

auto bind_listener() -> std::pair<tcp_listener, socket_addr> {
  const auto addr = socket_addr::ipv4_loopback(0);
  auto listener = tcp_listener::bind(addr).value();
  auto local = listener.local_addr().value();
  return {std::move(listener), local};
}

}

TEST(tcp_test, listener_bind_and_local_addr) {
  auto [listener, addr] = bind_listener();
  EXPECT_TRUE(addr.is_ipv4());
  EXPECT_GT(addr.port(), 0);
}

TEST(tcp_test, connect_and_accept) {
  auto [listener, addr] = bind_listener();

  auto client = tcp_stream::connect(addr).value();

  auto p = poll::create().value();
  auto reg = p.get_registry();
  reg.register_source(listener, k_listener_token, interest::readable()).value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{500}).value();
  ASSERT_GE(evs.size(), 1u);

  auto [server, peer] = listener.accept().value();
  EXPECT_TRUE(peer.is_ipv4());
  EXPECT_GT(peer.port(), 0);
}

TEST(tcp_test, write_and_read_roundtrip) {
  auto [listener, addr] = bind_listener();

  auto client = tcp_stream::connect(addr).value();

  auto p = poll::create().value();
  auto reg = p.get_registry();
  reg.register_source(listener, k_listener_token, interest::readable()).value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  auto [server, peer] = listener.accept().value();

  reg.register_source(client, k_client_token, interest::readable() | interest::writable()).value();
  reg.register_source(server, k_server_token, interest::readable() | interest::writable()).value();

  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  const char* msg = "hello mio";
  auto data = std::as_bytes(std::span{msg, std::strlen(msg)});
  auto written = client.write(data).value();
  EXPECT_EQ(written, std::strlen(msg));

  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  bool server_readable = false;
  for (const auto& ev : evs) {
    if (ev.tok() == k_server_token && ev.is_readable()) {
      server_readable = true;
    }
  }
  EXPECT_TRUE(server_readable);

  std::array<std::byte, 128> buf{};
  auto n = server.read(buf).value();
  EXPECT_EQ(n, std::strlen(msg));

  std::string received(reinterpret_cast<const char*>(buf.data()), n);
  EXPECT_EQ(received, "hello mio");
}

TEST(tcp_test, echo_roundtrip) {
  auto [listener, addr] = bind_listener();
  auto client = tcp_stream::connect(addr).value();

  auto p = poll::create().value();
  auto reg = p.get_registry();
  reg.register_source(listener, k_listener_token, interest::readable()).value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{500}).value();
  auto [server, _] = listener.accept().value();

  reg.register_source(client, k_client_token, interest::readable() | interest::writable()).value();
  reg.register_source(server, k_server_token, interest::readable() | interest::writable()).value();

  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  const char* msg = "echo test";
  client.write(std::as_bytes(std::span{msg, std::strlen(msg)})).value();

  p.do_poll(evs, std::chrono::milliseconds{500}).value();
  std::array<std::byte, 128> buf{};
  auto n = server.read(buf).value();

  server.write(std::span{buf.data(), n}).value();

  p.do_poll(evs, std::chrono::milliseconds{500}).value();
  std::array<std::byte, 128> buf2{};
  auto n2 = client.read(buf2).value();

  std::string echoed(reinterpret_cast<const char*>(buf2.data()), n2);
  EXPECT_EQ(echoed, "echo test");
}

TEST(tcp_test, peek) {
  auto [listener, addr] = bind_listener();
  auto client = tcp_stream::connect(addr).value();

  auto p = poll::create().value();
  auto reg = p.get_registry();
  reg.register_source(listener, k_listener_token, interest::readable()).value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{500}).value();
  auto [server, _] = listener.accept().value();

  reg.register_source(client, k_client_token, interest::writable()).value();
  reg.register_source(server, k_server_token, interest::readable()).value();

  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  const char* msg = "peek";
  client.write(std::as_bytes(std::span{msg, std::strlen(msg)})).value();

  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  std::array<std::byte, 128> buf{};
  auto n1 = server.peek(buf).value();
  EXPECT_EQ(n1, 4u);

  auto n2 = server.read(buf).value();
  EXPECT_EQ(n2, 4u);
}

TEST(tcp_test, set_nodelay) {
  auto [listener, addr] = bind_listener();
  auto client = tcp_stream::connect(addr).value();

  client.set_nodelay(true).value();
  EXPECT_TRUE(client.nodelay().value());

  client.set_nodelay(false).value();
  EXPECT_FALSE(client.nodelay().value());
}

TEST(tcp_test, shutdown_write) {
  auto [listener, addr] = bind_listener();
  auto client = tcp_stream::connect(addr).value();

  auto p = poll::create().value();
  auto reg = p.get_registry();
  reg.register_source(listener, k_listener_token, interest::readable()).value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{500}).value();
  auto [server, _] = listener.accept().value();

  client.shutdown(SHUT_WR).value();

  reg.register_source(server, k_server_token, interest::readable()).value();
  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  std::array<std::byte, 128> buf{};
  auto n = server.read(buf).value();
  EXPECT_EQ(n, 0u);
}

TEST(tcp_test, peer_and_local_addr) {
  auto [listener, server_addr] = bind_listener();
  auto client = tcp_stream::connect(server_addr).value();

  auto p = poll::create().value();
  auto reg = p.get_registry();
  reg.register_source(listener, k_listener_token, interest::readable()).value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{500}).value();
  auto [server, peer] = listener.accept().value();

  auto client_local = client.local_addr().value();
  EXPECT_EQ(client_local.port(), peer.port());

  auto server_local = server.local_addr().value();
  EXPECT_EQ(server_local.port(), server_addr.port());
}

TEST(tcp_test, would_block_on_empty_read) {
  auto [listener, addr] = bind_listener();
  auto client = tcp_stream::connect(addr).value();

  auto p = poll::create().value();
  auto reg = p.get_registry();
  reg.register_source(listener, k_listener_token, interest::readable()).value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{500}).value();
  auto [server, _] = listener.accept().value();

  std::array<std::byte, 128> buf{};
  auto r = server.read(buf);
  EXPECT_FALSE(r.has_value());
  EXPECT_TRUE(r.error().is_would_block());
}

TEST(tcp_test, accept_would_block) {
  auto [listener, addr] = bind_listener();

  auto r = listener.accept();
  EXPECT_FALSE(r.has_value());
  EXPECT_TRUE(r.error().is_would_block());
}

TEST(tcp_test, set_reuseaddr) {
  auto [listener, addr] = bind_listener();
  listener.set_reuseaddr(true).value();
  listener.set_reuseaddr(false).value();
}

TEST(tcp_test, set_reuseport) {
  auto [listener, addr] = bind_listener();
  listener.set_reuse_port(true).value();
  listener.set_reuse_port(false).value();
}

TEST(tcp_test, bidirectional) {
  auto [listener, addr] = bind_listener();
  auto client = tcp_stream::connect(addr).value();

  auto p = poll::create().value();
  auto reg = p.get_registry();
  reg.register_source(listener, k_listener_token, interest::readable()).value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{500}).value();
  auto [server, _] = listener.accept().value();

  reg.register_source(client, k_client_token, interest::readable() | interest::writable()).value();
  reg.register_source(server, k_server_token, interest::readable() | interest::writable()).value();

  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  const char* msg1 = "ping";
  client.write(std::as_bytes(std::span{msg1, std::strlen(msg1)})).value();

  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  std::array<std::byte, 128> buf{};
  auto n1 = server.read(buf).value();
  EXPECT_EQ(std::string(reinterpret_cast<const char*>(buf.data()), n1), "ping");

  const char* msg2 = "pong";
  server.write(std::as_bytes(std::span{msg2, std::strlen(msg2)})).value();

  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  auto n2 = client.read(buf).value();
  EXPECT_EQ(std::string(reinterpret_cast<const char*>(buf.data()), n2), "pong");
}

TEST(tcp_test, multiple_clients) {
  auto [listener, addr] = bind_listener();

  auto c1 = tcp_stream::connect(addr).value();
  auto c2 = tcp_stream::connect(addr).value();

  auto p = poll::create().value();
  auto reg = p.get_registry();
  reg.register_source(listener, k_listener_token, interest::readable()).value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  auto [s1, peer1] = listener.accept().value();
  auto [s2, peer2] = listener.accept().value();

  EXPECT_NE(peer1.port(), peer2.port());

  constexpr auto k_s1 = token{10};
  constexpr auto k_s2 = token{20};
  reg.register_source(s1, k_s1, interest::readable()).value();
  reg.register_source(s2, k_s2, interest::readable()).value();

  const char* msg1 = "from c1";
  c1.write(std::as_bytes(std::span{msg1, std::strlen(msg1)})).value();
  const char* msg2 = "from c2";
  c2.write(std::as_bytes(std::span{msg2, std::strlen(msg2)})).value();

  p.do_poll(evs, std::chrono::milliseconds{500}).value();
  EXPECT_EQ(evs.size(), 2u);

  std::array<std::byte, 128> buf{};
  auto n1 = s1.read(buf).value();
  EXPECT_EQ(std::string(reinterpret_cast<const char*>(buf.data()), n1), "from c1");

  auto n2 = s2.read(buf).value();
  EXPECT_EQ(std::string(reinterpret_cast<const char*>(buf.data()), n2), "from c2");
}

TEST(tcp_test, reregister_changes_interest) {
  auto [listener, addr] = bind_listener();
  auto client = tcp_stream::connect(addr).value();

  auto p = poll::create().value();
  auto reg = p.get_registry();
  reg.register_source(listener, k_listener_token, interest::readable()).value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{500}).value();
  auto [server, _] = listener.accept().value();

  reg.register_source(server, k_server_token, interest::writable()).value();

  reg.reregister_source(server, k_server_token, interest::readable()).value();

  p.do_poll(evs, std::chrono::milliseconds{50}).value();
  EXPECT_EQ(evs.size(), 0u);
}

TEST(tcp_test, deregister_stops_events) {
  auto [listener, addr] = bind_listener();
  auto client = tcp_stream::connect(addr).value();

  auto p = poll::create().value();
  auto reg = p.get_registry();
  reg.register_source(listener, k_listener_token, interest::readable()).value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{500}).value();
  auto [server, _] = listener.accept().value();

  reg.register_source(server, k_server_token, interest::readable()).value();

  const char* msg = "x";
  client.write(std::as_bytes(std::span{msg, 1})).value();

  reg.deregister_source(server).value();

  p.do_poll(evs, std::chrono::milliseconds{50}).value();
  EXPECT_EQ(evs.size(), 0u);
}

TEST(tcp_test, move_construct_listener) {
  auto [l1, addr] = bind_listener();
  auto l2 = std::move(l1);

  auto client = tcp_stream::connect(addr).value();

  auto p = poll::create().value();
  auto reg = p.get_registry();
  reg.register_source(l2, k_listener_token, interest::readable()).value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{500}).value();
  ASSERT_GE(evs.size(), 1u);

  auto [server, peer] = l2.accept().value();
  EXPECT_GT(peer.port(), 0);
}

TEST(tcp_test, move_construct_stream) {
  auto [listener, addr] = bind_listener();
  auto c1 = tcp_stream::connect(addr).value();

  auto p = poll::create().value();
  auto reg = p.get_registry();
  reg.register_source(listener, k_listener_token, interest::readable()).value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{500}).value();
  auto [s1, _] = listener.accept().value();

  auto s2 = std::move(s1);

  reg.register_source(s2, k_server_token, interest::readable()).value();

  const char* msg = "moved";
  c1.write(std::as_bytes(std::span{msg, std::strlen(msg)})).value();

  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  std::array<std::byte, 128> buf{};
  auto n = s2.read(buf).value();
  EXPECT_EQ(std::string(reinterpret_cast<const char*>(buf.data()), n), "moved");
}

TEST(tcp_test, source_concept) {
  static_assert(tio::source<tcp_listener>);
  static_assert(tio::source<tcp_stream>);
}

TEST(tcp_test, listener_set_ttl) {
  auto [listener, addr] = bind_listener();

  listener.set_ttl(128).value();
  EXPECT_EQ(listener.ttl().value(), 128u);

  listener.set_ttl(64).value();
  EXPECT_EQ(listener.ttl().value(), 64u);
}

TEST(tcp_test, listener_take_error) {
  auto [listener, addr] = bind_listener();
  auto err = listener.take_error().value();
  EXPECT_EQ(err.code(), 0);
}

TEST(tcp_test, stream_set_ttl) {
  auto [listener, addr] = bind_listener();
  auto client = tcp_stream::connect(addr).value();

  client.set_ttl(128).value();
  EXPECT_EQ(client.ttl().value(), 128u);

  client.set_ttl(64).value();
  EXPECT_EQ(client.ttl().value(), 64u);
}

TEST(tcp_test, stream_take_error) {
  auto [listener, addr] = bind_listener();
  auto client = tcp_stream::connect(addr).value();
  auto err = client.take_error().value();
  EXPECT_EQ(err.code(), 0);
}

TEST(tcp_test, listener_from_raw_fd) {
  auto [listener, addr] = bind_listener();
  const int fd = listener.into_raw_fd();
  EXPECT_GE(fd, 0);

  auto l2 = tcp_listener::from_raw_fd(fd);
  auto client = tcp_stream::connect(addr).value();

  auto p = poll::create().value();
  auto reg = p.get_registry();
  reg.register_source(l2, k_listener_token, interest::readable()).value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  auto [server, peer] = l2.accept().value();
  EXPECT_GT(peer.port(), 0);
}

TEST(tcp_test, stream_from_raw_fd) {
  auto [listener, addr] = bind_listener();
  auto c1 = tcp_stream::connect(addr).value();
  const int fd = c1.into_raw_fd();
  EXPECT_GE(fd, 0);

  auto c2 = tcp_stream::from_raw_fd(fd);
  EXPECT_EQ(c2.raw_fd(), fd);
}

TEST(tcp_test, write_vectored_and_read_vectored) {
  auto [listener, addr] = bind_listener();
  auto client = tcp_stream::connect(addr).value();

  auto p = poll::create().value();
  auto reg = p.get_registry();
  reg.register_source(listener, k_listener_token, interest::readable()).value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{500}).value();
  auto [server, _] = listener.accept().value();

  reg.register_source(client, k_client_token, interest::readable() | interest::writable()).value();
  reg.register_source(server, k_server_token, interest::readable() | interest::writable()).value();

  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  char part1[] = "hello";
  char part2[] = " world";
  iovec write_bufs[2] = {
      {part1, 5},
      {part2, 6},
  };
  auto written = client.write_vectored(write_bufs).value();
  EXPECT_EQ(written, 11u);

  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  std::array<std::byte, 64> buf1{};
  std::array<std::byte, 64> buf2{};
  iovec read_bufs[2] = {
      {buf1.data(), buf1.size()},
      {buf2.data(), buf2.size()},
  };
  auto n = server.read_vectored(read_bufs).value();
  EXPECT_EQ(n, 11u);

  std::string received(reinterpret_cast<const char*>(buf1.data()), n);
  EXPECT_EQ(received, "hello world");
}

TEST(tcp_test, registry_try_clone) {
  auto p = poll::create().value();
  auto reg1 = p.get_registry();
  auto reg2 = reg1.try_clone().value();

  auto [listener, addr] = bind_listener();
  reg1.register_source(listener, k_listener_token, interest::readable()).value();

  auto client = tcp_stream::connect(addr).value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{500}).value();
  ASSERT_GE(evs.size(), 1u);
}
