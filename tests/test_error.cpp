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

#include <format>

#include <tio/error.hpp>

#include <gtest/gtest.h>

using tio::error;
using tio::result;
using tio::void_result;

TEST(error_test, default_constructed_is_zero) {
  error e;
  EXPECT_EQ(e.code(), 0);
}

TEST(error_test, explicit_code) {
  error e{EAGAIN};
  EXPECT_EQ(e.code(), EAGAIN);
}

TEST(error_test, is_would_block) {
  EXPECT_TRUE(error{EAGAIN}.is_would_block());
  EXPECT_TRUE(error{EWOULDBLOCK}.is_would_block());
  EXPECT_FALSE(error{EINTR}.is_would_block());
}

TEST(error_test, is_interrupted) {
  EXPECT_TRUE(error{EINTR}.is_interrupted());
  EXPECT_FALSE(error{EAGAIN}.is_interrupted());
}

TEST(error_test, is_connection_refused) {
  EXPECT_TRUE(error{ECONNREFUSED}.is_connection_refused());
  EXPECT_FALSE(error{0}.is_connection_refused());
}

TEST(error_test, is_connection_reset) {
  EXPECT_TRUE(error{ECONNRESET}.is_connection_reset());
}

TEST(error_test, is_addr_in_use) {
  EXPECT_TRUE(error{EADDRINUSE}.is_addr_in_use());
}

TEST(error_test, is_in_progress) {
  EXPECT_TRUE(error{EINPROGRESS}.is_in_progress());
}

TEST(error_test, equality) {
  EXPECT_EQ(error{EAGAIN}, error{EAGAIN});
  EXPECT_NE(error{EAGAIN}, error{EINTR});
}

TEST(error_test, ordering) {
  EXPECT_LT(error{1}, error{2});
}

TEST(error_test, last_os_error) {
  errno = ENOENT;
  auto e = error::last_os_error();
  EXPECT_EQ(e.code(), ENOENT);
}

TEST(error_test, message_not_empty) {
  error e{ENOENT};
  EXPECT_FALSE(e.message().empty());
}

TEST(error_test, format) {
  error e{ENOENT};
  auto s = std::format("{}", e);
  EXPECT_TRUE(s.starts_with("error("));
  EXPECT_NE(s.find("No such file"), std::string::npos);
}

TEST(error_test, result_with_value) {
  result<int> r{42};
  EXPECT_TRUE(r.has_value());
  EXPECT_EQ(r.value(), 42);
}

TEST(error_test, result_with_error) {
  result<int> r{std::unexpected{error{EAGAIN}}};
  EXPECT_FALSE(r.has_value());
  EXPECT_TRUE(r.error().is_would_block());
}

TEST(error_test, void_result_success) {
  void_result r{};
  EXPECT_TRUE(r.has_value());
}

TEST(error_test, void_result_error) {
  void_result r{std::unexpected{error{EINTR}}};
  EXPECT_FALSE(r.has_value());
  EXPECT_TRUE(r.error().is_interrupted());
}
