/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TraceMessages.h"
#include "TraceEvent.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace {
constexpr auto detailed = TraceMessageStyle::Detailed;
constexpr auto compact = TraceMessageStyle::Compact;
constexpr std::uint64_t wideValue = 0x100000001ULL;

struct MessageCase {
  TraceMessage message;
  TraceIssueCode code;
  const char* detailed;
  const char* compact;
};
} // namespace

TEST(TraceMessagesTests, PayloadsPreserveDetailedTextAndFormatTypedCompactParameters)
{
  const std::vector<MessageCase> cases{
      {TraceCounterPayload{TraceCounterKind::Dwt, 2U, 0x81U}, TraceIssueCode::UnsupportedDwtEventCounterPayload,
       "unsupported DWT event-counter payload: size 2, value 0x81; expected a non-zero 1-byte mask using bits 0..5 only",
       "Invalid DWT counter: 2 B; 0x81"},
      {TraceCounterPayload{TraceCounterKind::Pmu, 4U, 0x80000000U}, TraceIssueCode::UnsupportedPmuEventCounterPayload,
       "unsupported PMU event-counter payload: size 4, value 0x80000000; expected a non-zero 1-byte mask using bits 0..7",
       "Invalid PMU counter: 4 B; 0x80000000"},
      {TraceInvalidExceptionAction{511U}, TraceIssueCode::InvalidExceptionAction,
       "invalid exception action 0x0 for exception 511", "Invalid action 0x0: exception 511"},
      {TracePcSamplePayload{2U, 255U}, TraceIssueCode::UnsupportedDwtPcSamplePayload,
       "unsupported DWT PC-sample payload: size 2, value 255; expected a 4-byte PC or a 1-byte marker (0x00: CPU Sleeping, 0xff: Trace prohibited)",
       "Invalid PC sample: 2 B; 0xff"},
      {TraceAddressPayload{TraceAddressKind::DataAddress, 3U}, TraceIssueCode::UnsupportedDwtAddressPayload,
       "unsupported DWT data address payload size 3; expected 1, 2, or 4 bytes", "Invalid DWT data address: 3 B"},
      {TraceAddressPayload{TraceAddressKind::PcOrMatch, 0U}, TraceIssueCode::UnsupportedDwtAddressPayload,
       "unsupported DWT PC or match payload size 0; expected 1, 2, or 4 bytes", "Invalid DWT PC or match: 0 B"},
      {TraceRecovery{}, TraceIssueCode::DataLoss,
       "data loss/resync boundary; timestamps across this point may not match", "Data loss; timestamp discontinuity"},
      {TraceRecovery{TraceRecoveryKind::Consumed, 0U, 0U, wideValue}, TraceIssueCode::DataLoss,
       "OpenCSD consumed 4294967297 raw bytes while waiting for usable ITM trace packets; data loss until a later sync/recovery point",
       "4294967297 raw bytes without usable ITM packets"},
      {TraceRecovery{TraceRecoveryKind::Resumed, wideValue, wideValue + 9U, 9U}, TraceIssueCode::DataLoss,
       "ITM decoding resumed at hardware SYNC at raw offset 4294967306; affected raw interval [4294967297, 4294967306) spans 9 bytes",
       "ITM resynced; raw span [4294967297,4294967306): 9 bytes"},
      {TraceRecovery{TraceRecoveryKind::Unresolved, wideValue, wideValue + 9U, 9U}, TraceIssueCode::DataLoss,
       "ITM decoding did not resume before end of input at raw offset 4294967306; no later hardware SYNC; affected raw interval [4294967297, 4294967306) spans 9 bytes",
       "No ITM resync before EOF; raw span [4294967297,4294967306): 9 bytes"},
      {TracePacketDiagnostic{TracePacketDiagnosticKind::Reserved, {}}, TraceIssueCode::OpenCsdDecodeError,
       "Reserved ITM packet", "Reserved ITM packet"},
      {TracePacketDiagnostic{TracePacketDiagnosticKind::BadSequence, {}}, TraceIssueCode::OpenCsdDecodeError,
       "Bad ITM packet sequence", "Invalid ITM packet sequence"},
      {TracePacketDiagnostic{TracePacketDiagnosticKind::Incomplete, wideValue}, TraceIssueCode::OpenCsdIncompleteTail,
       "incomplete ITM packet at end of input at raw offset 4294967297", "Incomplete ITM packet at EOF"},
      {TraceMissingSync{wideValue}, TraceIssueCode::OpenCsdMissingSync,
       "no hardware ITM SYNC before end of input; first formatter group at raw offset 4294967297", "No ITM SYNC before EOF"},
      {TraceMissingSync{}, TraceIssueCode::OpenCsdMissingSync,
       "no hardware ITM SYNC before end of input", "No ITM SYNC before EOF"},
      {TraceInvalidFormattedChunk{}, TraceIssueCode::OpenCsdDecodeError,
       "formatted raw trace chunk is not a multiple of 16 bytes", "Invalid formatted chunk size"},
      {TraceFormattedSessionFailure{"foreign failure", wideValue}, TraceIssueCode::OpenCsdDecodeError,
       "formatted OpenCSD session operation failed: foreign failure at raw input offset 4294967297", "OpenCSD session operation failed"},
      {TraceInitializationFailure{"foreign initialization failure"}, TraceIssueCode::OpenCsdInitializationError,
       "foreign initialization failure", "OpenCSD initialization failed"},
  };
  for (const auto& test : cases) {
    SCOPED_TRACE(test.detailed);
    EXPECT_EQ(formatTraceMessage(test.message, detailed), test.detailed);
    EXPECT_EQ(formatTraceMessage(test.message, compact), test.compact);
    const auto issue = makeTraceIssue(test.message, TraceIssueSeverity::Warning);
    EXPECT_EQ(issue.code, test.code);
    EXPECT_EQ(issue.severity, TraceIssueSeverity::Warning);
  }
}

TEST(TraceMessagesTests, ProgressAndTimeoutCausesRemainDistinct)
{
  const std::array<std::pair<TraceProgressKind, const char*>, 6U> progress{{
      {TraceProgressKind::ConsumedTooMany, "Invalid consumed byte count"},
      {TraceProgressKind::PartialFormatterFrame, "Stopped inside formatter frame"},
      {TraceProgressKind::FormattedInput, "No formatted decode progress"},
      {TraceProgressKind::FormattedDrain, "No decode progress after formatted drain"},
      {TraceProgressKind::Retry, "No decode progress after retry"},
      {TraceProgressKind::ResetForSync, "No decode progress; reset for ITM SYNC"},
  }};
  for (const auto& [kind, text] : progress) {
    const TraceMessage failure = TraceProgressFailure{kind};
    const TraceMessage aborted = TraceProgressFailure{kind, true};
    EXPECT_EQ(formatTraceMessage(failure, compact), text);
    EXPECT_EQ(formatTraceMessage(aborted, compact), text);
    EXPECT_EQ(formatTraceMessage(aborted, detailed), formatTraceMessage(failure, detailed) + "; decode aborted");
    EXPECT_EQ(makeTraceIssue(failure).code, TraceIssueCode::OpenCsdNoProgress);
  }
  for (const auto phase : {TraceFlushPhase::Wait, TraceFlushPhase::FormattedDrain}) {
    const std::string name = phase == TraceFlushPhase::Wait ? "WAIT" : "formatted drain";
    const TraceMessage message = TraceFlushTimeout{phase, wideValue, true};
    EXPECT_EQ(formatTraceMessage(message, detailed),
              "OpenCSD " + name + " did not clear after 4294967297 FLUSH operations; decode aborted");
    EXPECT_EQ(formatTraceMessage(message, compact), "OpenCSD " + name + " timeout: 4294967297 flushes");
    EXPECT_EQ(makeTraceIssue(message).code, TraceIssueCode::OpenCsdWaitTimeout);
  }
}

TEST(TraceMessagesTests, NativeCodesAndPacketPreviewsAreSeparatedFromOpaqueText)
{
  const std::vector<std::pair<TraceNativeCategory, std::string>> categories{
      {TraceNativeCategory::Error, "OpenCSD error"}, {TraceNativeCategory::Warning, "OpenCSD warning"},
      {TraceNativeCategory::BadPacketSequence, "Invalid ITM packet sequence"},
      {TraceNativeCategory::InvalidPacketHeader, "Invalid ITM packet header"},
      {TraceNativeCategory::NotInitialized, "OpenCSD not initialized"},
      {TraceNativeCategory::OutOfMemory, "OpenCSD out of memory"},
      {TraceNativeCategory::InvalidParameter, "Invalid OpenCSD parameter"},
      {TraceNativeCategory::InputRead, "OpenCSD input read failed"},
      {TraceNativeCategory::DecodeFailed, "OpenCSD decode failed"},
      {TraceNativeCategory::InvalidOperation, "Invalid OpenCSD operation"},
      {TraceNativeCategory::InvalidData, "Invalid OpenCSD trace data"},
      {TraceNativeCategory::SystemError, "OpenCSD system error"},
  };
  for (const auto& [category, text] : categories) {
    TraceNativeDiagnostic native{category, 42, {}, "native detail, with punctuation", wideValue};
    TraceMessage message = native;
    message.packet = TracePacketContext{TracePacketKind::Dwt, 2U, std::vector<std::uint8_t>{0x00U, 0xffU}};
    const auto issue = makeTraceIssue(message);
    EXPECT_EQ(formatTraceIssue(issue, wideValue, compact), text + " (code 42); raw@4294967297");
    EXPECT_NE(formatTraceIssue(issue, wideValue, detailed).find(" at raw offset 4294967297. native detail, with punctuation"),
              std::string::npos);
    EXPECT_NE(formatTraceIssue(issue, wideValue, detailed).find("; packet=DWT, size=2 bytes, bytes=[00 ff]"),
              std::string::npos);
    const auto expectedCode = category == TraceNativeCategory::BadPacketSequence ? TraceIssueCode::OpenCsdBadPacketSequence
                           : category == TraceNativeCategory::InvalidPacketHeader ? TraceIssueCode::OpenCsdInvalidPacketHeader
                           : TraceIssueCode::OpenCsdDecodeError;
    EXPECT_EQ(issue.code, expectedCode);
    native.errorCode.reset();
    native.responseCode = 7;
    native.offset = 0U;
    EXPECT_EQ(formatTraceMessage(native, compact), text + " (response 7)");
    EXPECT_NE(formatTraceMessage(native, detailed).find(" at raw offset 0. "), std::string::npos);
  }
  const TraceMessage warning = TraceNativeDiagnostic{TraceNativeCategory::Error, 42, {}, "warning detail", {}, true};
  EXPECT_EQ(formatTraceMessage(warning, detailed), "OpenCSD decoder error. warning detail");
  EXPECT_EQ(formatTraceMessage(warning, compact), "OpenCSD warning (code 42)");
  TraceMessage trailingSeparator = TraceNativeDiagnostic{TraceNativeCategory::Error, 42, {}, "native detail;", {}};
  trailingSeparator.packet = TracePacketContext{TracePacketKind::Reserved, 0U, {}};
  EXPECT_EQ(formatTraceMessage(trailingSeparator, detailed),
            "OpenCSD decoder error. native detail; packet=RESERVED, size=0 bytes, bytes=[]");
}

TEST(TraceMessagesTests, PacketPreviewHandlesAbsentEmptyAndTruncatedBytes)
{
  EXPECT_EQ(formatTracePacketContext({TracePacketKind::Incomplete, 3U, {}}),
            "packet=INCOMPLETE_EOT, size=3 bytes, bytes=unavailable");
  EXPECT_EQ(formatTracePacketContext({TracePacketKind::Reserved, 0U, {}}),
            "packet=RESERVED, size=0 bytes, bytes=[]");
  EXPECT_EQ(formatTracePacketContext({TracePacketKind::BadSequence, 1U, std::vector<std::uint8_t>{0x0aU}}),
            "packet=BAD_SEQUENCE, size=1 byte, bytes=[0a]");
  EXPECT_EQ(formatTracePacketContext({TracePacketKind::Software, 20U, std::vector<std::uint8_t>(20U, 0xabU)}),
            "packet=SWIT, size=20 bytes, bytes=[ab ab ab ab ab ab ab ab ab ab ab ab ab ab ab ab ... (truncated)]");
}

TEST(TraceMessagesTests, TimestampRangesAndRecoverySpansRetainTheirDifferentUnits)
{
  auto issue = makeTraceIssue(TraceRecovery{TraceRecoveryKind::Resumed, wideValue, wideValue + 16U, 16U});
  issue.message.timestampRange = TraceTimestampRange{wideValue, std::nullopt};
  EXPECT_EQ(formatTraceIssue(issue, wideValue, compact),
            "ITM resynced; raw span [4294967297,4294967313): 16 bytes; cycles 4294967297..?");
  EXPECT_EQ(formatTraceIssue(issue, wideValue, detailed),
            "ITM decoding resumed at hardware SYNC at raw offset 4294967313; affected raw interval [4294967297, 4294967313) spans 16 bytes; timestamp 4294967297 .. unknown.");
  issue.message.timestampRange->firstResumed = wideValue + 3U;
  EXPECT_EQ(formatTraceIssue(issue, wideValue, compact),
            "ITM resynced; raw span [4294967297,4294967313): 16 bytes; cycles 4294967297..4294967300");
  auto consumed = makeTraceIssue(TraceRecovery{TraceRecoveryKind::Consumed, wideValue, 0U, 9U});
  EXPECT_EQ(formatTraceIssue(consumed, wideValue, compact), "9 raw bytes without usable ITM packets; raw@4294967297");
}

TEST(TraceMessagesTests, ByteSkipsKeepCountsAndReasonsButNoCsvOffsets)
{
  struct SkipCase { TraceByteSkipReason reason; const char* detailed; const char* compact; };
  const std::array<SkipCase, 5U> cases{{
      {TraceByteSkipReason::NoSourceId, "due to missing source ID", "no source ID"},
      {TraceByteSkipReason::NullSourceId, "for null source ID 0", "null source ID"},
      {TraceByteSkipReason::ReservedSourceId, "for reserved source ID 112", "reserved source ID"},
      {TraceByteSkipReason::UnconfiguredSourceId, "for unconfigured source ID 112", "unconfigured source ID"},
      {TraceByteSkipReason::MissingSync, "due to missing SYNC", "no SYNC"},
  }};
  for (const auto& test : cases) {
    const TraceByteSkip skipped{wideValue, wideValue + 1U, test.reason, 112U};
    EXPECT_EQ(formatTraceMessage(skipped, detailed), "4294967298 bytes skipped " + std::string(test.detailed) +
              "; first formatter group at raw offset 4294967297");
    EXPECT_EQ(formatTraceMessage(skipped, compact), "4294967298 bytes skipped: " + std::string(test.compact));
  }
  EXPECT_EQ(formatTraceMessage(TraceByteSkip{0U, 2U, TraceByteSkipReason::ReservedSourceId, {}}, detailed),
            "2 bytes skipped for reserved source ID unknown; first formatter group at raw offset 0");
}

TEST(TraceMessagesTests, AbortKeepsTypedCauseAndPhaseWhileOpaqueReasonsStayDetailed)
{
  const std::array<std::pair<TraceAbortPhase, const char*>, 7U> prefixes{{
      {TraceAbortPhase::None, ""}, {TraceAbortPhase::Decode, "OpenCSD aborted decode: "},
      {TraceAbortPhase::EndOfTrace, "OpenCSD aborted end-of-trace processing: "},
      {TraceAbortPhase::WaitFlush, "OpenCSD aborted while flushing a WAIT response: "},
      {TraceAbortPhase::FormattedDrain, "OpenCSD aborted while draining formatted trace: "},
      {TraceAbortPhase::DecoderReset, "OpenCSD decoder reset failed: "},
      {TraceAbortPhase::RouteReset, "OpenCSD route-local decoder reset failed: "},
  }};
  for (const auto& [phase, prefix] : prefixes) {
    TraceDecodeAbort failure{wideValue, TraceInvalidFormattedChunk{}};
    failure.reason.phase = phase;
    EXPECT_EQ(formatTraceMessage(failure, detailed),
              "decode aborted after processing 4294967297 input bytes; trace is incomplete: " + std::string(prefix) +
              "formatted raw trace chunk is not a multiple of 16 bytes");
    EXPECT_NE(formatTraceMessage(failure, compact).find("Invalid formatted chunk size"), std::string::npos);
  }
  const TraceDecodeAbort opaque{wideValue, "foreign text, never parsed"};
  EXPECT_EQ(formatTraceMessage(opaque, compact), "Decode aborted after 4294967297 bytes; trace incomplete");
  EXPECT_EQ(formatTraceMessage(opaque, detailed),
            "decode aborted after processing 4294967297 input bytes; trace is incomplete: foreign text, never parsed");
}

TEST(TraceMessagesTests, SetupFailuresCarryNativeDescriptionsAndCompactCodes)
{
  const TraceMessage setup = TraceSetupFailure{TraceSetupOperation::CreateDecoder};
  EXPECT_EQ(formatTraceMessage(setup, detailed), "failed to create OpenCSD decoder");
  EXPECT_EQ(formatTraceMessage(setup, compact), "OpenCSD initialization failed");
  const TraceMessage native = TraceSetupFailure{TraceSetupOperation::CreateDecoder, "native API error: detail", 9};
  EXPECT_EQ(formatTraceMessage(native, detailed), "native API error: detail");
  EXPECT_EQ(formatTraceMessage(native, compact), "OpenCSD initialization failed (code 9)");
  EXPECT_EQ(makeTraceIssue(native).code, TraceIssueCode::OpenCsdInitializationError);
}

TEST(TraceMessagesTests, FormattedSessionFailureRetainsItsTypedSetupCause)
{
  const TraceSetupFailure setup{TraceSetupOperation::ResolveDecoderInput, "native API error: detail", 9};
  const TraceMessage failure = TraceFormattedSessionFailure{"", wideValue, setup};
  EXPECT_EQ(formatTraceMessage(failure, detailed),
            "formatted OpenCSD session operation failed: native API error: detail at raw input offset 4294967297");
  EXPECT_EQ(formatTraceMessage(failure, compact), "OpenCSD session operation failed (code 9)");
  EXPECT_EQ(makeTraceIssue(failure).code, TraceIssueCode::OpenCsdDecodeError);
  const TraceMessage missingObject = TraceFormattedSessionFailure{
      "", 0U, TraceSetupFailure{TraceSetupOperation::DecoderInput}};
  EXPECT_EQ(formatTraceMessage(missingObject, detailed),
            "formatted OpenCSD session operation failed: OpenCSD decoder input is not initialized at raw input offset 0");
  EXPECT_EQ(formatTraceMessage(missingObject, compact), "OpenCSD session operation failed");
}

TEST(TraceMessagesTests, OpaqueAndEmptyIssuesUseSemanticFallbacksWithoutInventingParameters)
{
  static_assert(!std::is_convertible_v<TraceMessage, std::string>, "messages require explicit output formatting");
  TraceIssueEvent issue;
  EXPECT_TRUE(issue.message.empty());
  EXPECT_EQ(formatTraceIssue(issue, wideValue, detailed), "trace decode error at raw offset 4294967297");
  EXPECT_EQ(formatTraceIssue(issue, wideValue, compact), "Trace decode error; raw@4294967297");
  issue.code = TraceIssueCode::UnsupportedDwtPcSamplePayload;
  issue.message = "arbitrary native text";
  EXPECT_FALSE(issue.message.empty());
  EXPECT_EQ(formatTraceIssue(issue, wideValue, detailed), "arbitrary native text");
  EXPECT_EQ(formatTraceIssue(issue, wideValue, compact), "Invalid PC sample; raw@4294967297");
  issue.message = {};
  issue.code = TraceIssueCode::DataLoss;
  EXPECT_EQ(formatTraceIssue(issue, wideValue, detailed),
            "trace data at raw offset 4294967297 could not be decoded before the next hardware ITM sync");
  EXPECT_EQ(formatTraceIssue(issue, wideValue, compact), "ITM data loss; raw@4294967297");
  issue.rawBytesConsumed = wideValue;
  EXPECT_EQ(formatTraceIssue(issue, wideValue, detailed),
            "4294967297 raw bytes from raw offset 4294967297 could not be decoded before the next hardware ITM sync");
  EXPECT_EQ(formatTraceIssue(issue, wideValue, compact), "ITM data loss; 4294967297 raw bytes affected; raw@4294967297");
}

TEST(TraceMessagesTests, OverflowEventAndSummaryKeepSeparateMeanings)
{
  EXPECT_EQ(formatTraceMessage(OverflowTraceEvent{}, detailed),
            "overflow: new timestamp segment; time across boundary may be unreliable");
  EXPECT_EQ(formatTraceMessage(OverflowTraceEvent{"custom overflow detail"}, detailed), "custom overflow detail");
  EXPECT_EQ(formatTraceMessage(OverflowTraceEvent{"custom overflow detail"}, compact), "Timestamp discontinuity");
  EXPECT_EQ(formatOverflowSummary({{}, 1U}), "first overflow occurred at an unknown cycle timestamp");
  EXPECT_EQ(formatOverflowSummary({wideValue, 4U}), "first overflow occurred at cycle timestamp 4294967297; 3 more occurred");
}

TEST(TraceMessagesTests, EmptyInitializationDetailKeepsIssueFallbackAndOriginalAbortDetail)
{
  const TraceMessage message = TraceInitializationFailure{""};
  const auto issue = makeTraceIssue(message);
  EXPECT_EQ(formatTraceIssue(issue, wideValue, detailed), "OpenCSD initialization failed");
  EXPECT_EQ(formatTraceIssue(issue, wideValue, compact), "OpenCSD initialization failed; raw@4294967297");
  EXPECT_EQ(formatTraceMessage(TraceDecodeAbort{0U, message}, detailed),
            "decode aborted after processing 0 input bytes; trace is incomplete: ");
  EXPECT_EQ(formatTraceMessage(TraceDecodeAbort{0U, message}, compact),
            "Decode aborted after 0 bytes; trace incomplete; OpenCSD initialization failed");
}

TEST(TraceMessagesTests, SemanticFallbacksKeepCategoriesWithoutInventingPayloadParameters)
{
  const std::vector<std::pair<TraceIssueCode, const char*>> cases{
      {TraceIssueCode::DecodeError, "Trace decode error"},
      {TraceIssueCode::InvalidExceptionAction, "Invalid exception action"},
      {TraceIssueCode::UnsupportedDwtEventCounterPayload, "Invalid DWT counter"},
      {TraceIssueCode::UnsupportedPmuEventCounterPayload, "Invalid PMU counter"},
      {TraceIssueCode::UnsupportedDwtAddressPayload, "Invalid DWT address payload"},
      {TraceIssueCode::UnsupportedDwtPcSamplePayload, "Invalid PC sample"},
      {TraceIssueCode::OpenCsdBadPacketSequence, "Invalid ITM packet sequence"},
      {TraceIssueCode::OpenCsdInvalidPacketHeader, "Invalid ITM packet header"},
      {TraceIssueCode::OpenCsdIncompleteTail, "Incomplete ITM packet at EOF"},
      {TraceIssueCode::OpenCsdMissingSync, "No ITM SYNC before EOF"},
      {TraceIssueCode::OpenCsdNoProgress, "No decode progress"},
      {TraceIssueCode::OpenCsdWaitTimeout, "OpenCSD flush timeout"},
      {TraceIssueCode::OpenCsdInitializationError, "OpenCSD initialization failed"},
      {TraceIssueCode::OpenCsdDecodeError, "OpenCSD error"},
  };
  for (const auto& [code, expected] : cases) {
    const TraceIssueEvent issue{code, TraceIssueSeverity::Error, "foreign {0} text"};
    EXPECT_EQ(formatTraceIssue(issue, wideValue, compact), std::string(expected) + "; raw@4294967297");
    EXPECT_EQ(formatTraceIssue(issue, wideValue, detailed), "foreign {0} text");
  }
  const TraceIssueEvent warning{TraceIssueCode::OpenCsdDecodeError, TraceIssueSeverity::Warning, ""};
  EXPECT_EQ(formatTraceIssue(warning, 0U, compact), "OpenCSD warning; raw@0");
  EXPECT_EQ(formatTraceIssue(warning, 0U, detailed), "trace decode error at raw offset 0");
  EXPECT_EQ(makeTraceIssue(TraceOpaqueMessage{"native detail"}).code, TraceIssueCode::DecodeError);
}

TEST(TraceMessagesTests, InvalidTypedParametersAreRejectedInsteadOfProducingEmptyMessages)
{
  EXPECT_THROW(formatTraceMessage(TraceCounterPayload{static_cast<TraceCounterKind>(99)}, detailed),
               std::invalid_argument);
  EXPECT_THROW(formatTraceMessage(TraceAddressPayload{static_cast<TraceAddressKind>(99)}, compact),
               std::invalid_argument);
  EXPECT_THROW(formatTraceMessage(TraceFlushTimeout{static_cast<TraceFlushPhase>(99)}, detailed),
               std::invalid_argument);
  EXPECT_THROW(formatTraceMessage(TraceRecovery{static_cast<TraceRecoveryKind>(99)}, detailed),
               std::invalid_argument);
  EXPECT_THROW(formatTraceMessage(TraceNativeDiagnostic{static_cast<TraceNativeCategory>(99)}, compact),
               std::invalid_argument);
  EXPECT_THROW(formatTraceMessage(TracePacketDiagnostic{static_cast<TracePacketDiagnosticKind>(99)}, detailed),
               std::invalid_argument);
  EXPECT_THROW(formatTraceMessage(TraceProgressFailure{static_cast<TraceProgressKind>(99)}, detailed),
               std::invalid_argument);
  EXPECT_THROW(formatTraceMessage(TraceByteSkip{0U, 1U, static_cast<TraceByteSkipReason>(99)}, compact),
               std::invalid_argument);
  const auto invalidStyle = static_cast<TraceMessageStyle>(99);
  for (const auto reason : {TraceByteSkipReason::NoSourceId, TraceByteSkipReason::ReservedSourceId,
                           TraceByteSkipReason::UnconfiguredSourceId}) {
    EXPECT_THROW(formatTraceMessage(TraceByteSkip{0U, 1U, reason, 112U}, invalidStyle), std::invalid_argument);
  }
  EXPECT_THROW(formatTraceSetupOperation(static_cast<TraceSetupOperation>(99)), std::invalid_argument);
  EXPECT_THROW(formatTracePacketContext({static_cast<TracePacketKind>(99), 0U, {}}), std::invalid_argument);
  TraceMessage failure = TraceInvalidFormattedChunk{};
  failure.phase = static_cast<TraceAbortPhase>(99);
  EXPECT_THROW(formatTraceMessage(failure, detailed), std::invalid_argument);
}
