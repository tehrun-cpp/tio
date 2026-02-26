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

#include <algorithm>
#include <cstring>
#include <string>
#include <string_view>

#include <sys/socket.h>
#include <sys/un.h>

namespace tio::detail {

class unix_addr {
public:
  unix_addr() noexcept : storage_{}, len_{sizeof(sa_family_t)} {
    storage_.sun_family = AF_UNIX;
  }

  static auto from_pathname(std::string_view path) noexcept -> unix_addr {
    unix_addr addr;
    addr.storage_.sun_family = AF_UNIX;
    const auto max_path = sizeof(addr.storage_.sun_path) - 1;
    const auto copy_len = std::min(path.size(), max_path);
    std::memcpy(addr.storage_.sun_path, path.data(), copy_len);
    addr.storage_.sun_path[copy_len] = '\0';
    addr.len_ = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + copy_len + 1);
    return addr;
  }

  static auto from_raw(const sockaddr* sa, socklen_t len) noexcept -> unix_addr {
    unix_addr addr;
    std::memcpy(&addr.storage_, sa, std::min(static_cast<size_t>(len), sizeof(sockaddr_un)));
    addr.len_ = len;
    return addr;
  }

  [[nodiscard]] auto as_sockaddr() const noexcept -> const sockaddr* {
    return reinterpret_cast<const sockaddr*>(&storage_);
  }

  [[nodiscard]] auto as_sockaddr_mut() noexcept -> sockaddr* {
    return reinterpret_cast<sockaddr*>(&storage_);
  }

  [[nodiscard]] auto len() const noexcept -> socklen_t { return len_; }

  [[nodiscard]] auto len_mut() noexcept -> socklen_t* { return &len_; }

  [[nodiscard]] auto family() const noexcept -> int { return AF_UNIX; }

  [[nodiscard]] auto is_unnamed() const noexcept -> bool {
    return len_ <= static_cast<socklen_t>(offsetof(sockaddr_un, sun_path));
  }

  [[nodiscard]] auto as_pathname() const noexcept -> std::string_view {
    if (is_unnamed()) {
      return {};
    }
    const auto path_len = len_ - static_cast<socklen_t>(offsetof(sockaddr_un, sun_path));
    if (path_len > 0 && storage_.sun_path[path_len - 1] == '\0') {
      return {storage_.sun_path, path_len - 1};
    }
    return {storage_.sun_path, path_len};
  }

  [[nodiscard]] auto to_string() const -> std::string {
    if (is_unnamed()) {
      return "(unnamed)";
    }
    return std::string{as_pathname()};
  }

private:
  sockaddr_un storage_;
  socklen_t len_;
};

}
