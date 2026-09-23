/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CtfMetadataModel.h"

#include "CtfSchema.h"
#include "TraceStreamId.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <stdexcept>
#include <tuple>

/** @brief Tests whether a name is safe for use as a TSDL identifier. */
static bool isTsdlIdentifier(const std::string& name)
{
  if (name.empty() || (std::isalpha(static_cast<unsigned char>(name.front())) == 0 && name.front() != '_')) {
    return false;
  }
  return std::all_of(name.begin() + 1, name.end(), [](char character) {
    const auto value = static_cast<unsigned char>(character);
    return std::isalnum(value) != 0 || character == '_';
  });
}

/** @brief Tests whether two source descriptors carry identical metadata. */
static bool equivalentSource(const CtfSourceDescriptor& left, const CtfSourceDescriptor& right)
{
  return left.type == right.type && left.source == right.source && left.route == right.route &&
         left.label == right.label && left.address == right.address && left.dataType == right.dataType &&
         left.dataSize == right.dataSize;
}

CtfMetadataModel::CtfMetadataModel(CtfUuid traceUuid, CtfMetadataTopology topology)
  : m_traceUuid(std::move(traceUuid)),
    m_topology(std::move(topology))
{
  std::sort(
      m_topology.clockDomains.begin(), m_topology.clockDomains.end(),
      [](const CtfClockDomainDescriptor& left, const CtfClockDomainDescriptor& right) { return left.id < right.id; });
  std::sort(m_topology.streams.begin(), m_topology.streams.end(),
            [](const CtfStreamDescriptor& left, const CtfStreamDescriptor& right) {
              return std::tie(left.streamClassId, left.route.id) < std::tie(right.streamClassId, right.route.id);
            });
  std::sort(m_topology.sources.begin(), m_topology.sources.end(),
            [](const CtfSourceDescriptor& left, const CtfSourceDescriptor& right) {
              return std::tie(left.route.id, left.type, left.source) <
                     std::tie(right.route.id, right.type, right.source);
            });
  validate();
}

const CtfUuid& CtfMetadataModel::traceUuid() const noexcept
{
  return m_traceUuid;
}

const CtfMetadataTopology& CtfMetadataModel::topology() const noexcept
{
  return m_topology;
}

const CtfStreamDescriptor* CtfMetadataModel::streamForRoute(const TraceRouteIdentity& route) const noexcept
{
  const auto found = std::find_if(m_topology.streams.begin(), m_topology.streams.end(),
                                  [&](const CtfStreamDescriptor& stream) { return stream.route == route; });
  return found == m_topology.streams.end() ? nullptr : &*found;
}

const CtfClockDomainDescriptor* CtfMetadataModel::clockDomain(CtfClockDomainId id) const noexcept
{
  const auto found = std::lower_bound(
      m_topology.clockDomains.begin(), m_topology.clockDomains.end(), id,
      [](const CtfClockDomainDescriptor& clock, CtfClockDomainId candidate) { return clock.id < candidate; });
  return found == m_topology.clockDomains.end() || found->id != id ? nullptr : &*found;
}

const CtfSourceDescriptor* CtfMetadataModel::source(const TraceRouteIdentity& route, const char* type,
                                                    std::uint32_t sourceNumber) const noexcept
{
  const auto found =
      std::find_if(m_topology.sources.begin(), m_topology.sources.end(), [&](const CtfSourceDescriptor& candidate) {
        return candidate.route == route && candidate.type == type && candidate.source == sourceNumber;
      });
  return found == m_topology.sources.end() ? nullptr : &*found;
}

void CtfMetadataModel::observeException(CtfStreamClassId streamClassId, ExceptionNumber number)
{
  const auto stream =
      std::find_if(m_topology.streams.begin(), m_topology.streams.end(),
                   [&](const CtfStreamDescriptor& candidate) { return candidate.streamClassId == streamClassId; });
  if (stream == m_topology.streams.end()) {
    throw std::runtime_error("CTF exception observation references an unknown stream class");
  }
  m_observedExceptions[streamClassId].insert(number);
}

std::vector<ExceptionNumber> CtfMetadataModel::observedExceptions(CtfStreamClassId streamClassId) const
{
  const auto found = m_observedExceptions.find(streamClassId);
  return found == m_observedExceptions.end() ? std::vector<ExceptionNumber>{}
                                             : std::vector<ExceptionNumber>{found->second.begin(), found->second.end()};
}

void CtfMetadataModel::observeGraphicalTopic(CtfStreamClassId streamClassId, CtfGraphicalTopic topic)
{
  if (std::none_of(m_topology.streams.begin(), m_topology.streams.end(), [&](const auto& stream) {
        return stream.streamClassId == streamClassId;
      })) {
    throw std::runtime_error("CTF graphical-topic observation references an unknown stream class");
  }
  m_observedGraphicalTopics[streamClassId].insert(topic);
}

bool CtfMetadataModel::observedGraphicalTopic(CtfStreamClassId streamClassId, CtfGraphicalTopic topic) const
{
  const auto found = m_observedGraphicalTopics.find(streamClassId);
  return found != m_observedGraphicalTopics.end() && found->second.find(topic) != found->second.end();
}

CtfMetadataModel CtfMetadataModel::projectToEmittedStreams(const std::set<CtfStreamClassId>& streamClassIds) const
{
  CtfMetadataTopology topology;
  std::set<CtfClockDomainId> clockDomainIds;
  std::set<TraceRouteId> routeIds;
  for (const auto& stream : m_topology.streams) {
    if (streamClassIds.find(stream.streamClassId) != streamClassIds.end()) {
      topology.streams.push_back(stream);
      clockDomainIds.insert(stream.clockDomainId);
      routeIds.insert(stream.route.id);
    }
  }
  for (const auto& clock : m_topology.clockDomains) {
    if (clockDomainIds.find(clock.id) != clockDomainIds.end()) {
      topology.clockDomains.push_back(clock);
    }
  }
  for (const auto& source : m_topology.sources) {
    if (routeIds.find(source.route.id) != routeIds.end()) {
      topology.sources.push_back(source);
    }
  }

  CtfMetadataModel projected(m_traceUuid, std::move(topology));
  for (const auto streamClassId : streamClassIds) {
    if (std::none_of(projected.topology().streams.begin(), projected.topology().streams.end(),
                     [&](const auto& stream) { return stream.streamClassId == streamClassId; })) {
      continue;
    }
    for (const auto number : observedExceptions(streamClassId)) {
      projected.observeException(streamClassId, number);
    }
    const auto topics = m_observedGraphicalTopics.find(streamClassId);
    if (topics != m_observedGraphicalTopics.end()) {
      for (const auto topic : topics->second) {
        projected.observeGraphicalTopic(streamClassId, topic);
      }
    }
  }
  return projected;
}

bool CtfMetadataModel::isLegacySingleStreamLayout() const noexcept
{
  if (m_topology.clockDomains.size() != 1U || m_topology.streams.size() != 1U) {
    return false;
  }
  const auto& clock = m_topology.clockDomains.front();
  const auto& stream = m_topology.streams.front();
  return clock.id == CtfClockDomainId{0U} && clock.name == "swo_clock" && !clock.absolute &&
         stream.streamClassId == CtfStreamClassId{0U} && !stream.route.traceBusId.has_value() &&
         stream.clockDomainId == clock.id;
}

void CtfMetadataModel::validate() const
{
  validateClockDomains();
  validateStreams();
  validateSources();
  validateNonLegacyClockDomains();
}

void CtfMetadataModel::validateClockDomains() const
{
  std::set<CtfClockDomainId> clockIds;
  std::set<std::string> clockNames;
  std::set<CtfUuid> clockUuids;
  for (const auto& clock : m_topology.clockDomains) {
    if (!clockIds.insert(clock.id).second) {
      throw std::invalid_argument("CTF metadata contains a duplicate clock-domain ID");
    }
    if (!isTsdlIdentifier(clock.name) || !clockNames.insert(clock.name).second) {
      throw std::invalid_argument("CTF metadata requires unique valid clock-domain names");
    }
    if (clock.frequencyHz == 0U) {
      throw std::invalid_argument("CTF metadata requires a non-zero clock-domain frequency");
    }
    if (clock.uuid.has_value() && (*clock.uuid == m_traceUuid || !clockUuids.insert(*clock.uuid).second)) {
      throw std::invalid_argument("CTF clock UUIDs must be distinct from the trace and other clock domains");
    }
  }
}

void CtfMetadataModel::validateStreams() const
{
  std::set<CtfStreamClassId> streamIds;
  std::set<CtfClockDomainId> referencedClockIds;
  std::map<TraceRouteId, TraceRouteIdentity> routeIdentities;
  for (const auto& stream : m_topology.streams) {
    const auto [route, inserted] = routeIdentities.emplace(stream.route.id, stream.route);
    if (!inserted) {
      throw std::invalid_argument(route->second == stream.route
                                      ? "CTF metadata contains a duplicate normalized route"
                                      : "CTF metadata contains inconsistent normalized route identities");
    }
    if (!streamIds.insert(stream.streamClassId).second) {
      throw std::invalid_argument("CTF metadata contains a duplicate stream-class ID");
    }
    if (clockDomain(stream.clockDomainId) == nullptr) {
      throw std::invalid_argument("CTF stream class references an unknown clock domain");
    }
    referencedClockIds.insert(stream.clockDomainId);
    if (stream.route.traceBusId.has_value() && !CoreSight::isAtbTraceId(*stream.route.traceBusId)) {
      throw std::invalid_argument("CTF ITM stream route requires a CoreSight ATB trace ID between 1 and 111");
    }
    const auto expectedStreamClassId = CtfStreamClassId{stream.route.traceBusId.value_or(0U)};
    if (stream.streamClassId != expectedStreamClassId) {
      throw std::invalid_argument("CTF stream-class ID does not match its normalized route identity");
    }
  }

  for (const auto& clock : m_topology.clockDomains) {
    if (referencedClockIds.find(clock.id) == referencedClockIds.end()) {
      throw std::invalid_argument("CTF metadata contains a clock domain without a referencing stream class");
    }
  }
}

void CtfMetadataModel::validateSources() const
{
  for (std::size_t index = 0U; index < m_topology.sources.size(); ++index) {
    const auto& source = m_topology.sources[index];
    if (streamForRoute(source.route) == nullptr) {
      throw std::invalid_argument("CTF source metadata references an unknown normalized route");
    }
    if (source.type == "itm") {
      if (source.source == CoreSight::kExcludedItmStimulusPort || !CoreSight::isItmStimulusPort(source.source)) {
        throw std::invalid_argument("CTF ITM source metadata requires a channel between 1 and 31");
      }
    } else if (source.type == "dwt") {
      if (source.source > 3U) {
        throw std::invalid_argument("CTF DWT source metadata requires a comparator between 0 and 3");
      }
      if (CtfSchema::valueVariantForTraceRunType(source.dataType, source.dataSize) == nullptr) {
        throw std::invalid_argument("CTF DWT source metadata has an invalid data-type/size combination");
      }
      const auto extent = static_cast<std::uint64_t>(source.dataSize - 1U);
      if (source.address.has_value() && *source.address > std::numeric_limits<std::uint64_t>::max() - extent) {
        throw std::invalid_argument("CTF DWT source address range exceeds the unsigned 64-bit metadata domain");
      }
    } else {
      throw std::invalid_argument("CTF source metadata type must be 'itm' or 'dwt'");
    }
    if (index > 0U) {
      const auto& previous = m_topology.sources[index - 1U];
      const auto sameKey =
          previous.route.id == source.route.id && previous.type == source.type && previous.source == source.source;
      if (sameKey) {
        throw std::invalid_argument(equivalentSource(previous, source)
                                        ? "CTF metadata contains duplicate source metadata"
                                        : "CTF metadata contains conflicting source metadata for one route");
      }
    }
  }
}

void CtfMetadataModel::validateNonLegacyClockDomains() const
{
  if (!isLegacySingleStreamLayout()) {
    for (const auto& clock : m_topology.clockDomains) {
      if (!clock.uuid.has_value()) {
        throw std::invalid_argument("non-legacy CTF clock domains require an explicit UUID");
      }
    }
  }
}
