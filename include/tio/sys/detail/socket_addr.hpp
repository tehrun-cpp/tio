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

#pragma once

#include <cstdint>
#include <cstring>
#include <format>
#include <string>
#include <variant>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

namespace tio::detail {

class socket_addr {
public:
  socket_addr() noexcept : storage_{sockaddr_in{}} {}

  static auto ipv4(std::uint32_t addr, std::uint16_t port) noexcept -> socket_addr {
    sockaddr_in sa{};
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr = htonl(addr);
    return socket_addr{sa};
  }

  static auto ipv4_loopback(std::uint16_t port) noexcept -> socket_addr {
    return ipv4(INADDR_LOOPBACK, port);
  }

  static auto ipv4_any(std::uint16_t port) noexcept -> socket_addr {
    return ipv4(INADDR_ANY, port);
  }

  static auto ipv6_loopback(std::uint16_t port) noexcept -> socket_addr {
    sockaddr_in6 sa{};
    sa.sin6_family = AF_INET6;
    sa.sin6_port = htons(port);
    sa.sin6_addr = IN6ADDR_LOOPBACK_INIT;
    return socket_addr{sa};
  }

  static auto ipv6_any(std::uint16_t port) noexcept -> socket_addr {
    sockaddr_in6 sa{};
    sa.sin6_family = AF_INET6;
    sa.sin6_port = htons(port);
    sa.sin6_addr = in6addr_any;
    return socket_addr{sa};
  }

  static auto from_raw(const sockaddr* sa, socklen_t len) noexcept -> socket_addr {
    if (sa->sa_family == AF_INET && len >= sizeof(sockaddr_in)) {
      sockaddr_in v4{};
      std::memcpy(&v4, sa, sizeof(sockaddr_in));
      return socket_addr{v4};
    }
    if (sa->sa_family == AF_INET6 && len >= sizeof(sockaddr_in6)) {
      sockaddr_in6 v6{};
      std::memcpy(&v6, sa, sizeof(sockaddr_in6));
      return socket_addr{v6};
    }
    return {};
  }

  [[nodiscard]] auto is_ipv4() const noexcept -> bool {
    return std::holds_alternative<sockaddr_in>(storage_);
  }

  [[nodiscard]] auto is_ipv6() const noexcept -> bool {
    return std::holds_alternative<sockaddr_in6>(storage_);
  }

  [[nodiscard]] auto family() const noexcept -> int { return is_ipv4() ? AF_INET : AF_INET6; }

  [[nodiscard]] auto port() const noexcept -> std::uint16_t {
    if (const auto* v4 = std::get_if<sockaddr_in>(&storage_)) {
      return ntohs(v4->sin_port);
    }
    return ntohs(std::get<sockaddr_in6>(storage_).sin6_port);
  }

  [[nodiscard]] auto ipv4_addr() const noexcept -> in_addr {
    if (const auto* v4 = std::get_if<sockaddr_in>(&storage_)) {
      return v4->sin_addr;
    }
    return in_addr{};
  }

  [[nodiscard]] auto ipv6_addr() const noexcept -> in6_addr {
    if (const auto* v6 = std::get_if<sockaddr_in6>(&storage_)) {
      return v6->sin6_addr;
    }
    return in6_addr{};
  }

  [[nodiscard]] auto as_sockaddr() const noexcept -> const sockaddr* {
    if (const auto* v4 = std::get_if<sockaddr_in>(&storage_)) {
      return reinterpret_cast<const sockaddr*>(v4);
    }
    return reinterpret_cast<const sockaddr*>(&std::get<sockaddr_in6>(storage_));
  }

  [[nodiscard]] auto as_sockaddr_mut() noexcept -> sockaddr* {
    if (auto* v4 = std::get_if<sockaddr_in>(&storage_)) {
      return reinterpret_cast<sockaddr*>(v4);
    }
    return reinterpret_cast<sockaddr*>(&std::get<sockaddr_in6>(storage_));
  }

  [[nodiscard]] auto len() const noexcept -> socklen_t {
    return is_ipv4() ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);
  }

  [[nodiscard]] auto to_string() const -> std::string {
    char buf[INET6_ADDRSTRLEN]{};
    if (auto* v4 = std::get_if<sockaddr_in>(&storage_)) {
      ::inet_ntop(AF_INET, &v4->sin_addr, buf, sizeof(buf));
      return std::format("{}:{}", buf, ntohs(v4->sin_port));
    }
    const auto& v6 = std::get<sockaddr_in6>(storage_);
    ::inet_ntop(AF_INET6, &v6.sin6_addr, buf, sizeof(buf));
    return std::format("[{}]:{}", buf, ntohs(v6.sin6_port));
  }

private:
  explicit socket_addr(sockaddr_in v4) noexcept : storage_{v4} {}
  explicit socket_addr(sockaddr_in6 v6) noexcept : storage_{v6} {}

  std::variant<sockaddr_in, sockaddr_in6> storage_;
};

}

template <> struct std::formatter<tio::detail::socket_addr> {
  static constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

  static auto format(const tio::detail::socket_addr& addr, std::format_context& ctx) {
    return std::format_to(ctx.out(), "{}", addr.to_string());
  }
};
