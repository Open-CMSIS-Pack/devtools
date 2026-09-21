/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "DwtPacketDecoder.h"

#include "TraceEvent.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

/** @brief Identifies the hardware source encoded by a DWT packet discriminator. */
enum class DwtPacketSource : std::uint8_t {
  EventCounter = 0U,
  ExceptionTrace = 1U,
  PeriodicPcSample = 2U,
  PmuTraceOnOverflow = 3U,
};

/** @brief Identifies address and value variants of DWT data-trace packets. */
enum class DwtDataPacketType : std::uint8_t {
  Address = 1U,
  Value = 2U,
};

/** @brief Maps encoded DWT exception actions to their wire values. */
enum class DwtExceptionActionCode : std::uint32_t {
  Entered = 1U,
  Exited = 2U,
  Returned = 3U,
};

// DWT hardware source IDs 8..23 encode packet type, comparator, and subtype.
constexpr std::uint8_t kFirstDataTraceSource = 8U;
constexpr std::uint8_t kLastDataTraceSource = 23U;
constexpr std::uint8_t kDataTraceSubtypeMask = 0x1U;
constexpr std::uint8_t kDataTraceComparatorMask = 0x3U;
constexpr std::uint8_t kDataTraceComparatorShift = 1U;
constexpr std::uint8_t kDataTracePacketTypeMask = 0x3U;
constexpr std::uint8_t kDataTracePacketTypeShift = 3U;

constexpr std::uint32_t kExceptionNumberMask = 0x1ffU;
constexpr std::uint32_t kExceptionActionMask = 0x3U;
constexpr std::uint32_t kExceptionActionShift = 12U;
constexpr std::uint32_t kPmuOverflowMask = 0xffU;

constexpr std::uint8_t kArmv8MMatchBytes = 1U;
constexpr std::uint32_t kArmv8MMatchValue = 1U;

/** @brief Returns whether a raw DWT address fragment width can be preserved. */
static bool isSupportedAddressFragmentSize(std::uint8_t size)
{
  return size == 1U || size == 2U || size == 4U;
}

/** @brief Classifies PC-sample payloads without confusing a four-byte PC with a one-byte marker. */
static std::optional<PcSampleKind> pcSampleKind(const DwtPayloadPacket& payload)
{
  if (payload.size == 4U) {
    return PcSampleKind::Pc;
  }
  if (payload.size == 1U) {
    if (payload.value == 0U) {
      return PcSampleKind::Sleep;
    }
    if (payload.value == 0xffU) {
      return PcSampleKind::TraceProhibited;
    }
  }
  return std::nullopt;
}

/** @brief Describes an invalid DWT event-counter payload. */
static std::string invalidEventCounterMessage(const DwtPayloadPacket& payload)
{
  std::ostringstream message;
  message << "unsupported DWT event-counter payload: size " << static_cast<unsigned>(payload.size) << ", value 0x"
          << std::hex << payload.value << "; expected a non-zero 1-byte mask using bits 0..5 only";
  return message.str();
}

/** @brief Describes an invalid PMU trace-on-overflow payload. */
static std::string invalidPmuEventCounterMessage(const DwtPayloadPacket& payload)
{
  std::ostringstream message;
  message << "unsupported PMU event-counter payload: size " << static_cast<unsigned>(payload.size) << ", value 0x"
          << std::hex << payload.value << "; expected a non-zero 1-byte mask using bits 0..7";
  return message.str();
}

/** @brief Wraps a DWT payload with its decoded-event metadata. */
static TraceEvent makeDwtEvent(std::uint64_t index, const TraceRouteIdentity& route, std::uint64_t tcyc,
                               const TraceQuality& quality, TraceEventPayload payload)
{
  TraceEvent event{std::move(payload)};
  event.index = index;
  event.route = route;
  event.tcyc = tcyc;
  event.quality = quality;
  return event;
}

/** @brief Wraps a decoded event with metadata from its source DWT packet. */
static TraceEvent makeDwtEvent(const DwtPayloadPacket& packet, TraceEventPayload payload)
{
  return makeDwtEvent(packet.index, packet.route, packet.tcyc, packet.quality, std::move(payload));
}

std::vector<TraceEvent> DwtPacketDecoder::decode(const DwtPayloadPacket& payload)
{
  switch (static_cast<DwtPacketSource>(payload.discriminator)) {
  case DwtPacketSource::EventCounter:
    return decodeEventCounter(payload);
  case DwtPacketSource::ExceptionTrace:
    return decodeExceptionTrace(payload);
  case DwtPacketSource::PeriodicPcSample:
    return decodePeriodicPcSample(payload);
  case DwtPacketSource::PmuTraceOnOverflow:
    return decodePmuTraceOnOverflow(payload);
  }

  std::vector<TraceEvent> output;
  if (payload.discriminator >= kFirstDataTraceSource && payload.discriminator <= kLastDataTraceSource) {
    decodeDataTrace(payload, output);
    return output;
  }

  output = flush(payload.quality, payload.tcyc);
  return output;
}

std::vector<TraceEvent> DwtPacketDecoder::decodeEventCounter(const DwtPayloadPacket& payload)
{
  auto output = flush(payload.quality, payload.tcyc);
  const auto validPayload = payload.size == 1U && payload.value != 0U &&
                            (payload.value & ~static_cast<std::uint32_t>(kDwtEventCounterValidMask)) == 0U;
  if (!validPayload) {
    output.push_back(makeDwtEvent(payload, TraceIssueEvent{
        TraceIssueCode::UnsupportedDwtEventCounterPayload,
        TraceIssueSeverity::Error,
        invalidEventCounterMessage(payload),
        std::nullopt,
        std::nullopt,
    }));
    return output;
  }
  output.push_back(makeDwtEvent(payload, DwtEventTraceEvent{static_cast<std::uint8_t>(payload.value)}));
  return output;
}

std::vector<TraceEvent> DwtPacketDecoder::decodePmuTraceOnOverflow(const DwtPayloadPacket& payload)
{
  auto output = flush(payload.quality, payload.tcyc);
  const auto validPayload = payload.size == 1U && payload.value != 0U && (payload.value & ~kPmuOverflowMask) == 0U;
  if (!validPayload) {
    output.push_back(makeDwtEvent(payload, TraceIssueEvent{
        TraceIssueCode::UnsupportedPmuEventCounterPayload,
        TraceIssueSeverity::Error,
        invalidPmuEventCounterMessage(payload),
        std::nullopt,
        std::nullopt,
    }));
    return output;
  }
  output.push_back(makeDwtEvent(payload, PmuTraceEvent{static_cast<std::uint8_t>(payload.value)}));
  return output;
}

std::vector<TraceEvent> DwtPacketDecoder::decodeExceptionTrace(const DwtPayloadPacket& payload)
{
  auto output = flush(payload.quality, payload.tcyc);
  const auto exceptionNumber = static_cast<ExceptionNumber>(payload.value & kExceptionNumberMask);
  const auto action = exceptionAction((payload.value >> kExceptionActionShift) & kExceptionActionMask);
  if (action == ExceptionAction::Unknown) {
    output.push_back(makeDwtEvent(payload, TraceIssueEvent{
        TraceIssueCode::InvalidExceptionAction,
        TraceIssueSeverity::Error,
        "invalid exception action 0x0 for exception " + std::to_string(exceptionNumber),
        std::nullopt,
        std::nullopt,
    }));
    return output;
  }
  output.push_back(makeDwtEvent(payload, ExceptionTraceEvent{exceptionNumber, action}));
  return output;
}

std::vector<TraceEvent> DwtPacketDecoder::decodePeriodicPcSample(const DwtPayloadPacket& payload)
{
  auto output = flush(payload.quality, payload.tcyc);
  const auto kind = pcSampleKind(payload);
  if (!kind.has_value()) {
    output.push_back(makeDwtEvent(payload, TraceIssueEvent{
        TraceIssueCode::UnsupportedDwtPcSamplePayload,
        TraceIssueSeverity::Error,
        "unsupported DWT PC-sample payload: size " + std::to_string(payload.size) +
            ", value " + std::to_string(payload.value) +
            "; expected a 4-byte PC or a 1-byte marker (0x00: CPU Sleeping, 0xff: Trace prohibited)",
        std::nullopt,
        std::nullopt,
    }));
    return output;
  }
  const auto pc = kind.value() == PcSampleKind::Pc ? payload.value : 0U;
  output.push_back(makeDwtEvent(payload, PcSampleTraceEvent{pc, kind.value()}));
  return output;
}

std::vector<TraceEvent> DwtPacketDecoder::flush(const TraceQuality& quality, std::uint64_t tcyc)
{
  std::vector<TraceEvent> output;
  std::vector<std::uint32_t> comparators;
  for (std::uint32_t comparator = 0; comparator < m_pendingDataTrace.size(); ++comparator) {
    if (m_pendingDataTrace[comparator].has_value()) {
      comparators.push_back(comparator);
    }
  }
  std::sort(comparators.begin(), comparators.end(), [this](const auto left, const auto right) {
    const auto leftIndex = m_pendingDataTrace[left]->index;
    const auto rightIndex = m_pendingDataTrace[right]->index;
    return leftIndex == rightIndex ? left < right : leftIndex < rightIndex;
  });
  for (const auto comparator : comparators) {
    flushPending(comparator, quality, tcyc, output);
  }
  return output;
}

void DwtPacketDecoder::reset()
{
  for (auto& pending : m_pendingDataTrace) {
    pending.reset();
  }
}

void DwtPacketDecoder::decodeDataTrace(const DwtPayloadPacket& payload, std::vector<TraceEvent>& output)
{
  const auto discriminator = payload.discriminator;
  const auto comparator =
      static_cast<std::uint32_t>((discriminator >> kDataTraceComparatorShift) & kDataTraceComparatorMask);
  const auto packetType =
      static_cast<DwtDataPacketType>((discriminator >> kDataTracePacketTypeShift) & kDataTracePacketTypeMask);
  const auto secondarySubtype = (discriminator & kDataTraceSubtypeMask) != 0U;

  PendingDataTrace event;
  event.index = payload.index;
  event.route = payload.route;
  event.quality = payload.quality;

  if (packetType == DwtDataPacketType::Address) {
    decodeDataAddressTrace(payload, comparator, secondarySubtype, std::move(event), output);
    return;
  }
  decodeDataValueTrace(payload, comparator, secondarySubtype, std::move(event), output);
}

void DwtPacketDecoder::decodeDataAddressTrace(const DwtPayloadPacket& payload, std::uint32_t comparator,
                                              bool secondarySubtype, PendingDataTrace event,
                                              std::vector<TraceEvent>& output)
{
  const auto isMatch = !secondarySubtype && payload.size == kArmv8MMatchBytes && payload.value == kArmv8MMatchValue;
  if (isMatch) {
    auto& pending = m_pendingDataTrace[comparator];
    if (pending.has_value()) {
      flushPending(comparator, qualityForPendingFlush(*pending, payload.quality), payload.tcyc, output);
    }
    output.push_back(makeDwtEvent(payload, DwtMatchTraceEvent{comparator}));
    return;
  }
  if (!isSupportedAddressFragmentSize(payload.size)) {
    auto flushed = flush(payload.quality, payload.tcyc);
    output.insert(output.end(), std::make_move_iterator(flushed.begin()), std::make_move_iterator(flushed.end()));
    output.push_back(makeDwtEvent(payload, TraceIssueEvent{
        TraceIssueCode::UnsupportedDwtAddressPayload,
        TraceIssueSeverity::Error,
        "unsupported DWT " + std::string(secondarySubtype ? "data address" : "PC or match") +
            " payload size " + std::to_string(payload.size) + "; expected 1, 2, or 4 bytes",
        std::nullopt,
        std::nullopt,
    }));
    return;
  }
  const DwtAddressFragment fragment{payload.size, payload.value};
  if (secondarySubtype) {
    event.address = fragment;
    event.hasAddress = true;
  } else {
    event.pc = fragment;
    event.hasPc = true;
  }
  sendDataTraceEvent(comparator, event, payload.quality, payload.tcyc, output);
}

void DwtPacketDecoder::decodeDataValueTrace(const DwtPayloadPacket& payload, std::uint32_t comparator,
                                            bool secondarySubtype, PendingDataTrace event,
                                            std::vector<TraceEvent>& output)
{
  event.value = payload.value;
  event.size = payload.size;
  event.isRead = !secondarySubtype;
  event.hasValue = true;
  sendDataTraceEvent(comparator, event, payload.quality, payload.tcyc, output);
}

void DwtPacketDecoder::sendDataTraceEvent(std::uint32_t comparator, const PendingDataTrace& event,
                                          const TraceQuality& quality, std::uint64_t tcyc,
                                          std::vector<TraceEvent>& output)
{
  auto& pending = m_pendingDataTrace[comparator];
  if (!pending.has_value()) {
    pending = event;
    return;
  }

  // The individual short-circuit permutations are an implementation detail;
  // repeated and complementary fragments are covered as complete behaviors.
  const auto repeatsFragmentKind = (pending->hasPc && event.hasPc) ||
                                   (pending->hasAddress && event.hasAddress) ||
                                   (pending->hasValue && event.hasValue);
  if (!repeatsFragmentKind) {
    pending->index = event.index;
    pending->route = event.route;
    pending->pc = event.hasPc ? event.pc : pending->pc;
    pending->address = event.hasAddress ? event.address : pending->address;
    pending->value = event.hasValue ? event.value : pending->value;
    pending->size = event.hasValue ? event.size : pending->size;
    pending->isRead = event.hasValue ? event.isRead : pending->isRead;
    pending->hasPc = pending->hasPc || event.hasPc;
    pending->hasAddress = pending->hasAddress || event.hasAddress;
    pending->hasValue = pending->hasValue || event.hasValue;
    pending->quality.overflow = pending->quality.overflow || event.quality.overflow;
    pending->quality.timestampReliable = pending->quality.timestampReliable && event.quality.timestampReliable;
    pending->quality.overflowCount = std::max(pending->quality.overflowCount, event.quality.overflowCount);
    if (pending->hasValue) {
      flushPending(comparator, qualityForPendingFlush(*pending, quality), tcyc, output);
    }
    return;
  }

  flushPending(comparator, qualityForPendingFlush(*pending, quality), tcyc, output);
  pending = event;
}

void DwtPacketDecoder::flushPending(std::uint32_t comparator, const TraceQuality& quality, std::uint64_t tcyc,
                                    std::vector<TraceEvent>& output)
{
  auto& pending = m_pendingDataTrace[comparator];

  const auto makePayload = [&]() -> TraceEventPayload {
    if (pending->hasValue) {
      return DwtDataTraceEvent{
          comparator,
          pending->size,
          pending->value,
          pending->isRead ? AccessType::Read : AccessType::Write,
          pending->hasAddress ? std::optional<DwtAddressFragment>(pending->address) : std::nullopt,
          pending->hasPc ? std::optional<DwtAddressFragment>(pending->pc) : std::nullopt,
      };
    }

    DwtAddressTraceLocation location = DwtDataAddressTraceLocation{pending->address};
    if (pending->hasPc && pending->hasAddress) {
      location = DwtPcAndDataAddressTraceLocation{pending->pc, pending->address};
    } else if (pending->hasPc) {
      location = DwtPcTraceLocation{pending->pc};
    }
    return DwtAddressTraceEvent{comparator, location};
  };

  output.push_back(makeDwtEvent(pending->index, pending->route, tcyc, quality, makePayload()));
  pending.reset();
}

TraceQuality DwtPacketDecoder::qualityForPendingFlush(const PendingDataTrace& pending,
                                                      const TraceQuality& current) const
{
  TraceQuality quality = current;
  quality.overflow = quality.overflow || pending.quality.overflow;
  quality.timestampReliable =
      quality.timestampReliable && !pending.quality.overflow && pending.quality.timestampReliable;
  quality.overflowCount = std::max(quality.overflowCount, pending.quality.overflowCount);
  return quality;
}

ExceptionAction DwtPacketDecoder::exceptionAction(std::uint32_t value)
{
  switch (static_cast<DwtExceptionActionCode>(value)) {
  case DwtExceptionActionCode::Entered:
    return ExceptionAction::Entered;
  case DwtExceptionActionCode::Exited:
    return ExceptionAction::Exited;
  case DwtExceptionActionCode::Returned:
    return ExceptionAction::Returned;
  default:
    return ExceptionAction::Unknown;
  }
}
