/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "DwtPacketDecoder.hpp"

#include "TraceEvent.hpp"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

enum class DwtPacketSource : std::uint8_t {
  EventCounter = 0U,
  ExceptionTrace = 1U,
  PeriodicPcSample = 2U,
  PmuOverflow = 3U,
};

enum class DwtDataPacketType : std::uint8_t {
  Address = 1U,
  Value = 2U,
};

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

constexpr std::uint8_t kArmv7MFullPcBytes = 4U;
constexpr std::uint8_t kArmv7MAddressOffsetBytes = 2U;

} // namespace

void applyDwtTraceStatus(TraceEvent& event, const DwtTraceStatus& status)
{
  event.quality = status;
}

std::vector<TraceEvent> DwtPacketDecoder::decode(const DwtPayloadPacket& payload)
{
  std::vector<TraceEvent> output;
  const auto discriminator = payload.discriminator;

  const auto source = static_cast<DwtPacketSource>(discriminator);
  if (source == DwtPacketSource::EventCounter || source == DwtPacketSource::PmuOverflow) {
    output = flush(payload.status, payload.tcyc);
    TraceEvent packet = source == DwtPacketSource::EventCounter
                            ? TraceEvent(DwtEventTraceEvent{discriminator, payload.size, payload.value})
                            : TraceEvent(PmuTraceEvent{discriminator, payload.size, payload.value});
    packet.index = payload.index;
    packet.traceBusId = payload.traceBusId;
    packet.tcyc = payload.tcyc;
    applyDwtTraceStatus(packet, payload.status);
    output.push_back(std::move(packet));
    return output;
  }

  if (source == DwtPacketSource::ExceptionTrace) {
    output = flush(payload.status, payload.tcyc);
    const auto exceptionNumber = payload.value & kExceptionNumberMask;
    const auto action = exceptionAction((payload.value >> kExceptionActionShift) & kExceptionActionMask);
    if (action == ExceptionAction::Unknown) {
      TraceEvent error{TraceIssueEvent{
          "invalid-exception-action",
          TraceIssueSeverity::Error,
          "invalid exception action 0x0 for exception " + std::to_string(exceptionNumber),
          std::nullopt,
          std::nullopt,
      }};
      error.index = payload.index;
      error.traceBusId = payload.traceBusId;
      error.tcyc = payload.tcyc;
      applyDwtTraceStatus(error, payload.status);
      output.push_back(std::move(error));
      return output;
    }
    TraceEvent packet{ExceptionTraceEvent{exceptionNumber, action}};
    packet.index = payload.index;
    packet.traceBusId = payload.traceBusId;
    packet.tcyc = payload.tcyc;
    applyDwtTraceStatus(packet, payload.status);
    output.push_back(std::move(packet));
    return output;
  }

  if (source == DwtPacketSource::PeriodicPcSample) {
    // PC samples need a dedicated output event. Until that event is
    // defined, flush preceding data trace but do not expose the sample as
    // an address event.
    output = flush(payload.status, payload.tcyc);
    return output;
  }

  if (discriminator >= kFirstDataTraceSource && discriminator <= kLastDataTraceSource) {
    decodeDataTrace(payload, output);
    return output;
  }

  output = flush(payload.status, payload.tcyc);
  return output;
}

std::vector<TraceEvent> DwtPacketDecoder::flush(const DwtTraceStatus& status, std::uint64_t tcyc)
{
  std::vector<TraceEvent> output;
  std::vector<std::uint32_t> comparators;
  for (std::uint32_t comparator = 0; comparator < pendingDataTrace_.size(); ++comparator) {
    if (pendingDataTrace_[comparator].has_value()) {
      comparators.push_back(comparator);
    }
  }
  std::sort(comparators.begin(), comparators.end(), [this](const auto left, const auto right) {
    const auto leftIndex = pendingDataTrace_[left]->index;
    const auto rightIndex = pendingDataTrace_[right]->index;
    return leftIndex == rightIndex ? left < right : leftIndex < rightIndex;
  });
  for (const auto comparator : comparators) {
    flushPending(comparator, status, tcyc, output);
  }
  return output;
}

void DwtPacketDecoder::reset()
{
  for (auto& pending : pendingDataTrace_) {
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
  event.traceBusId = payload.traceBusId;
  event.status = payload.status;

  if (packetType == DwtDataPacketType::Address) {
    const auto expectedSize = secondarySubtype ? kArmv7MAddressOffsetBytes : kArmv7MFullPcBytes;
    if (payload.size != expectedSize) {
      auto flushed = flush(payload.status, payload.tcyc);
      output.insert(output.end(), std::make_move_iterator(flushed.begin()), std::make_move_iterator(flushed.end()));
      TraceEvent error{TraceIssueEvent{
          "unsupported-dwt-address-payload",
          TraceIssueSeverity::Error,
          "unsupported DWT " + std::string(secondarySubtype ? "address" : "PC or match") + " payload size " +
              std::to_string(payload.size) +
              "; the current ctrace-run format does not provide the "
              "architecture and reconstruction data needed to decode it safely",
          std::nullopt,
          std::nullopt,
      }};
      error.index = payload.index;
      error.traceBusId = payload.traceBusId;
      error.tcyc = payload.tcyc;
      applyDwtTraceStatus(error, payload.status);
      output.push_back(std::move(error));
      return;
    }
    if (secondarySubtype) {
      event.addressLo16 = payload.value;
      event.hasAddressLo16 = true;
    } else {
      event.pc = payload.value;
      event.hasPc = true;
    }
    sendDataTraceEvent(comparator, event, payload.status, payload.tcyc, output);
    return;
  }
  if (packetType == DwtDataPacketType::Value) {
    event.value = payload.value;
    event.size = payload.size;
    event.isRead = !secondarySubtype;
    event.hasValue = true;
    sendDataTraceEvent(comparator, event, payload.status, payload.tcyc, output);
    return;
  }

  auto flushed = flush(payload.status, payload.tcyc);
  output.insert(output.end(), std::make_move_iterator(flushed.begin()), std::make_move_iterator(flushed.end()));
}

void DwtPacketDecoder::sendDataTraceEvent(std::uint32_t comparator, const PendingDataTrace& event,
                                          const DwtTraceStatus& status, std::uint64_t tcyc,
                                          std::vector<TraceEvent>& output)
{
  auto& pending = pendingDataTrace_[comparator];
  if (!pending.has_value()) {
    pending = event;
    return;
  }

  const auto repeatsFragmentKind = (pending->hasPc && event.hasPc) ||
                                   (pending->hasAddressLo16 && event.hasAddressLo16) ||
                                   (pending->hasValue && event.hasValue);
  if (!repeatsFragmentKind) {
    pending->index = event.index;
    pending->traceBusId = event.traceBusId;
    pending->pc = event.hasPc ? event.pc : pending->pc;
    pending->addressLo16 = event.hasAddressLo16 ? event.addressLo16 : pending->addressLo16;
    pending->value = event.hasValue ? event.value : pending->value;
    pending->size = event.hasValue ? event.size : pending->size;
    pending->isRead = event.hasValue ? event.isRead : pending->isRead;
    pending->hasPc = pending->hasPc || event.hasPc;
    pending->hasAddressLo16 = pending->hasAddressLo16 || event.hasAddressLo16;
    pending->hasValue = pending->hasValue || event.hasValue;
    pending->status.overflow = pending->status.overflow || event.status.overflow;
    pending->status.timestampReliable = pending->status.timestampReliable && event.status.timestampReliable;
    pending->status.overflowCount = std::max(pending->status.overflowCount, event.status.overflowCount);
    if (pending->hasValue) {
      flushPending(comparator, statusForPendingFlush(*pending, status), tcyc, output);
    }
    return;
  }

  flushPending(comparator, statusForPendingFlush(*pending, status), tcyc, output);
  pending = event;
}

void DwtPacketDecoder::flushPending(std::uint32_t comparator, const DwtTraceStatus& status, std::uint64_t tcyc,
                                    std::vector<TraceEvent>& output)
{
  auto& pending = pendingDataTrace_[comparator];
  if (!pending.has_value()) {
    return;
  }

  const auto makePacket = [&]() -> TraceEvent {
    if (pending->hasValue) {
      return TraceEvent(DwtDataTraceEvent{
          comparator,
          pending->size,
          pending->value,
          pending->isRead ? AccessType::Read : AccessType::Write,
          pending->hasAddressLo16 ? std::optional<std::uint32_t>(pending->addressLo16) : std::nullopt,
          pending->hasPc ? std::optional<std::uint32_t>(pending->pc) : std::nullopt,
      });
    }

    DwtAddressTraceLocation location = DwtOffsetTraceLocation{pending->addressLo16};
    if (pending->hasPc && pending->hasAddressLo16) {
      location = DwtPcAndOffsetTraceLocation{pending->pc, pending->addressLo16};
    } else if (pending->hasPc) {
      location = DwtPcTraceLocation{pending->pc};
    }
    return TraceEvent(DwtAddressTraceEvent{comparator, location});
  };

  TraceEvent packet = makePacket();
  packet.index = pending->index;
  packet.traceBusId = pending->traceBusId;
  packet.tcyc = tcyc;
  applyDwtTraceStatus(packet, status);
  output.push_back(std::move(packet));
  pending.reset();
}

DwtTraceStatus DwtPacketDecoder::statusForPendingFlush(const PendingDataTrace& pending,
                                                       const DwtTraceStatus& current) const
{
  DwtTraceStatus status = current;
  status.overflow = status.overflow || pending.status.overflow;
  status.timestampReliable = status.timestampReliable && !pending.status.overflow && pending.status.timestampReliable;
  status.overflowCount = std::max(status.overflowCount, pending.status.overflowCount);
  return status;
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
