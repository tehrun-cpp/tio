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

#include <tio/interest.hpp>

#include <gtest/gtest.h>

using tio::interest;

TEST(interest_test, default_is_empty) {
  interest i;
  EXPECT_TRUE(i.is_empty());
  EXPECT_FALSE(i.is_readable());
  EXPECT_FALSE(i.is_writable());
  EXPECT_FALSE(i.is_priority());
}

TEST(interest_test, readable) {
  auto i = interest::readable();
  EXPECT_TRUE(i.is_readable());
  EXPECT_FALSE(i.is_writable());
  EXPECT_FALSE(i.is_priority());
  EXPECT_FALSE(i.is_empty());
}

TEST(interest_test, writable) {
  auto i = interest::writable();
  EXPECT_FALSE(i.is_readable());
  EXPECT_TRUE(i.is_writable());
  EXPECT_FALSE(i.is_priority());
}

TEST(interest_test, priority) {
  auto i = interest::priority();
  EXPECT_FALSE(i.is_readable());
  EXPECT_FALSE(i.is_writable());
  EXPECT_TRUE(i.is_priority());
}

TEST(interest_test, bitwise_or) {
  auto i = interest::readable() | interest::writable();
  EXPECT_TRUE(i.is_readable());
  EXPECT_TRUE(i.is_writable());
  EXPECT_FALSE(i.is_priority());
}

TEST(interest_test, bitwise_or_all) {
  auto i = interest::readable() | interest::writable() | interest::priority();
  EXPECT_TRUE(i.is_readable());
  EXPECT_TRUE(i.is_writable());
  EXPECT_TRUE(i.is_priority());
}

TEST(interest_test, or_assign) {
  auto i = interest::readable();
  i |= interest::writable();
  EXPECT_TRUE(i.is_readable());
  EXPECT_TRUE(i.is_writable());
}

TEST(interest_test, equality) {
  EXPECT_EQ(interest::readable(), interest::readable());
  EXPECT_NE(interest::readable(), interest::writable());
  EXPECT_EQ(interest::readable() | interest::writable(),
            interest::writable() | interest::readable());
}

TEST(interest_test, raw_value) {
  EXPECT_EQ(interest::readable().raw(), 0b001);
  EXPECT_EQ(interest::writable().raw(), 0b010);
  EXPECT_EQ(interest::priority().raw(), 0b100);
  EXPECT_EQ((interest::readable() | interest::writable()).raw(), 0b011);
}

TEST(interest_test, format_readable) {
  auto s = std::format("{}", interest::readable());
  EXPECT_EQ(s, "interest(READABLE)");
}

TEST(interest_test, format_combined) {
  auto s = std::format("{}", interest::readable() | interest::writable());
  EXPECT_EQ(s, "interest(READABLE|WRITABLE)");
}

TEST(interest_test, format_all) {
  auto s = std::format("{}", interest::readable() | interest::writable() | interest::priority());
  EXPECT_EQ(s, "interest(READABLE|WRITABLE|PRIORITY)");
}

TEST(interest_test, format_empty) {
  auto s = std::format("{}", interest{});
  EXPECT_EQ(s, "interest(NONE)");
}

TEST(interest_test, remove) {
  auto i = interest::readable() | interest::writable();
  auto r = i.remove(interest::writable());
  EXPECT_TRUE(r.is_readable());
  EXPECT_FALSE(r.is_writable());
}

TEST(interest_test, remove_all) {
  auto i = interest::readable() | interest::writable();
  auto r = i.remove(interest::readable() | interest::writable());
  EXPECT_TRUE(r.is_empty());
}

TEST(interest_test, remove_nonexistent) {
  auto i = interest::readable();
  auto r = i.remove(interest::writable());
  EXPECT_TRUE(r.is_readable());
  EXPECT_FALSE(r.is_writable());
}
