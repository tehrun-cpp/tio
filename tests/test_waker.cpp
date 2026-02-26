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

#include <atomic>
#include <thread>

#include <tio/waker.hpp>

#include <gtest/gtest.h>

using tio::events;
using tio::poll;
using tio::token;
using tio::waker;

static constexpr auto k_waker_token = token{0xFFFF};

TEST(waker_test, create_succeeds) {
  auto p = poll::create().value();
  auto w = waker::create(p.get_registry(), k_waker_token);
  ASSERT_TRUE(w.has_value());
}

TEST(waker_test, wake_before_poll) {
  auto p = poll::create().value();
  auto w = waker::create(p.get_registry(), k_waker_token).value();

  w.wake().value();

  events evs{64};
  auto r = p.do_poll(evs, std::chrono::milliseconds{100});
  ASSERT_TRUE(r.has_value());
  EXPECT_GE(evs.size(), 1u);

  bool found = false;
  for (const auto& ev : evs) {
    if (ev.tok() == k_waker_token && ev.is_readable()) {
      found = true;
    }
  }
  EXPECT_TRUE(found);

  w.drain();
}

TEST(waker_test, wake_from_another_thread) {
  auto p = poll::create().value();
  auto w = waker::create(p.get_registry(), k_waker_token).value();

  std::atomic<bool> poll_started{false};

  std::thread waker_thread([&w, &poll_started] {
    while (!poll_started.load(std::memory_order_acquire)) {
      std::this_thread::yield();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds{20});
    w.wake().value();
  });

  events evs{64};
  poll_started.store(true, std::memory_order_release);

  auto r = p.do_poll(evs, std::chrono::milliseconds{2000});
  ASSERT_TRUE(r.has_value());
  EXPECT_GE(evs.size(), 1u);

  bool found = false;
  for (const auto& ev : evs) {
    if (ev.tok() == k_waker_token && ev.is_readable()) {
      found = true;
    }
  }
  EXPECT_TRUE(found);

  w.drain();
  waker_thread.join();
}

TEST(waker_test, multiple_wakes_coalesce) {
  auto p = poll::create().value();
  auto w = waker::create(p.get_registry(), k_waker_token).value();

  w.wake().value();
  w.wake().value();
  w.wake().value();

  events evs{64};
  auto r = p.do_poll(evs, std::chrono::milliseconds{100});
  ASSERT_TRUE(r.has_value());

  int wake_count = 0;
  for (const auto& ev : evs) {
    if (ev.tok() == k_waker_token) {
      ++wake_count;
    }
  }
  EXPECT_EQ(wake_count, 1);

  w.drain();
}

TEST(waker_test, drain_then_no_event) {
  auto p = poll::create().value();
  auto w = waker::create(p.get_registry(), k_waker_token).value();

  w.wake().value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{100}).value();
  EXPECT_GE(evs.size(), 1u);

  w.drain();

  p.do_poll(evs, std::chrono::milliseconds{50}).value();
  EXPECT_EQ(evs.size(), 0u);
}

TEST(waker_test, waker_is_copyable) {
  auto p = poll::create().value();
  auto w1 = waker::create(p.get_registry(), k_waker_token).value();

  auto w2 = w1;
  w2.wake().value();

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{100}).value();
  EXPECT_GE(evs.size(), 1u);

  w1.drain();
}

TEST(waker_test, wake_from_multiple_threads) {
  auto p = poll::create().value();
  auto w = waker::create(p.get_registry(), k_waker_token).value();

  constexpr int n_threads = 4;
  std::vector<std::thread> threads;
  threads.reserve(n_threads);

  for (int i = 0; i < n_threads; ++i) {
    threads.emplace_back([&w] { w.wake().value(); });
  }

  for (auto& t : threads) {
    t.join();
  }

  events evs{64};
  p.do_poll(evs, std::chrono::milliseconds{100}).value();

  bool found = false;
  for (const auto& ev : evs) {
    if (ev.tok() == k_waker_token) {
      found = true;
    }
  }
  EXPECT_TRUE(found);

  w.drain();
}
