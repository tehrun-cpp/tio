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

#include <concepts>

#include <tio/error.hpp>
#include <tio/interest.hpp>
#include <tio/token.hpp>

namespace tio {

class registry;

template <typename t>
concept source = requires(t &s, const registry &r, token tok, interest intr) {
  { s.tio_register(r, tok, intr) } -> std::same_as<void_result>;
  { s.tio_reregister(r, tok, intr) } -> std::same_as<void_result>;
  { s.tio_deregister(r) } -> std::same_as<void_result>;
};
} // namespace tio
