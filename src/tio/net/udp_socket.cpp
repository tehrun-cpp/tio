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

#include <netinet/in.h>
#include <sys/socket.h>

#include <tio/net/udp_socket.hpp>

namespace tio::net {

auto udp_socket::bind(const detail::socket_addr& addr) -> result<udp_socket> {
  const int fd = ::socket(addr.family(), SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (fd < 0) {
    return std::unexpected{error::last_os_error()};
  }

  detail::fd_guard guard{fd};

  if (::bind(fd, addr.as_sockaddr(), addr.len()) < 0) {
    return std::unexpected{error::last_os_error()};
  }

  return udp_socket{std::move(guard)};
}

auto udp_socket::send_to(std::span<const std::byte> buf, const detail::socket_addr& addr) const
    -> result<std::size_t> {
  const ssize_t n =
      ::sendto(fd_.raw_fd(), buf.data(), buf.size(), MSG_NOSIGNAL, addr.as_sockaddr(), addr.len());
  if (n < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return static_cast<std::size_t>(n);
}

auto udp_socket::recv_from(std::span<std::byte> buf) const
    -> result<std::pair<std::size_t, detail::socket_addr>> {
  sockaddr_storage storage{};
  socklen_t len = sizeof(storage);

  const ssize_t n = recvfrom(
    fd_.raw_fd(),
    buf.data(),
    buf.size(),
    0,
    reinterpret_cast<sockaddr*>(&storage),
    &len
  );

  if (n < 0) {
    return std::unexpected{error::last_os_error()};
  }

  const auto sender = detail::socket_addr::from_raw(reinterpret_cast<sockaddr*>(&storage), len);
  return std::pair{static_cast<std::size_t>(n), sender};
}

auto udp_socket::connect(const detail::socket_addr& addr) const -> void_result {
  if (::connect(fd_.raw_fd(), addr.as_sockaddr(), addr.len()) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return {};
}

auto udp_socket::send(const std::span<const std::byte> buf) const -> result<std::size_t> {
  const ssize_t n = ::send(fd_.raw_fd(), buf.data(), buf.size(), MSG_NOSIGNAL);
  if (n < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return static_cast<std::size_t>(n);
}

auto udp_socket::recv(std::span<std::byte> buf) const -> result<std::size_t> {
  const ssize_t n = ::recv(fd_.raw_fd(), buf.data(), buf.size(), 0);
  if (n < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return static_cast<std::size_t>(n);
}

auto udp_socket::peek(std::span<std::byte> buf) const -> result<std::size_t> {
  const ssize_t n = ::recv(fd_.raw_fd(), buf.data(), buf.size(), MSG_PEEK);
  if (n < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return static_cast<std::size_t>(n);
}

auto udp_socket::peek_from(
  std::span<std::byte> buf
) const -> result<std::pair<std::size_t, detail::socket_addr>> {
  sockaddr_storage storage{};
  socklen_t len = sizeof(storage);

  const ssize_t n = recvfrom(
    fd_.raw_fd(),
    buf.data(),
    buf.size(),
    MSG_PEEK,
    reinterpret_cast<sockaddr*>(&storage),
    &len
  );

  if (n < 0) {
    return std::unexpected{error::last_os_error()};
  }

  const auto sender = detail::socket_addr::from_raw(reinterpret_cast<sockaddr*>(&storage), len);
  return std::pair{static_cast<std::size_t>(n), sender};
}

auto udp_socket::local_addr() const -> result<detail::socket_addr> {
  detail::socket_addr addr;
  socklen_t len = addr.len();
  if (::getsockname(fd_.raw_fd(), addr.as_sockaddr_mut(), &len) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return addr;
}

auto udp_socket::set_broadcast(const bool enable) const -> void_result {
  const int val = enable ? 1 : 0;

  if (setsockopt(fd_.raw_fd(), SOL_SOCKET, SO_BROADCAST, &val, sizeof(val)) < 0) {
    return std::unexpected{error::last_os_error()};
  }

  return {};
}

auto udp_socket::broadcast() const -> result<bool> {
  int val = 0;
  socklen_t len = sizeof(val);
  if (getsockopt(fd_.raw_fd(), SOL_SOCKET, SO_BROADCAST, &val, &len) < 0) {
    return std::unexpected{error::last_os_error()};
  }

  return val != 0;
}

auto udp_socket::peer_addr() const -> result<detail::socket_addr> {
  detail::socket_addr addr;
  socklen_t len = addr.len();
  if (::getpeername(fd_.raw_fd(), addr.as_sockaddr_mut(), &len) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return addr;
}

auto udp_socket::set_ttl(const std::uint32_t ttl) const -> void_result {
  const int val = static_cast<int>(ttl);
  if (setsockopt(fd_.raw_fd(), IPPROTO_IP, IP_TTL, &val, sizeof(val)) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return {};
}

auto udp_socket::ttl() const -> result<std::uint32_t> {
  int val = 0;
  socklen_t len = sizeof(val);
  if (getsockopt(fd_.raw_fd(), IPPROTO_IP, IP_TTL, &val, &len) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return static_cast<std::uint32_t>(val);
}

auto udp_socket::only_v6() const -> result<bool> {
  int val = 0;
  socklen_t len = sizeof(val);
  if (getsockopt(fd_.raw_fd(), IPPROTO_IPV6, IPV6_V6ONLY, &val, &len) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return val != 0;
}

auto udp_socket::take_error() const -> result<error> {
  int val = 0;
  socklen_t len = sizeof(val);
  if (getsockopt(fd_.raw_fd(), SOL_SOCKET, SO_ERROR, &val, &len) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return error{val};
}

auto udp_socket::join_multicast_v4(in_addr group, in_addr iface) const -> void_result {
  ip_mreq mreq{};
  mreq.imr_multiaddr = group;
  mreq.imr_interface = iface;
  if (setsockopt(fd_.raw_fd(), IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return {};
}

auto udp_socket::leave_multicast_v4(in_addr group, in_addr iface) const -> void_result {
  ip_mreq mreq{};
  mreq.imr_multiaddr = group;
  mreq.imr_interface = iface;
  if (setsockopt(fd_.raw_fd(), IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return {};
}

auto udp_socket::set_multicast_ttl_v4(std::uint32_t ttl) const -> void_result {
  const int val = static_cast<int>(ttl);
  if (setsockopt(fd_.raw_fd(), IPPROTO_IP, IP_MULTICAST_TTL, &val, sizeof(val)) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return {};
}

auto udp_socket::multicast_ttl_v4() const -> result<std::uint32_t> {
  int val = 0;
  socklen_t len = sizeof(val);
  if (getsockopt(fd_.raw_fd(), IPPROTO_IP, IP_MULTICAST_TTL, &val, &len) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return static_cast<std::uint32_t>(val);
}

auto udp_socket::set_multicast_loop_v4(bool enable) const -> void_result {
  const int val = enable ? 1 : 0;
  if (setsockopt(fd_.raw_fd(), IPPROTO_IP, IP_MULTICAST_LOOP, &val, sizeof(val)) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return {};
}

auto udp_socket::multicast_loop_v4() const -> result<bool> {
  int val = 0;
  socklen_t len = sizeof(val);
  if (getsockopt(fd_.raw_fd(), IPPROTO_IP, IP_MULTICAST_LOOP, &val, &len) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return val != 0;
}

auto udp_socket::join_multicast_v6(in6_addr group, std::uint32_t iface) const -> void_result {
  ipv6_mreq mreq{};
  mreq.ipv6mr_multiaddr = group;
  mreq.ipv6mr_interface = iface;
  if (::setsockopt(fd_.raw_fd(), IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return {};
}

auto udp_socket::leave_multicast_v6(in6_addr group, std::uint32_t iface) const -> void_result {
  ipv6_mreq mreq{};
  mreq.ipv6mr_multiaddr = group;
  mreq.ipv6mr_interface = iface;
  if (setsockopt(fd_.raw_fd(), IPPROTO_IPV6, IPV6_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return {};
}

auto udp_socket::set_multicast_loop_v6(bool enable) const -> void_result {
  const int val = enable ? 1 : 0;
  if (setsockopt(fd_.raw_fd(), IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &val, sizeof(val)) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return {};
}

auto udp_socket::multicast_loop_v6() const -> result<bool> {
  int val = 0;
  socklen_t len = sizeof(val);
  if (getsockopt(fd_.raw_fd(), IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &val, &len) < 0) {
    return std::unexpected{error::last_os_error()};
  }
  return val != 0;
}

}
