/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_MODEL_TRACEROUTE_H
#define CTRACE_SRC_MODEL_TRACEROUTE_H

#include <cstdint>
#include <optional>

/** @brief Opaque identity of one normalized route within a trace-run catalogue. */
class TraceRouteId final {
public:
  /** @brief Creates the first deterministic catalogue identity. */
  constexpr TraceRouteId() = default;

  /** @brief Creates an opaque route identity from its deterministic catalogue ordinal. */
  explicit constexpr TraceRouteId(std::uint32_t value)
    : m_value(value)
  {
  }

  /** @brief Returns the internal catalogue ordinal. */
  constexpr std::uint32_t value() const noexcept
  {
    return m_value;
  }

private:
  std::uint32_t m_value = 0U;
};

/** @brief Compares normalized route identities. */
constexpr bool operator==(TraceRouteId left, TraceRouteId right) noexcept
{
  return left.value() == right.value();
}

/** @brief Compares normalized route identities. */
constexpr bool operator!=(TraceRouteId left, TraceRouteId right) noexcept
{
  return !(left == right);
}

/** @brief Orders normalized route identities for associative containers. */
constexpr bool operator<(TraceRouteId left, TraceRouteId right) noexcept
{
  return left.value() < right.value();
}

/** @brief Keeps internal route identity separate from an optional architectural ATB ID. */
struct TraceRouteIdentity {
  TraceRouteId id;
  std::optional<std::uint8_t> traceBusId;
};

/** @brief Compares a complete normalized route identity. */
constexpr bool operator==(const TraceRouteIdentity& left, const TraceRouteIdentity& right) noexcept
{
  return left.id == right.id && left.traceBusId == right.traceBusId;
}

/** @brief Compares a complete normalized route identity. */
constexpr bool operator!=(const TraceRouteIdentity& left, const TraceRouteIdentity& right) noexcept
{
  return !(left == right);
}

#endif // CTRACE_SRC_MODEL_TRACEROUTE_H
