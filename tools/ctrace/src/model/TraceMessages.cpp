/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TraceMessages.h"

#include "TraceEvent.h"

#include <algorithm>
#include <sstream>
#include <type_traits>

namespace {

std::string hexadecimal(std::uint32_t value)
{
  std::ostringstream text;
  text << std::hex << value;
  return text.str();
}

std::string render(const TraceOpaqueMessage& message, TraceMessageStyle style)
{
  return style == TraceMessageStyle::Detailed ? message.text : "Trace decode error";
}

std::string render(const TraceCounterPayload& message, TraceMessageStyle style)
{
  const std::string kind = message.kind == TraceCounterKind::Dwt ? "DWT" : "PMU";
  const auto size = std::to_string(message.size);
  const auto value = hexadecimal(message.value);
  if (style == TraceMessageStyle::Compact) {
    return "Invalid " + kind + " counter: " + size + " B; 0x" + value;
  }
  return "unsupported " + kind + " event-counter payload: size " + size + ", value 0x" + value +
         "; expected a non-zero 1-byte mask using bits " +
         (message.kind == TraceCounterKind::Dwt ? "0..5 only" : "0..7");
}

std::string render(const TraceInvalidExceptionAction& message, TraceMessageStyle style)
{
  return (style == TraceMessageStyle::Compact ? "Invalid action 0x0: exception "
                                            : "invalid exception action 0x0 for exception ") +
         std::to_string(message.number);
}

std::string render(const TracePcSamplePayload& message, TraceMessageStyle style)
{
  const auto size = std::to_string(message.size);
  if (style == TraceMessageStyle::Compact) {
    return "Invalid PC sample: " + size + " B; 0x" + hexadecimal(message.value);
  }
  return "unsupported DWT PC-sample payload: size " + size + ", value " + std::to_string(message.value) +
         "; expected a 4-byte PC or a 1-byte marker (0x00: CPU Sleeping, 0xff: Trace prohibited)";
}

std::string render(const TraceAddressPayload& message, TraceMessageStyle style)
{
  const std::string kind = message.kind == TraceAddressKind::DataAddress ? "data address" : "PC or match";
  const auto size = std::to_string(message.size);
  if (style == TraceMessageStyle::Compact) {
    return "Invalid DWT " + kind + ": " + size + " B";
  }
  return "unsupported DWT " + kind + " payload size " + size + "; expected 1, 2, or 4 bytes";
}

std::string render(const TraceRecovery& message, TraceMessageStyle style)
{
  const auto start = std::to_string(message.startOffset);
  const auto end = std::to_string(message.endOffset);
  const auto count = std::to_string(message.byteCount);
  if (style == TraceMessageStyle::Compact) {
    switch (message.kind) {
    case TraceRecoveryKind::Generic:
      return "Data loss; timestamp discontinuity";
    case TraceRecoveryKind::Consumed:
      return count + " raw bytes without usable ITM packets";
    case TraceRecoveryKind::Resumed:
      return "ITM resynced; raw span [" + start + "," + end + "): " + count + " bytes";
    case TraceRecoveryKind::Unresolved:
      return "No ITM resync before EOF; raw span [" + start + "," + end + "): " + count + " bytes";
    }
  }
  switch (message.kind) {
  case TraceRecoveryKind::Generic:
    return "data loss/resync boundary; timestamps across this point may not match";
  case TraceRecoveryKind::Consumed:
    return "OpenCSD consumed " + count +
           " raw bytes while waiting for usable ITM trace packets; data loss until a later sync/recovery point";
  case TraceRecoveryKind::Resumed:
    return "ITM decoding resumed at hardware SYNC at raw offset " + end + "; affected raw interval [" + start +
           ", " + end + ") spans " + count + " bytes";
  case TraceRecoveryKind::Unresolved:
    return "ITM decoding did not resume before end of input at raw offset " + end +
           "; no later hardware SYNC; affected raw interval [" + start + ", " + end + ") spans " + count + " bytes";
  }
  return {};
}

std::string nativeCategory(TraceNativeCategory category, TraceMessageStyle style)
{
  const bool compact = style == TraceMessageStyle::Compact;
  switch (category) {
  case TraceNativeCategory::Error:
    return compact ? "OpenCSD error" : "OpenCSD decoder error";
  case TraceNativeCategory::Warning:
    return compact ? "OpenCSD warning" : "OpenCSD decoder warning";
  case TraceNativeCategory::BadPacketSequence:
    return compact ? "Invalid ITM packet sequence" : "OpenCSD detected an invalid ITM packet sequence";
  case TraceNativeCategory::InvalidPacketHeader:
    return compact ? "Invalid ITM packet header" : "OpenCSD detected an invalid ITM packet header";
  case TraceNativeCategory::NotInitialized:
    return compact ? "OpenCSD not initialized" : "OpenCSD decoder is not initialized";
  case TraceNativeCategory::OutOfMemory:
    return compact ? "OpenCSD out of memory" : "OpenCSD decoder ran out of memory";
  case TraceNativeCategory::InvalidParameter:
    return compact ? "Invalid OpenCSD parameter" : "OpenCSD rejected a decoder parameter";
  case TraceNativeCategory::InputRead:
    return compact ? "OpenCSD input read failed" : "OpenCSD could not read required input data";
  case TraceNativeCategory::DecodeFailed:
    return compact ? "OpenCSD decode failed" : "OpenCSD could not decode the trace data";
  case TraceNativeCategory::InvalidOperation:
    return compact ? "Invalid OpenCSD operation" : "OpenCSD rejected a decoder operation";
  case TraceNativeCategory::InvalidData:
    return compact ? "Invalid OpenCSD trace data" : "OpenCSD rejected invalid trace data";
  case TraceNativeCategory::SystemError:
    return compact ? "OpenCSD system error" : "OpenCSD reported a system error";
  }
  return compact ? "OpenCSD error" : "OpenCSD decoder error";
}

std::string render(const TraceNativeDiagnostic& message, TraceMessageStyle style)
{
  const auto category = style == TraceMessageStyle::Compact && message.warning &&
                            message.category == TraceNativeCategory::Error
                            ? TraceNativeCategory::Warning : message.category;
  auto text = nativeCategory(category, style);
  if (style == TraceMessageStyle::Compact) {
    if (message.errorCode.has_value()) {
      text += " (code " + std::to_string(*message.errorCode) + ")";
    } else if (message.responseCode.has_value()) {
      text += " (response " + std::to_string(*message.responseCode) + ")";
    }
  } else {
    if (message.offset.has_value()) {
      text += " at raw offset " + std::to_string(*message.offset);
    }
    if (!message.nativeText.empty()) {
      text += ". " + message.nativeText;
    }
  }
  return text;
}

std::string render(const TracePacketDiagnostic& message, TraceMessageStyle style)
{
  switch (message.kind) {
  case TracePacketDiagnosticKind::Reserved:
    return "Reserved ITM packet";
  case TracePacketDiagnosticKind::BadSequence:
    return style == TraceMessageStyle::Compact ? "Invalid ITM packet sequence" : "Bad ITM packet sequence";
  case TracePacketDiagnosticKind::Incomplete:
    if (style == TraceMessageStyle::Compact) {
      return "Incomplete ITM packet at EOF";
    }
    return "incomplete ITM packet at end of input" +
           (message.offset.has_value() ? " at raw offset " + std::to_string(*message.offset) : "");
  }
  return {};
}

std::string render(const TraceMissingSync& message, TraceMessageStyle style)
{
  if (style == TraceMessageStyle::Compact) {
    return "No ITM SYNC before EOF";
  }
  return std::string("no hardware ITM SYNC before end of input") +
         (message.formatterOffset.has_value()
              ? "; first formatter group at raw offset " + std::to_string(*message.formatterOffset) : "");
}

std::string render(const TraceProgressFailure& message, TraceMessageStyle style)
{
  const bool compact = style == TraceMessageStyle::Compact;
  std::string text;
  switch (message.kind) {
  case TraceProgressKind::ConsumedTooMany:
    text = compact ? "Invalid consumed byte count" : "OpenCSD reported more formatted bytes processed than were supplied";
    break;
  case TraceProgressKind::PartialFormatterFrame:
    text = compact ? "Stopped inside formatter frame" : "OpenCSD stopped inside a memory-aligned formatter frame";
    break;
  case TraceProgressKind::FormattedInput:
    text = compact ? "No formatted decode progress" : "OpenCSD made no progress on formatted trace input";
    break;
  case TraceProgressKind::FormattedDrain:
    text = compact ? "No decode progress after formatted drain" : "OpenCSD made no progress after draining formatted trace";
    break;
  case TraceProgressKind::Retry:
    text = compact ? "No decode progress after retry" : "OpenCSD made no progress after a retry";
    break;
  case TraceProgressKind::ResetForSync:
    text = compact ? "No decode progress; reset for ITM SYNC"
                   : "OpenCSD made no progress while raw data was present; decoder reset and searching for next real ITM async sync";
    break;
  }
  if (message.decodeAborted && !compact) {
    text += "; decode aborted";
  }
  return text;
}

std::string render(const TraceFlushTimeout& message, TraceMessageStyle style)
{
  const std::string phase = message.phase == TraceFlushPhase::Wait ? "WAIT" : "formatted drain";
  const auto count = std::to_string(message.limit);
  if (style == TraceMessageStyle::Compact) {
    return "OpenCSD " + phase + " timeout: " + count + " flushes";
  }
  return "OpenCSD " + phase + " did not clear after " + count + " FLUSH operations" +
         (message.decodeAborted ? "; decode aborted" : "");
}

std::string render(const TraceInvalidFormattedChunk&, TraceMessageStyle style)
{
  return style == TraceMessageStyle::Compact ? "Invalid formatted chunk size"
                                            : "formatted raw trace chunk is not a multiple of 16 bytes";
}

std::string render(const TraceSetupFailure& message, TraceMessageStyle style)
{
  if (style == TraceMessageStyle::Compact) {
    return std::string("OpenCSD initialization failed") +
           (message.errorCode.has_value() ? " (code " + std::to_string(*message.errorCode) + ")" : "");
  }
  return message.nativeText.empty() ? formatTraceSetupOperation(message.operation) : message.nativeText;
}

std::string render(const TraceFormattedSessionFailure& message, TraceMessageStyle style)
{
  if (style == TraceMessageStyle::Compact) {
    return std::string("OpenCSD session operation failed") +
           (message.setup.has_value() && message.setup->errorCode.has_value()
                ? " (code " + std::to_string(*message.setup->errorCode) + ")" : "");
  }
  const auto detail = message.setup.has_value() ? render(*message.setup, style) : message.detail;
  return "formatted OpenCSD session operation failed: " + detail + " at raw input offset " +
         std::to_string(message.offset);
}

std::string render(const TraceInitializationFailure& message, TraceMessageStyle style)
{
  return style == TraceMessageStyle::Compact ? "OpenCSD initialization failed" : message.detail;
}

std::string phasePrefix(TraceAbortPhase phase, TraceMessageStyle style)
{
  const bool compact = style == TraceMessageStyle::Compact;
  switch (phase) {
  case TraceAbortPhase::None:
    return {};
  case TraceAbortPhase::Decode:
    return compact ? "Decode: " : "OpenCSD aborted decode: ";
  case TraceAbortPhase::EndOfTrace:
    return compact ? "End of trace: " : "OpenCSD aborted end-of-trace processing: ";
  case TraceAbortPhase::WaitFlush:
    return compact ? "WAIT flush: " : "OpenCSD aborted while flushing a WAIT response: ";
  case TraceAbortPhase::FormattedDrain:
    return compact ? "Formatted drain: " : "OpenCSD aborted while draining formatted trace: ";
  case TraceAbortPhase::DecoderReset:
    return compact ? "OpenCSD decoder reset failed: " : "OpenCSD decoder reset failed: ";
  case TraceAbortPhase::RouteReset:
    return compact ? "OpenCSD route reset failed: " : "OpenCSD route-local decoder reset failed: ";
  }
  return {};
}

std::string formatBaseMessage(const TraceMessage& message, TraceMessageStyle style)
{
  auto text = phasePrefix(message.phase, style) +
              std::visit([style](const auto& data) { return render(data, style); }, message.data);
  if (style == TraceMessageStyle::Detailed && message.packet.has_value()) {
    text += (!text.empty() && text.back() == ';' ? " " : "; ") + formatTracePacketContext(*message.packet);
  }
  return text;
}

void appendTimestampRange(std::string& text, const TraceMessage& message, TraceMessageStyle style)
{
  if (!message.timestampRange.has_value()) {
    return;
  }
  const auto& range = *message.timestampRange;
  const auto start = std::to_string(range.lastValid);
  if (style == TraceMessageStyle::Detailed) {
    text += "; timestamp " + start + " .. " +
            (range.firstResumed.has_value() ? std::to_string(*range.firstResumed) : "unknown") + ".";
  } else {
    text += "; cycles " + start + ".." +
            (range.firstResumed.has_value() ? std::to_string(*range.firstResumed) : "?");
  }
}

std::string fallbackIssue(const TraceIssueEvent& issue, std::uint64_t offset, TraceMessageStyle style)
{
  const bool compact = style == TraceMessageStyle::Compact;
  const auto atOffset = " at raw offset " + std::to_string(offset);
  switch (issue.code) {
  case TraceIssueCode::DataLoss:
    if (compact) {
      return std::string("ITM data loss") + (issue.rawBytesConsumed.has_value()
                 ? "; " + std::to_string(*issue.rawBytesConsumed) + " raw bytes affected" : "");
    }
    return (issue.rawBytesConsumed.has_value()
                ? std::to_string(*issue.rawBytesConsumed) + " raw bytes from raw offset " + std::to_string(offset)
                : "trace data" + atOffset) + " could not be decoded before the next hardware ITM sync";
  case TraceIssueCode::OpenCsdBadPacketSequence:
    return compact ? "Invalid ITM packet sequence" : "invalid ITM packet sequence" + atOffset;
  case TraceIssueCode::OpenCsdInvalidPacketHeader:
    return compact ? "Invalid ITM packet header" : "invalid ITM packet header" + atOffset;
  case TraceIssueCode::OpenCsdIncompleteTail:
    return compact ? "Incomplete ITM packet at EOF"
                   : "incomplete ITM packet starting" + atOffset + " at end of input";
  case TraceIssueCode::OpenCsdMissingSync:
    return compact ? "No ITM SYNC before EOF" : "no hardware ITM SYNC before end of input";
  case TraceIssueCode::OpenCsdNoProgress:
    return compact ? "No decode progress" : "OpenCSD made no decode progress" + atOffset;
  case TraceIssueCode::OpenCsdWaitTimeout:
    return compact ? "OpenCSD flush timeout" : "OpenCSD remained blocked while flushing pending data";
  case TraceIssueCode::OpenCsdInitializationError:
    return "OpenCSD initialization failed";
  default:
    if (!compact) {
      return "trace decode error" + atOffset;
    }
    break;
  }
  switch (issue.code) {
  case TraceIssueCode::InvalidExceptionAction: return "Invalid exception action";
  case TraceIssueCode::UnsupportedDwtEventCounterPayload: return "Invalid DWT counter";
  case TraceIssueCode::UnsupportedPmuEventCounterPayload: return "Invalid PMU counter";
  case TraceIssueCode::UnsupportedDwtAddressPayload: return "Invalid DWT address payload";
  case TraceIssueCode::UnsupportedDwtPcSamplePayload: return "Invalid PC sample";
  case TraceIssueCode::OpenCsdDecodeError:
    return issue.severity == TraceIssueSeverity::Warning ? "OpenCSD warning" : "OpenCSD error";
  default: return "Trace decode error";
  }
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
  auto text = std::to_string(skipped.byteCount);
  const auto id = skipped.traceId.has_value() ? std::to_string(*skipped.traceId) : "unknown";
  const bool compact = style == TraceMessageStyle::Compact;
  switch (skipped.reason) {
  case TraceByteSkipReason::NoSourceId:
    text += compact ? " bytes skipped: no source ID" : " bytes skipped due to missing source ID";
    break;
  case TraceByteSkipReason::NullSourceId:
    text += compact ? " bytes skipped: null source ID" : " bytes skipped for null source ID 0";
    break;
  case TraceByteSkipReason::ReservedSourceId:
    text += compact ? " bytes skipped: reserved source ID" : " bytes skipped for reserved source ID " + id;
    break;
  case TraceByteSkipReason::UnconfiguredSourceId:
    text += compact ? " bytes skipped: unconfigured source ID" : " bytes skipped for unconfigured source ID " + id;
    break;
  case TraceByteSkipReason::MissingSync:
    text += compact ? " bytes skipped: no SYNC" : " bytes skipped due to missing SYNC";
    break;
  }
  return compact ? text : text + "; first formatter group at raw offset " + std::to_string(skipped.formatterOffset);
}

std::string formatTraceMessage(const TraceDecodeAbort& failure, TraceMessageStyle style)
{
  if (style == TraceMessageStyle::Detailed) {
    return "decode aborted after processing " + std::to_string(failure.bytesProcessed) +
           " input bytes; trace is incomplete: " + formatTraceMessage(failure.reason, style);
  }
  auto text = "Decode aborted after " + std::to_string(failure.bytesProcessed) + " bytes; trace incomplete";
  if (!std::holds_alternative<TraceOpaqueMessage>(failure.reason.data)) {
    text += "; " + formatTraceMessage(failure.reason, style);
  }
  return text;
}

std::string formatTraceMessage(const OverflowTraceEvent& overflow, TraceMessageStyle style)
{
  if (style == TraceMessageStyle::Compact) {
    return "Timestamp discontinuity";
  }
  return overflow.message.empty() ? "overflow: new timestamp segment; time across boundary may be unreliable"
                                  : overflow.message;
}

std::string formatTraceIssue(const TraceIssueEvent& issue, std::uint64_t rawOffset, TraceMessageStyle style)
{
  const auto* opaque = std::get_if<TraceOpaqueMessage>(&issue.message.data);
  const bool fallback = opaque != nullptr && (opaque->text.empty() || style == TraceMessageStyle::Compact);
  auto text = fallback ? fallbackIssue(issue, rawOffset, style) : formatBaseMessage(issue.message, style);
  const auto* recovery = std::get_if<TraceRecovery>(&issue.message.data);
  const bool containsInterval = recovery != nullptr &&
      (recovery->kind == TraceRecoveryKind::Resumed || recovery->kind == TraceRecoveryKind::Unresolved);
  if (style == TraceMessageStyle::Compact && !containsInterval) {
    text += "; raw@" + std::to_string(rawOffset);
  }
  appendTimestampRange(text, issue.message, style);
  return text;
}

std::string formatTracePacketContext(const TracePacketContext& packet)
{
  const char* name = "RESERVED";
  switch (packet.kind) {
  case TracePacketKind::Reserved: name = "RESERVED"; break;
  case TracePacketKind::Incomplete: name = "INCOMPLETE_EOT"; break;
  case TracePacketKind::BadSequence: name = "BAD_SEQUENCE"; break;
  case TracePacketKind::Async: name = "ASYNC"; break;
  case TracePacketKind::Overflow: name = "OVERFLOW"; break;
  case TracePacketKind::Software: name = "SWIT"; break;
  case TracePacketKind::Dwt: name = "DWT"; break;
  case TracePacketKind::LocalTimestamp: name = "TS_LOCAL"; break;
  case TracePacketKind::GlobalTimestamp1: name = "TS_GLOBAL_1"; break;
  case TracePacketKind::GlobalTimestamp2: name = "TS_GLOBAL_2"; break;
  case TracePacketKind::Extension: name = "EXTENSION"; break;
  }
  auto text = "packet=" + std::string(name) + ", size=" + std::to_string(packet.size) +
              (packet.size == 1U ? " byte, bytes=" : " bytes, bytes=");
  if (!packet.bytes.has_value() && packet.size != 0U) {
    return text + "unavailable";
  }
  text += '[';
  constexpr char digits[] = "0123456789abcdef";
  const auto shown = packet.bytes.has_value()
                         ? std::min({packet.bytes->size(), static_cast<std::size_t>(packet.size), std::size_t{16U}})
                         : 0U;
  for (std::size_t i = 0; i < shown; ++i) {
    if (i != 0U) {
      text += ' ';
    }
    const auto value = (*packet.bytes)[i];
    text += digits[value >> 4U];
    text += digits[value & 0xfU];
  }
  if (shown < packet.size) {
    text += " ... (truncated)";
  }
  return text + ']';
}

std::string formatTraceSetupOperation(TraceSetupOperation operation)
{
  switch (operation) {
  case TraceSetupOperation::AlreadyActive: return "another OpenCSD DecodeTree session is already active";
  case TraceSetupOperation::UnsupportedSource: return "unsupported OpenCSD DecodeTree source type";
  case TraceSetupOperation::MissingCreator: return "OpenCSD DecodeTree creator is not configured";
  case TraceSetupOperation::MissingDestroyer: return "OpenCSD DecodeTree destroyer is not configured";
  case TraceSetupOperation::CreateTree: return "failed to create OpenCSD DecodeTree";
  case TraceSetupOperation::CreateDecoder: return "failed to create OpenCSD decoder";
  case TraceSetupOperation::DecoderElement: return "OpenCSD decoder element is not initialized";
  case TraceSetupOperation::DecoderManager: return "OpenCSD decoder manager is not initialized";
  case TraceSetupOperation::FullDecoderComponent: return "OpenCSD full decoder component is not initialized";
  case TraceSetupOperation::PacketProcessor: return "OpenCSD associated packet processor is not initialized";
  case TraceSetupOperation::AttachPacketLogger: return "failed to attach OpenCSD packet-processor error logger";
  case TraceSetupOperation::AttachPacketMonitor: return "failed to attach OpenCSD packet monitor";
  case TraceSetupOperation::RawMonitorRequiresFormatted: return "OpenCSD raw frame monitor requires a formatted DecodeTree";
  case TraceSetupOperation::FrameDeformatter: return "OpenCSD frame deformatter is not initialized";
  case TraceSetupOperation::RawFrameAttachPoint: return "OpenCSD raw-frame attach point is not initialized";
  case TraceSetupOperation::AttachRawFrameMonitor: return "failed to attach OpenCSD raw frame monitor";
  case TraceSetupOperation::DecoderComponent: return "OpenCSD decoder component is not initialized";
  case TraceSetupOperation::ResolveDecoderInput: return "failed to resolve OpenCSD decoder input";
  case TraceSetupOperation::DecoderInput: return "OpenCSD decoder input is not initialized";
  case TraceSetupOperation::SingleChannel: return "OpenCSD SINGLE tree requires decoder channel 0";
  case TraceSetupOperation::FormattedChannel: return "OpenCSD formatted tree requires a decoder channel between 1 and 111";
  case TraceSetupOperation::MissingSession: return "OpenCSD ITM session factory returned no session";
  case TraceSetupOperation::EmptyFormattedRoutes: return "formatted OpenCSD ITM session requires at least one route";
  case TraceSetupOperation::InvalidFormattedRoute: return "formatted OpenCSD ITM route requires a Trace Bus ID between 1 and 111";
  case TraceSetupOperation::DuplicateFormattedRoute: return "formatted OpenCSD ITM routes require unique Trace Bus IDs";
  }
  return "OpenCSD initialization failed";
}

std::string formatOverflowSummary(const TraceOverflowSummary& summary)
{
  auto text = "first overflow occurred at " +
              (summary.firstTimestamp.has_value() ? "cycle timestamp " + std::to_string(*summary.firstTimestamp)
                                                  : std::string("an unknown cycle timestamp"));
  if (summary.packetCount > 1U) {
    text += "; " + std::to_string(summary.packetCount - 1U) + " more occurred";
  }
  return text;
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
