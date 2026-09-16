/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_OUTPUT_CTF_CTFMETADATAMODEL_H
#define CTRACE_SRC_OUTPUT_CTF_CTFMETADATAMODEL_H

#include "CtfGraphicalTopic.h"
#include "CtfUuid.h"
#include "TraceEvent.h"
#include "TraceRoute.h"

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

/** @brief Identifies one CTF stream class independently from a trace route. */
class CtfStreamClassId final {
public:
  /** @brief Creates CTF stream-class ID zero. */
  constexpr CtfStreamClassId() = default;

  /** @brief Creates a CTF stream-class ID from its encoded value. */
  explicit constexpr CtfStreamClassId(std::uint32_t value)
    : m_value(value)
  {
  }

  /** @brief Returns the encoded stream-class ID. */
  constexpr std::uint32_t value() const noexcept
  {
    return m_value;
  }

private:
  std::uint32_t m_value = 0U;
};

/** @brief Compares CTF stream-class IDs. */
constexpr bool operator==(CtfStreamClassId left, CtfStreamClassId right) noexcept
{
  return left.value() == right.value();
}

/** @brief Compares CTF stream-class IDs. */
constexpr bool operator!=(CtfStreamClassId left, CtfStreamClassId right) noexcept
{
  return !(left == right);
}

/** @brief Orders CTF stream-class IDs. */
constexpr bool operator<(CtfStreamClassId left, CtfStreamClassId right) noexcept
{
  return left.value() < right.value();
}

/** @brief Identifies one CTF clock domain independently from frequency. */
class CtfClockDomainId final {
public:
  /** @brief Creates CTF clock-domain ID zero. */
  constexpr CtfClockDomainId() = default;

  /** @brief Creates a CTF clock-domain ID from its bundle-local value. */
  explicit constexpr CtfClockDomainId(std::uint32_t value)
    : m_value(value)
  {
  }

  /** @brief Returns the bundle-local clock-domain ID. */
  constexpr std::uint32_t value() const noexcept
  {
    return m_value;
  }

private:
  std::uint32_t m_value = 0U;
};

/** @brief Compares CTF clock-domain IDs. */
constexpr bool operator==(CtfClockDomainId left, CtfClockDomainId right) noexcept
{
  return left.value() == right.value();
}

/** @brief Compares CTF clock-domain IDs. */
constexpr bool operator!=(CtfClockDomainId left, CtfClockDomainId right) noexcept
{
  return !(left == right);
}

/** @brief Orders CTF clock-domain IDs. */
constexpr bool operator<(CtfClockDomainId left, CtfClockDomainId right) noexcept
{
  return left.value() < right.value();
}

/** @brief Describes one counter/timebase declaration in a CTF bundle. */
struct CtfClockDomainDescriptor {
  CtfClockDomainId id;
  std::string name;
  std::optional<CtfUuid> uuid;
  std::uint64_t frequencyHz = 0U;
  bool absolute = false;
};

/** @brief Describes one normalized route's CTF stream-class identity. */
struct CtfStreamDescriptor {
  CtfStreamClassId streamClassId;
  TraceRouteIdentity route;
  std::optional<std::string> processorName;
  CtfClockDomainId clockDomainId;
};

/** @brief Describes one configured ITM or DWT source on an exact route. */
struct CtfSourceDescriptor {
  std::string type;
  std::uint32_t source = 0U;
  TraceRouteIdentity route;
  std::optional<std::string> label;
  std::optional<std::uint64_t> address;
  std::string dataType = "unsigned";
  std::uint8_t dataSize = 4U;
};

/** @brief Stores the configured stream, clock, and source topology for one CTF bundle. */
struct CtfMetadataTopology {
  std::vector<CtfClockDomainDescriptor> clockDomains;
  std::vector<CtfStreamDescriptor> streams;
  std::vector<CtfSourceDescriptor> sources;
};

/** @brief Owns validated CTF metadata and runtime observations for one bundle. */
class CtfMetadataModel final {
public:
  /** @brief Creates and validates a bundle-local metadata model. */
  CtfMetadataModel(CtfUuid traceUuid, CtfMetadataTopology topology);

  /** @brief Returns the trace UUID shared by metadata and packet headers. */
  const CtfUuid& traceUuid() const noexcept;
  /** @brief Returns the canonical configured topology. */
  const CtfMetadataTopology& topology() const noexcept;
  /** @brief Returns the stream descriptor for one exact normalized route. */
  const CtfStreamDescriptor* streamForRoute(const TraceRouteIdentity& route) const noexcept;
  /** @brief Returns the clock-domain descriptor with one bundle-local ID. */
  const CtfClockDomainDescriptor* clockDomain(CtfClockDomainId id) const noexcept;
  /** @brief Returns exact configured source metadata for one route/type/source key. */
  const CtfSourceDescriptor* source(const TraceRouteIdentity& route, const char* type,
                                    std::uint32_t source) const noexcept;
  /** @brief Records one exception number observed on a concrete stream class. */
  void observeException(CtfStreamClassId streamClassId, ExceptionNumber number);
  /** @brief Returns sorted exception numbers observed on one stream class. */
  std::vector<ExceptionNumber> observedExceptions(CtfStreamClassId streamClassId) const;
  /** @brief Records one graphical topic backed by emitted records on a concrete stream class. */
  void observeGraphicalTopic(CtfStreamClassId streamClassId, CtfGraphicalTopic topic);
  /** @brief Tests whether one graphical topic is backed by emitted records on a stream class. */
  bool observedGraphicalTopic(CtfStreamClassId streamClassId, CtfGraphicalTopic topic) const;
  /** @brief Projects configured topology and observations to streams with completed packet output. */
  CtfMetadataModel projectToEmittedStreams(const std::set<CtfStreamClassId>& streamClassIds) const;
  /** @brief Tests whether this topology uses the exact legacy single-stream CTF layout. */
  bool isLegacySingleStreamLayout() const noexcept;

private:
  /** @brief Runs every topology validation group in deterministic error order. */
  void validate() const;
  /** @brief Validates clock-domain identity and scalar properties. */
  void validateClockDomains() const;
  /** @brief Validates stream identities and their clock-domain references. */
  void validateStreams() const;
  /** @brief Validates source identities, metadata, and ordering. */
  void validateSources() const;
  /** @brief Requires explicit clock UUIDs outside the exact legacy layout. */
  void validateNonLegacyClockDomains() const;

  CtfUuid m_traceUuid;
  CtfMetadataTopology m_topology;
  std::map<CtfStreamClassId, std::set<ExceptionNumber>> m_observedExceptions;
  std::map<CtfStreamClassId, std::set<CtfGraphicalTopic>> m_observedGraphicalTopics;
};

#endif // CTRACE_SRC_OUTPUT_CTF_CTFMETADATAMODEL_H
