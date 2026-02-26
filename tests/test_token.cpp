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
#include <unordered_set>

#include <tio/token.hpp>

#include <gtest/gtest.h>

using tio::token;

TEST(token_test, default_constructed_is_zero) {
  token t;
  EXPECT_EQ(t.value(), 0u);
}

TEST(token_test, explicit_value) {
  token t{42};
  EXPECT_EQ(t.value(), 42u);
}

TEST(token_test, equality) {
  EXPECT_EQ(token{1}, token{1});
  EXPECT_NE(token{1}, token{2});
}

TEST(token_test, ordering) {
  EXPECT_LT(token{1}, token{2});
  EXPECT_GT(token{3}, token{2});
  EXPECT_LE(token{2}, token{2});
  EXPECT_GE(token{2}, token{2});
}

TEST(token_test, hash) {
  std::unordered_set<token> set;
  set.insert(token{1});
  set.insert(token{2});
  set.insert(token{1});
  EXPECT_EQ(set.size(), 2u);
  EXPECT_TRUE(set.contains(token{1}));
  EXPECT_TRUE(set.contains(token{2}));
}

TEST(token_test, format) {
  auto s = std::format("{}", token{7});
  EXPECT_EQ(s, "token(7)");
}

TEST(token_test, max_value) {
  token t{std::size_t(-1)};
  EXPECT_EQ(t.value(), std::size_t(-1));
}

TEST(token_test, copy) {
  token a{5};
  token b = a;
  EXPECT_EQ(a, b);
}
