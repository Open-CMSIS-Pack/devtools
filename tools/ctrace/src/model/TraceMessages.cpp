/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TraceMessages.h"

#include "TraceEvent.h"

#include <algorithm>
#include <array>
#include <sstream>
#include <stdexcept>
#include <type_traits>

namespace {

/** @brief Maps a typed discriminator to its catalog ID, rejecting invalid internal values. */
template <typename Enum, std::size_t N>
MessageId selectMessage(Enum value, const std::array<MessageId, N>& ids)
{
  const auto index = static_cast<std::size_t>(value);
  if (index >= N) {
    throw std::invalid_argument(formatMessage(MessageId::TraceUnknownParameter, MessageStyle::Detailed,
                                              {static_cast<int>(value)}));
  }
  return ids[index];
}

std::string hexadecimal(std::uint32_t value)
{
  std::ostringstream text;
  text << std::hex << value;
  return text.str();
}

std::string render(const TraceOpaqueMessage& message, TraceMessageStyle style)
{
  return style == TraceMessageStyle::Detailed ? message.text : formatMessage(MessageId::TraceDecodeError, style);
}

std::string render(const TraceCounterPayload& message, TraceMessageStyle style)
{
  constexpr std::array<MessageId, 2U> ids{{MessageId::TraceDwtCounterPayload, MessageId::TracePmuCounterPayload}};
  const auto id = selectMessage(message.kind, ids);
  return formatMessage(id, style, {message.size, hexadecimal(message.value)});
}

std::string render(const TraceInvalidExceptionAction& message, TraceMessageStyle style)
{
  return formatMessage(MessageId::TraceInvalidExceptionAction, style, {message.number});
}

std::string render(const TracePcSamplePayload& message, TraceMessageStyle style)
{
  return formatMessage(MessageId::TracePcSamplePayload, style, {message.size, message.value});
}

std::string render(const TraceAddressPayload& message, TraceMessageStyle style)
{
  constexpr std::array<MessageId, 2U> ids{{MessageId::TraceDataAddressPayload, MessageId::TracePcOrMatchPayload}};
  const auto id = selectMessage(message.kind, ids);
  return formatMessage(id, style, {message.size});
}

std::string render(const TraceRecovery& message, TraceMessageStyle style)
{
  constexpr std::array<MessageId, 4U> ids{{
      MessageId::TraceRecoveryGeneric, MessageId::TraceRecoveryConsumed,
      MessageId::TraceRecoveryResumed, MessageId::TraceRecoveryUnresolved,
  }};
  const auto id = selectMessage(message.kind, ids);
  if (message.kind == TraceRecoveryKind::Generic) {
    return formatMessage(id, style);
  }
  if (message.kind == TraceRecoveryKind::Consumed) {
    return formatMessage(id, style, {message.byteCount});
  }
  return formatMessage(id, style, {message.startOffset, message.endOffset, message.byteCount});
}

std::string nativeCategory(TraceNativeCategory category, TraceMessageStyle style)
{
  constexpr std::array<MessageId, 12U> ids{{
      MessageId::TraceNativeError,
      MessageId::TraceNativeWarning,
      MessageId::TraceNativeBadPacketSequence,
      MessageId::TraceNativeInvalidPacketHeader,
      MessageId::TraceNativeNotInitialized,
      MessageId::TraceNativeOutOfMemory,
      MessageId::TraceNativeInvalidParameter,
      MessageId::TraceNativeInputRead,
      MessageId::TraceNativeDecodeFailed,
      MessageId::TraceNativeInvalidOperation,
      MessageId::TraceNativeInvalidData,
      MessageId::TraceNativeSystemError
  }};
  return formatMessage(selectMessage(category, ids), style);
}

std::string render(const TraceNativeDiagnostic& message, TraceMessageStyle style)
{
  const auto category = style == TraceMessageStyle::Compact && message.warning &&
                            message.category == TraceNativeCategory::Error
                            ? TraceNativeCategory::Warning : message.category;
  auto text = nativeCategory(category, style);
  if (style == TraceMessageStyle::Detailed) {
    if (message.offset.has_value()) {
      text = formatMessage(MessageId::TraceAtRawOffset, style, {text, *message.offset});
    }
    if (!message.nativeText.empty()) {
      text = formatMessage(MessageId::TraceNativeDetail, style, {text, message.nativeText});
    }
    if (message.errorCode.has_value()) {
      text = formatMessage(MessageId::TraceNativeErrorCode, style, {text, *message.errorCode});
    }
    if (message.responseCode.has_value()) {
      text = formatMessage(MessageId::TraceNativeResponseCode, style, {text, *message.responseCode});
    }
  }
  return text;
}

std::string render(const TracePacketDiagnostic& message, TraceMessageStyle style)
{
  constexpr std::array<MessageId, 3U> ids{{
      MessageId::TracePacketReserved, MessageId::TracePacketBadSequence, MessageId::TracePacketIncomplete,
  }};
  auto text = formatMessage(selectMessage(message.kind, ids), style);
  if (message.kind == TracePacketDiagnosticKind::Incomplete && message.offset.has_value()) {
    text = formatMessage(MessageId::TraceAtRawOffset, style, {text, *message.offset});
  }
  return text;
}

std::string render(const TraceMissingSync& message, TraceMessageStyle style)
{
  return message.formatterOffset.has_value()
             ? formatMessage(MessageId::TraceMissingSyncAtOffset, style, {*message.formatterOffset})
             : formatMessage(MessageId::TraceMissingSync, style);
}

std::string render(const TraceProgressFailure& message, TraceMessageStyle style)
{
  constexpr std::array<MessageId, 6U> ids{{
      MessageId::TraceProgressConsumedTooMany,
      MessageId::TraceProgressPartialFormatterFrame,
      MessageId::TraceProgressFormattedInput,
      MessageId::TraceProgressFormattedDrain,
      MessageId::TraceProgressRetry,
      MessageId::TraceProgressResetForSync
  }};
  auto text = formatMessage(selectMessage(message.kind, ids), style);
  return message.decodeAborted ? formatMessage(MessageId::TraceAbortedSuffix, style, {text}) : text;
}

std::string render(const TraceFlushTimeout& message, TraceMessageStyle style)
{
  constexpr std::array<MessageId, 2U> ids{{MessageId::TraceWaitTimeout, MessageId::TraceDrainTimeout}};
  const auto id = selectMessage(message.phase, ids);
  auto text = formatMessage(id, style, {message.limit});
  return message.decodeAborted ? formatMessage(MessageId::TraceAbortedSuffix, style, {text}) : text;
}

std::string render(const TraceInvalidFormattedChunk&, TraceMessageStyle style)
{
  return formatMessage(MessageId::TraceInvalidFormattedChunk, style);
}

MessageId setupMessageId(TraceSetupOperation operation)
{
  constexpr std::array<MessageId, 25U> ids{{
      MessageId::TraceSetupAlreadyActive,
      MessageId::TraceSetupUnsupportedSource,
      MessageId::TraceSetupMissingCreator,
      MessageId::TraceSetupMissingDestroyer,
      MessageId::TraceSetupCreateTree,
      MessageId::TraceSetupCreateDecoder,
      MessageId::TraceSetupDecoderElement,
      MessageId::TraceSetupDecoderManager,
      MessageId::TraceSetupFullDecoderComponent,
      MessageId::TraceSetupPacketProcessor,
      MessageId::TraceSetupAttachPacketLogger,
      MessageId::TraceSetupAttachPacketMonitor,
      MessageId::TraceSetupRawMonitorRequiresFormatted,
      MessageId::TraceSetupFrameDeformatter,
      MessageId::TraceSetupRawFrameAttachPoint,
      MessageId::TraceSetupAttachRawFrameMonitor,
      MessageId::TraceSetupDecoderComponent,
      MessageId::TraceSetupResolveDecoderInput,
      MessageId::TraceSetupDecoderInput,
      MessageId::TraceSetupSingleChannel,
      MessageId::TraceSetupFormattedChannel,
      MessageId::TraceSetupMissingSession,
      MessageId::TraceSetupEmptyFormattedRoutes,
      MessageId::TraceSetupInvalidFormattedRoute,
      MessageId::TraceSetupDuplicateFormattedRoute
  }};
  return selectMessage(operation, ids);
}

std::string render(const TraceSetupFailure& message, TraceMessageStyle style)
{
  auto text = style == TraceMessageStyle::Detailed && !message.nativeText.empty()
                  ? message.nativeText : formatMessage(setupMessageId(message.operation), style);
  return style == TraceMessageStyle::Detailed && message.errorCode.has_value()
             ? formatMessage(MessageId::TraceNativeErrorCode, style, {text, *message.errorCode}) : text;
}

std::string render(const TraceFormattedSessionFailure& message, TraceMessageStyle style)
{
  const auto detail = message.setup.has_value() ? render(*message.setup, TraceMessageStyle::Detailed) : message.detail;
  return formatMessage(MessageId::TraceFormattedSessionFailure, style, {detail, message.offset});
}

std::string render(const TraceInitializationFailure& message, TraceMessageStyle style)
{
  return style == TraceMessageStyle::Compact ? formatMessage(MessageId::TraceInitializationFailure, style)
                                             : message.detail;
}

std::string formatBaseMessage(const TraceMessage& message, TraceMessageStyle style)
{
  constexpr std::array<MessageId, 7U> phases{{
      MessageId::TraceAbortNone,
      MessageId::TraceAbortDecode,
      MessageId::TraceAbortEndOfTrace,
      MessageId::TraceAbortWaitFlush,
      MessageId::TraceAbortFormattedDrain,
      MessageId::TraceAbortDecoderReset,
      MessageId::TraceAbortRouteReset
  }};
  auto text = formatMessage(selectMessage(message.phase, phases), style,
                            {std::visit([style](const auto& data) { return render(data, style); }, message.data)});
  if (style == TraceMessageStyle::Detailed && message.packet.has_value()) {
    const auto id = !text.empty() && text.back() == ';' ? MessageId::TraceAppendPacketAfterSeparator
                                                      : MessageId::TraceAppendPacket;
    text = formatMessage(id, style, {text, formatTracePacketContext(*message.packet)});
  }
  return text;
}

void appendTimestampRange(std::string& text, const TraceMessage& message, TraceMessageStyle style)
{
  if (style != TraceMessageStyle::Detailed || !message.timestampRange.has_value()) {
    return;
  }
  const auto& range = *message.timestampRange;
  text = range.firstResumed.has_value()
             ? formatMessage(MessageId::TraceTimestampRange, style, {text, range.lastValid, *range.firstResumed})
             : formatMessage(MessageId::TraceTimestampRangeUnknown, style, {text, range.lastValid});
}

std::string fallbackIssue(const TraceIssueEvent& issue, std::uint64_t offset, TraceMessageStyle style)
{
  MessageId id = MessageId::TraceFallbackDecodeError;
  switch (issue.code) {
  case TraceIssueCode::DataLoss:
    return issue.rawBytesConsumed.has_value()
               ? formatMessage(MessageId::TraceFallbackDataLossCount, style, {*issue.rawBytesConsumed, offset})
               : formatMessage(MessageId::TraceFallbackDataLoss, style, {offset});
  case TraceIssueCode::OpenCsdBadPacketSequence: id = MessageId::TraceFallbackBadPacketSequence; break;
  case TraceIssueCode::OpenCsdInvalidPacketHeader: id = MessageId::TraceFallbackInvalidPacketHeader; break;
  case TraceIssueCode::OpenCsdIncompleteTail: id = MessageId::TraceFallbackIncompleteTail; break;
  case TraceIssueCode::OpenCsdNoProgress: id = MessageId::TraceFallbackNoProgress; break;
  case TraceIssueCode::OpenCsdMissingSync: return formatMessage(MessageId::TraceMissingSync, style);
  case TraceIssueCode::OpenCsdWaitTimeout: return formatMessage(MessageId::TraceFallbackWaitTimeout, style);
  case TraceIssueCode::OpenCsdInitializationError: return formatMessage(MessageId::TraceInitializationFailure, style);
  case TraceIssueCode::InvalidExceptionAction: id = MessageId::TraceFallbackInvalidExceptionAction; break;
  case TraceIssueCode::UnsupportedDwtEventCounterPayload: id = MessageId::TraceFallbackDwtCounter; break;
  case TraceIssueCode::UnsupportedPmuEventCounterPayload: id = MessageId::TraceFallbackPmuCounter; break;
  case TraceIssueCode::UnsupportedDwtAddressPayload: id = MessageId::TraceFallbackDwtAddress; break;
  case TraceIssueCode::UnsupportedDwtPcSamplePayload: id = MessageId::TraceFallbackPcSample; break;
  case TraceIssueCode::OpenCsdDecodeError:
    id = issue.severity == TraceIssueSeverity::Warning ? MessageId::TraceFallbackNativeWarning
                                                      : MessageId::TraceFallbackNativeError;
    break;
  default: break;
  }
  return formatMessage(id, style, {offset});
}

} // namespace

std::string formatTraceMessage(const TraceMessage& message, TraceMessageStyle style)
{
  auto text = formatBaseMessage(message, style);
  appendTimestampRange(text, message, style);
  return text;
}

std::string formatTraceMessage(const TraceByteSkip& skipped, TraceMessageStyle style)
{
  constexpr std::array<MessageId, 5U> ids{{
      MessageId::TraceSkipNoSourceId,
      MessageId::TraceSkipNullSourceId,
      MessageId::TraceSkipReservedSourceId,
      MessageId::TraceSkipUnconfiguredSourceId,
      MessageId::TraceSkipMissingSync
  }};
  const auto id = selectMessage(skipped.reason, ids);
  if (skipped.reason == TraceByteSkipReason::ReservedSourceId ||
      skipped.reason == TraceByteSkipReason::UnconfiguredSourceId) {
    const auto source = skipped.traceId.has_value() ? std::to_string(*skipped.traceId)
                                                   : formatMessage(MessageId::TraceUnknownSourceId);
    return formatMessage(id, style, {skipped.byteCount, skipped.formatterOffset, source});
  }
  return formatMessage(id, style, {skipped.byteCount, skipped.formatterOffset});
}

std::string formatTraceMessage(const TraceDecodeAbort& failure, TraceMessageStyle style)
{
  const auto id = std::holds_alternative<TraceOpaqueMessage>(failure.reason.data)
                      ? MessageId::TraceDecodeAbortOpaque : MessageId::TraceDecodeAbort;
  return formatMessage(id, style, {failure.bytesProcessed, formatTraceMessage(failure.reason, style)});
}

std::string formatTraceMessage(const OverflowTraceEvent& overflow, TraceMessageStyle style)
{
  return style == TraceMessageStyle::Detailed && !overflow.message.empty()
             ? overflow.message : formatMessage(MessageId::TraceOverflow, style);
}

std::string formatTraceIssue(const TraceIssueEvent& issue, std::uint64_t rawOffset, TraceMessageStyle style)
{
  const auto* opaque = std::get_if<TraceOpaqueMessage>(&issue.message.data);
  const auto* initialization = std::get_if<TraceInitializationFailure>(&issue.message.data);
  const bool fallback = (opaque != nullptr && (opaque->text.empty() || style == TraceMessageStyle::Compact)) ||
                        (initialization != nullptr && initialization->detail.empty());
  auto text = fallback ? fallbackIssue(issue, rawOffset, style) : formatBaseMessage(issue.message, style);
  appendTimestampRange(text, issue.message, style);
  return text;
}

std::string formatTracePacketContext(const TracePacketContext& packet)
{
  constexpr std::array<MessageId, 11U> kinds{{
      MessageId::TracePacketKindReserved,
      MessageId::TracePacketKindIncomplete,
      MessageId::TracePacketKindBadSequence,
      MessageId::TracePacketKindAsync,
      MessageId::TracePacketKindOverflow,
      MessageId::TracePacketKindSoftware,
      MessageId::TracePacketKindDwt,
      MessageId::TracePacketKindLocalTimestamp,
      MessageId::TracePacketKindGlobalTimestamp1,
      MessageId::TracePacketKindGlobalTimestamp2,
      MessageId::TracePacketKindExtension
  }};
  const auto name = formatMessage(selectMessage(packet.kind, kinds));
  std::string bytes;
  bool truncated = false;
  if (!packet.bytes.has_value() && packet.size != 0U) {
    bytes = formatMessage(MessageId::TracePacketBytesUnavailable);
  } else {
    constexpr char digits[] = "0123456789abcdef";
    const auto shown = packet.bytes.has_value()
                           ? std::min({packet.bytes->size(), static_cast<std::size_t>(packet.size), std::size_t{16U}})
                           : 0U;
    for (std::size_t i = 0; i < shown; ++i) {
      if (i != 0U) {
        bytes += ' ';
      }
      const auto value = (*packet.bytes)[i];
      bytes += digits[value >> 4U];
      bytes += digits[value & 0xfU];
    }
    truncated = shown < packet.size;
    bytes = formatMessage(MessageId::TracePacketBytes, MessageStyle::Detailed, {bytes});
  }
  return formatMessage(MessageId::TracePacketPreview, MessageStyle::Detailed,
                       {name, packet.size, bytes, truncated ? "true" : "false"});
}

std::string formatTraceSetupOperation(TraceSetupOperation operation)
{
  return formatMessage(setupMessageId(operation));
}

std::string formatOverflowSummary(const TraceOverflowSummary& summary, TraceMessageStyle style)
{
  auto text = summary.firstTimestamp.has_value()
                  ? formatMessage(MessageId::TraceOverflowSummary, style, {*summary.firstTimestamp})
                  : formatMessage(MessageId::TraceOverflowSummaryUnknown, style);
  return summary.packetCount > 1U
             ? formatMessage(MessageId::TraceOverflowSummaryMore, style, {text, summary.packetCount - 1U})
             : text;
}

TraceIssueEvent makeTraceIssue(TraceMessage message, TraceIssueSeverity severity)
{
  const auto code = std::visit([](const auto& data) {
    using T = std::decay_t<decltype(data)>;
    if constexpr (std::is_same_v<T, TraceCounterPayload>) {
      return data.kind == TraceCounterKind::Dwt ? TraceIssueCode::UnsupportedDwtEventCounterPayload
                                                 : TraceIssueCode::UnsupportedPmuEventCounterPayload;
    } else if constexpr (std::is_same_v<T, TraceInvalidExceptionAction>) {
      return TraceIssueCode::InvalidExceptionAction;
    } else if constexpr (std::is_same_v<T, TracePcSamplePayload>) {
      return TraceIssueCode::UnsupportedDwtPcSamplePayload;
    } else if constexpr (std::is_same_v<T, TraceAddressPayload>) {
      return TraceIssueCode::UnsupportedDwtAddressPayload;
    } else if constexpr (std::is_same_v<T, TraceRecovery>) {
      return TraceIssueCode::DataLoss;
    } else if constexpr (std::is_same_v<T, TraceNativeDiagnostic>) {
      if (data.category == TraceNativeCategory::BadPacketSequence) {
        return TraceIssueCode::OpenCsdBadPacketSequence;
      }
      if (data.category == TraceNativeCategory::InvalidPacketHeader) {
        return TraceIssueCode::OpenCsdInvalidPacketHeader;
      }
      return TraceIssueCode::OpenCsdDecodeError;
    } else if constexpr (std::is_same_v<T, TracePacketDiagnostic>) {
      return data.kind == TracePacketDiagnosticKind::Incomplete ? TraceIssueCode::OpenCsdIncompleteTail
                                                                : TraceIssueCode::OpenCsdDecodeError;
    } else if constexpr (std::is_same_v<T, TraceMissingSync>) {
      return TraceIssueCode::OpenCsdMissingSync;
    } else if constexpr (std::is_same_v<T, TraceProgressFailure>) {
      return TraceIssueCode::OpenCsdNoProgress;
    } else if constexpr (std::is_same_v<T, TraceFlushTimeout>) {
      return TraceIssueCode::OpenCsdWaitTimeout;
    } else if constexpr (std::is_same_v<T, TraceSetupFailure> || std::is_same_v<T, TraceInitializationFailure>) {
      return TraceIssueCode::OpenCsdInitializationError;
    } else if constexpr (std::is_same_v<T, TraceInvalidFormattedChunk> ||
                         std::is_same_v<T, TraceFormattedSessionFailure>) {
      return TraceIssueCode::OpenCsdDecodeError;
    } else {
      return TraceIssueCode::DecodeError;
    }
  }, message.data);
  return {code, severity, std::move(message)};
}

TraceIssueEvent makeTraceIssue(TraceMessage message)
{
  return makeTraceIssue(std::move(message), TraceIssueSeverity::Error);
}
