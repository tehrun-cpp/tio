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
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

#include <sys/socket.h>

#include <tio/poll.hpp>
#include <tio/unix/unix_listener.hpp>
#include <tio/unix/unix_stream.hpp>

#include <gtest/gtest.h>

using tio::events;
using tio::interest;
using tio::poll;
using tio::token;
using tio::detail::unix_addr;
using tio::unix_::unix_listener;
using tio::unix_::unix_stream;

namespace {

constexpr auto k_listener_token = token{0};
constexpr auto k_client_token = token{1};
constexpr auto k_server_token = token{2};

auto temp_dir_template() -> std::string {
  const char* base = std::getenv("TMPDIR");
  std::string dir = (base != nullptr && *base != '\0') ? base : "/tmp";
  if (dir.back() == '/') {
    dir.pop_back();
  }
  return dir + "/tio_test_XXXXXX";
}

auto sock_file_creation_denied([[maybe_unused]] const std::string& path) -> bool {
#if defined(__ANDROID__)
  const auto probe = unix_listener::bind(unix_addr::from_pathname(path).value());
  if (!probe.has_value() && probe.error().code() == EACCES) {
    return true;
  }
  std::filesystem::remove(path);
#endif
  return false;
}

class unix_listener_test : public ::testing::Test {
protected:
  void SetUp() override {
    auto tmpl = temp_dir_template();
    ASSERT_NE(::mkdtemp(tmpl.data()), nullptr);
    dir_ = tmpl;
    path_ = dir_ + "/sock";

    if (sock_file_creation_denied(path_)) {
      GTEST_SKIP() << "sandbox denies socket-file creation under " << dir_;
    }
  }

  void TearDown() override {
    std::filesystem::remove_all(dir_);
  }

  auto addr() const -> unix_addr { return unix_addr::from_pathname(path_).value(); }

  std::string dir_;
  std::string path_;
};

}

TEST_F(unix_listener_test, bind_and_local_addr) {
  auto listener = unix_listener::bind(addr()).value();
  auto local = listener.local_addr().value();
  EXPECT_FALSE(local.is_unnamed());
  EXPECT_EQ(local.as_pathname(), path_);
}

TEST_F(unix_listener_test, connect_and_accept) {
  auto listener = unix_listener::bind(addr()).value();

  auto client = unix_stream::connect(addr()).value();

  auto p = poll::create().value();
  auto reg = p.get_registry();
  reg.register_source(listener, k_listener_token, interest::readable()).value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{500}).value();
  ASSERT_GE(evs.size(), 1u);

  auto [server, peer] = listener.accept().value();
  EXPECT_GE(server.raw_fd(), 0);
}

TEST_F(unix_listener_test, read_write_roundtrip) {
  auto listener = unix_listener::bind(addr()).value();
  auto client = unix_stream::connect(addr()).value();

  auto p = poll::create().value();
  auto reg = p.get_registry();
  reg.register_source(listener, k_listener_token, interest::readable()).value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{500}).value();
  auto [server, _] = listener.accept().value();

  reg.register_source(client, k_client_token, interest::readable() | interest::writable()).value();
  reg.register_source(server, k_server_token, interest::readable() | interest::writable()).value();

  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  const char* msg = "hello unix";
  auto data = std::as_bytes(std::span{msg, std::strlen(msg)});
  auto written = client.write(data).value();
  EXPECT_EQ(written, std::strlen(msg));

  p.do_poll(evs, std::chrono::milliseconds{500}).value();

  std::array<std::byte, 128> buf{};
  auto n = server.read(buf).value();
  EXPECT_EQ(n, std::strlen(msg));

  std::string received(reinterpret_cast<const char*>(buf.data()), n);
  EXPECT_EQ(received, "hello unix");
}

TEST_F(unix_listener_test, echo) {
  auto listener = unix_listener::bind(addr()).value();
  auto client = unix_stream::connect(addr()).value();

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

TEST_F(unix_listener_test, peek) {
  auto listener = unix_listener::bind(addr()).value();
  auto client = unix_stream::connect(addr()).value();

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

TEST_F(unix_listener_test, shutdown) {
  auto listener = unix_listener::bind(addr()).value();
  auto client = unix_stream::connect(addr()).value();

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

TEST_F(unix_listener_test, addresses) {
  auto listener = unix_listener::bind(addr()).value();
  auto client = unix_stream::connect(addr()).value();

  auto p = poll::create().value();
  auto reg = p.get_registry();
  reg.register_source(listener, k_listener_token, interest::readable()).value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{500}).value();
  auto [server, _] = listener.accept().value();

  auto server_local = server.local_addr().value();
  EXPECT_EQ(server_local.as_pathname(), path_);
}

TEST_F(unix_listener_test, would_block) {
  auto listener = unix_listener::bind(addr()).value();

  auto r = listener.accept();
  EXPECT_FALSE(r.has_value());
  EXPECT_TRUE(r.error().is_would_block());
}

TEST_F(unix_listener_test, move_construct) {
  auto l1 = unix_listener::bind(addr()).value();
  auto l2 = std::move(l1);

  auto client = unix_stream::connect(addr()).value();

  auto p = poll::create().value();
  auto reg = p.get_registry();
  reg.register_source(l2, k_listener_token, interest::readable()).value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{500}).value();
  ASSERT_GE(evs.size(), 1u);

  auto [server, peer] = l2.accept().value();
  EXPECT_GE(server.raw_fd(), 0);
}

TEST_F(unix_listener_test, source_concept) {
  static_assert(tio::source<unix_listener>);
}

TEST_F(unix_listener_test, take_error) {
  auto listener = unix_listener::bind(addr()).value();
  auto err = listener.take_error().value();
  EXPECT_EQ(err.code(), 0);
}

TEST(unix_addr_test, from_pathname_accepts_the_longest_path_that_fits) {
  const std::string path(sizeof(sockaddr_un::sun_path) - 1, 'a');

  auto addr = unix_addr::from_pathname(path);
  ASSERT_TRUE(addr.has_value());
  EXPECT_EQ(addr->as_pathname(), path);
}

TEST(unix_addr_test, from_pathname_rejects_a_path_that_does_not_fit) {
  const std::string path(sizeof(sockaddr_un::sun_path), 'a');

  auto addr = unix_addr::from_pathname(path);
  ASSERT_FALSE(addr.has_value());
  EXPECT_TRUE(addr.error().is_name_too_long());
}

TEST(unix_addr_test, from_pathname_round_trips_a_short_path) {
  auto addr = unix_addr::from_pathname("/tmp/tio.sock");
  ASSERT_TRUE(addr.has_value());
  EXPECT_FALSE(addr->is_unnamed());
  EXPECT_EQ(addr->as_pathname(), "/tmp/tio.sock");
}
