/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_MODEL_TRACEMESSAGES_H
#define CTRACE_SRC_MODEL_TRACEMESSAGES_H

#include "Messages.h"

#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

/** @brief Chooses the complete verbose description or the compact CLI/CSV note. */
using TraceMessageStyle = MessageStyle;

/** @brief Retains foreign text without inferring structured parameters from it. */
struct TraceOpaqueMessage {
  std::string text;
};

/** @brief Selects the event-counter protocol whose payload failed validation. */
enum class TraceCounterKind {
  Dwt,
  Pmu,
};
struct TraceCounterPayload {
  TraceCounterKind kind = TraceCounterKind::Dwt;
  std::uint8_t size = 0;
  std::uint32_t value = 0;
};
struct TraceInvalidExceptionAction {
  std::uint16_t number = 0;
};
struct TracePcSamplePayload {
  std::uint8_t size = 0;
  std::uint32_t value = 0;
};
enum class TraceAddressKind {
  DataAddress,
  PcOrMatch,
};
struct TraceAddressPayload {
  TraceAddressKind kind = TraceAddressKind::DataAddress;
  std::uint8_t size = 0;
};

enum class TraceRecoveryKind {
  Generic,
  Consumed,
  Resumed,
  Unresolved,
};
/** @brief Describes recovery; formatted intervals include control bytes and other routes. */
struct TraceRecovery {
  TraceRecoveryKind kind = TraceRecoveryKind::Generic;
  std::uint64_t startOffset = 0;
  std::uint64_t endOffset = 0;
  std::uint64_t byteCount = 0;
};

enum class TraceNativeCategory {
  Error,
  Warning,
  BadPacketSequence,
  InvalidPacketHeader,
  NotInitialized,
  OutOfMemory,
  InvalidParameter,
  InputRead,
  DecodeFailed,
  InvalidOperation,
  InvalidData,
  SystemError,
};
/** @brief Keeps native error and response code domains separate from their library-owned text. */
struct TraceNativeDiagnostic {
  TraceNativeCategory category = TraceNativeCategory::Error;
  std::optional<int> errorCode = std::nullopt;
  std::optional<int> responseCode = std::nullopt;
  std::string nativeText;
  std::optional<std::uint64_t> offset = std::nullopt;
  bool warning = false;
};

enum class TracePacketDiagnosticKind {
  Reserved,
  BadSequence,
  Incomplete,
};
struct TracePacketDiagnostic {
  TracePacketDiagnosticKind kind = TracePacketDiagnosticKind::Reserved;
  std::optional<std::uint64_t> offset = std::nullopt;
};
enum class TracePacketKind {
  Reserved,
  Incomplete,
  BadSequence,
  Async,
  Overflow,
  Software,
  Dwt,
  LocalTimestamp,
  GlobalTimestamp1,
  GlobalTimestamp2,
  Extension,
};
/** @brief Stores a bounded protocol-packet preview (deformatted for TB); nullopt denotes unavailable bytes. */
struct TracePacketContext {
  TracePacketKind kind = TracePacketKind::Reserved;
  std::uint32_t size = 0;
  std::optional<std::vector<std::uint8_t>> bytes = std::nullopt;
};
struct TraceMissingSync {
  std::optional<std::uint64_t> formatterOffset = std::nullopt;
};
enum class TraceProgressKind {
  ConsumedTooMany,
  PartialFormatterFrame,
  FormattedInput,
  FormattedDrain,
  Retry,
  ResetForSync,
};
struct TraceProgressFailure {
  TraceProgressKind kind = TraceProgressKind::FormattedInput;
  bool decodeAborted = false;
};
enum class TraceFlushPhase {
  Wait,
  FormattedDrain,
};
struct TraceFlushTimeout {
  TraceFlushPhase phase = TraceFlushPhase::Wait;
  std::uint64_t limit = 0;
  bool decodeAborted = false;
};
struct TraceInvalidFormattedChunk {};
enum class TraceSetupOperation {
  AlreadyActive,
  UnsupportedSource,
  MissingCreator,
  MissingDestroyer,
  CreateTree,
  CreateDecoder,
  DecoderElement,
  DecoderManager,
  FullDecoderComponent,
  PacketProcessor,
  AttachPacketLogger,
  AttachPacketMonitor,
  RawMonitorRequiresFormatted,
  FrameDeformatter,
  RawFrameAttachPoint,
  AttachRawFrameMonitor,
  DecoderComponent,
  ResolveDecoderInput,
  DecoderInput,
  SingleChannel,
  FormattedChannel,
  MissingSession,
  EmptyFormattedRoutes,
  InvalidFormattedRoute,
  DuplicateFormattedRoute,
};
struct TraceSetupFailure {
  TraceSetupOperation operation = TraceSetupOperation::CreateTree;
  // Native API errors retain their exact library-rendered description.
  std::string nativeText{};
  std::optional<int> errorCode = std::nullopt;
};
/** @brief Preserves a typed setup cause when a formatted session operation throws it. */
struct TraceFormattedSessionFailure {
  std::string detail;
  std::uint64_t offset = 0;
  std::optional<TraceSetupFailure> setup = std::nullopt;
};
struct TraceInitializationFailure {
  std::string detail;
};

/** @brief Identifies a fatal decoder operation without embedding prefixes in producer text. */
enum class TraceAbortPhase {
  None,
  Decode,
  EndOfTrace,
  WaitFlush,
  FormattedDrain,
  DecoderReset,
  RouteReset,
};
struct TraceTimestampRange {
  std::uint64_t lastValid = 0;
  std::optional<std::uint64_t> firstResumed = std::nullopt;
};

/** @brief Carries typed diagnostic data until a CLI or CSV formatter chooses its wording. */
struct TraceMessage {
  using Data = std::variant<TraceOpaqueMessage, TraceCounterPayload, TraceInvalidExceptionAction,
                            TracePcSamplePayload, TraceAddressPayload, TraceRecovery, TraceNativeDiagnostic,
                            TracePacketDiagnostic, TraceMissingSync, TraceProgressFailure, TraceFlushTimeout,
                            TraceInvalidFormattedChunk, TraceFormattedSessionFailure, TraceSetupFailure,
                            TraceInitializationFailure>;

  TraceMessage() = default;
  TraceMessage(const char* text)
    : data(TraceOpaqueMessage{text})
  {
  }
  TraceMessage(std::string text)
    : data(TraceOpaqueMessage{std::move(text)})
  {
  }
  template <typename Payload, std::enable_if_t<std::is_constructible_v<Data, Payload>, int> = 0>
  TraceMessage(Payload payload)
    : data(std::move(payload))
  {
  }

  bool empty() const noexcept
  {
    const auto* opaque = std::get_if<TraceOpaqueMessage>(&data);
    return opaque != nullptr && opaque->text.empty();
  }

  Data data;
  std::optional<TracePacketContext> packet;
  std::optional<TraceTimestampRange> timestampRange;
  TraceAbortPhase phase = TraceAbortPhase::None;
};

struct TraceByteSkip;
struct TraceDecodeAbort;
struct OverflowTraceEvent;
struct TraceIssueEvent;
enum class TraceIssueSeverity;

struct TraceOverflowSummary {
  std::optional<std::uint64_t> firstTimestamp;
  std::uint64_t packetCount = 0;
};

std::string formatTraceMessage(const TraceMessage& message, TraceMessageStyle style);
std::string formatTraceMessage(const TraceByteSkip& skipped, TraceMessageStyle style);
std::string formatTraceMessage(const TraceDecodeAbort& failure, TraceMessageStyle style);
std::string formatTraceMessage(const OverflowTraceEvent& overflow, TraceMessageStyle style);
std::string formatTraceIssue(const TraceIssueEvent& issue, std::uint64_t rawOffset, TraceMessageStyle style);
std::string formatTracePacketContext(const TracePacketContext& packet);
std::string formatTraceSetupOperation(TraceSetupOperation operation);
std::string formatOverflowSummary(const TraceOverflowSummary& summary,
                                  TraceMessageStyle style = TraceMessageStyle::Detailed);
TraceIssueEvent makeTraceIssue(TraceMessage message, TraceIssueSeverity severity);
TraceIssueEvent makeTraceIssue(TraceMessage message);

#endif // CTRACE_SRC_MODEL_TRACEMESSAGES_H
