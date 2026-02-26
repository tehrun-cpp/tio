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
#include <cstdlib>
#include <print>
#include <unordered_map>
#include <vector>

#include <tio/tio.hpp>

using namespace tio;
using namespace tio::net;

static constexpr auto k_listener_token = token{0};
static constexpr std::size_t k_max_events = 1024;
static constexpr std::size_t k_buf_size = 4096;

struct connection {
  tcp_stream stream;
  std::vector<std::byte> pending_write;
};

int main(int argc, char* argv[]) {
  std::uint16_t port = 9000;
  if (argc > 1) {
    port = static_cast<std::uint16_t>(std::atoi(argv[1]));
  }

  const auto addr = detail::socket_addr::ipv4_any(port);

  auto p = poll::create().value();
  auto listener = tcp_listener::bind(addr).value();
  const auto bound = listener.local_addr().value();
  std::println("echo server listening on {}", bound);

  p.get_registry().register_source(listener, k_listener_token, interest::readable()).value();

  events evs{k_max_events};
  std::unordered_map<std::size_t, connection> connections;
  std::size_t next_token = 1;

  while (true) {
    auto r = p.do_poll(evs, std::nullopt);
    if (!r.has_value()) {
      std::println(stderr, "poll error: {}", r.error());
      return 1;
    }

    for (const auto& ev : evs) {
      if (ev.tok() == k_listener_token) {
        while (true) {
          auto accept_result = listener.accept();
          if (!accept_result.has_value()) {
            if (accept_result.error().is_would_block()) {
              break;
            }
            std::println(stderr, "accept error: {}", accept_result.error());
            break;
          }

          auto& [stream, peer] = accept_result.value();
          const auto tok = token{next_token++};
          std::println("accepted connection from {} as {}", peer, tok);

          p.get_registry()
              .register_source(stream, tok, interest::readable() | interest::writable())
              .value();

          connections.emplace(tok.value(), connection{.stream=std::move(stream), .pending_write={}});
        }
        continue;
      }

      auto it = connections.find(ev.tok().value());
      if (it == connections.end()) {
        continue;
      }

      auto &[stream, pending_write] = it->second;

      if (ev.is_readable()) {
        std::array<std::byte, k_buf_size> buf{};
        while (true) {
          auto read_result = stream.read(buf);
          if (!read_result.has_value()) {
            if (read_result.error().is_would_block()) {
              break;
            }
            std::println("read error on {}: {}", ev.tok(), read_result.error());
            p.get_registry().deregister_source(stream).value();
            connections.erase(it);
            goto next_event;
          }

          const auto n = read_result.value();
          if (n == 0) {
            std::println("connection {} closed", ev.tok());
            p.get_registry().deregister_source(stream).value();
            connections.erase(it);
            goto next_event;
          }

          pending_write.insert(pending_write.end(), buf.begin(), buf.begin() + n);
        }
      }

      if (ev.is_writable() && !pending_write.empty()) {
        while (!pending_write.empty()) {
          auto write_result = stream.write(pending_write);
          if (!write_result.has_value()) {
            if (write_result.error().is_would_block()) {
              break;
            }
            std::println("write error on {}: {}", ev.tok(), write_result.error());
            p.get_registry().deregister_source(stream).value();
            connections.erase(it);
            goto next_event;
          }

          const auto n = write_result.value();
          pending_write.erase(pending_write.begin(), pending_write.begin() + n);
        }
      }

      if (ev.is_error() || ev.is_read_closed()) {
        std::println("connection {} error/closed", ev.tok());
        p.get_registry().deregister_source(stream).value();
        connections.erase(it);
      }

    next_event:;
    }
  }
}
