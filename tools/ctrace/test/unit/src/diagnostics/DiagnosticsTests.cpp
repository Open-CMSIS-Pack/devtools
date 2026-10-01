/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TestSupport.h"
#include <gtest/gtest.h>
#include "DiagnosticSink.h"
#include "TraceEvent.h"
#include "TraceIssueReporter.h"
#include "TraceRoute.h"
#include <cstddef>
#include <string>

TEST(CtraceUnitTests, testDiagnosticCollection)
{
  CollectingDiagnosticSink sink;
  sink.report({
      DiagnosticSink::Severity::Warning,
      "timestamp discontinuity",
      {{"offset", "42"}},
  });

  ASSERT_TRUE(sink.events().size() == 1) << "diagnostic event count mismatch";
  ASSERT_TRUE(sink.events()[0].severity == DiagnosticSink::Severity::Warning) << "diagnostic severity mismatch";
  ASSERT_TRUE(toString(sink.events()[0].severity) == "warning") << "diagnostic severity text mismatch";
  ASSERT_TRUE(sink.failureCount() == 0U) << "warnings must not fail the command";

  sink.report({
      DiagnosticSink::Severity::Error,
      "generator could not create an irrelevant register",
      {},
      DiagnosticSink::Impact::NonFailing,
  });
  ASSERT_TRUE(sink.events().back().impact == DiagnosticSink::Impact::NonFailing && sink.failureCount() == 0U)
      << "non-failing errors must retain error severity without failing the job";

  sink.report({
      DiagnosticSink::Severity::Error,
      "required input is missing",
  });
  ASSERT_TRUE(sink.events().back().impact == DiagnosticSink::Impact::Failing && sink.failureCount() == 1U)
      << "failing errors must fail the job";
}

TEST(CtraceUnitTests, testDiagnosticTextCoversSeverityAndFormatting)
{
  EXPECT_EQ(toString(DiagnosticSink::Severity::Info), "info");
  EXPECT_EQ(toString(DiagnosticSink::Severity::Warning), "warning");
  EXPECT_EQ(toString(DiagnosticSink::Severity::Error), "error");
  EXPECT_EQ(toString(static_cast<DiagnosticSink::Severity>(99)), "unknown");

  StderrDiagnosticSink sink;
  testing::internal::CaptureStderr();
  sink.report({DiagnosticSink::Severity::Info, "message with context", {{"argument", "--all"}}});
  sink.report({DiagnosticSink::Severity::Error, "plain message", {}, DiagnosticSink::Impact::Failing});
  const auto text = testing::internal::GetCapturedStderr();
  EXPECT_NE(text.find("[info] message with context: argument=--all"), std::string::npos);
  EXPECT_NE(text.find("[error] plain message"), std::string::npos);
  EXPECT_EQ(text.find("cli"), std::string::npos);
  EXPECT_EQ(text.find("output/write"), std::string::npos);
}

TEST(CtraceUnitTests, testDiagnosticVerboseSelectsDetailsAndTechnicalContext)
{
  DiagnosticSink::Event event{DiagnosticSink::Severity::Warning, "Invalid ITM packet sequence", {{"stream", "7"}}};
  event.detailedMessage = "OpenCSD detected an invalid ITM packet sequence; packet data";
  event.detailedContext = {{"raw_offset", "42"}, {"size", "2"}};
  StderrDiagnosticSink sink;

  testing::internal::CaptureStderr();
  sink.report(event);
  EXPECT_EQ(testing::internal::GetCapturedStderr(), "[warning] Invalid ITM packet sequence: stream=7\n");

  sink.setVerbose(true);
  testing::internal::CaptureStderr();
  sink.report(event);
  EXPECT_EQ(testing::internal::GetCapturedStderr(),
            "[warning] OpenCSD detected an invalid ITM packet sequence; packet data: stream=7, raw_offset=42, size=2\n");

  event.context.clear();
  event.detailedMessage.reset();
  testing::internal::CaptureStderr();
  sink.report(event);
  EXPECT_EQ(testing::internal::GetCapturedStderr(),
            "[warning] Invalid ITM packet sequence: raw_offset=42, size=2\n");
  EXPECT_EQ(sink.failureCount(), 0U);
}

TEST(CtraceUnitTests, testDiagnosticVisibilityDoesNotChangeFailureImpact)
{
  DiagnosticSink::Event event{DiagnosticSink::Severity::Info, "processing details", {}, DiagnosticSink::Impact::Failing};
  event.visibility = DiagnosticSink::Visibility::Verbose;
  StderrDiagnosticSink sink;
  testing::internal::CaptureStderr();
  sink.report(event);
  EXPECT_TRUE(testing::internal::GetCapturedStderr().empty());
  EXPECT_EQ(sink.failureCount(), 1U);

  sink.setVerbose(true);
  testing::internal::CaptureStderr();
  sink.report(event);
  sink.report({DiagnosticSink::Severity::Error, "foreign generator diagnostic", {}, DiagnosticSink::Impact::NonFailing});
  EXPECT_EQ(testing::internal::GetCapturedStderr(),
            "[info] processing details\n[error] foreign generator diagnostic\n");
  EXPECT_EQ(sink.failureCount(), 2U);

  sink.setVerbose(false);
  testing::internal::CaptureStderr();
  sink.report(event);
  EXPECT_TRUE(testing::internal::GetCapturedStderr().empty());
  EXPECT_EQ(sink.failureCount(), 3U);
}

TEST(CtraceUnitTests, testTraceIssueReporterReportsEveryIssue)
{
  CollectingDiagnosticSink payloadIndependentDiagnostics;
  TraceIssueReporter payloadIndependentReporter(payloadIndependentDiagnostics);
  TraceEvent payloadIndependentError = issuePacket(TraceIssueCode::OpenCsdIncompleteTail);
  payloadIndependentReporter.append(payloadIndependentError);
  ASSERT_TRUE(payloadIndependentDiagnostics.events().size() == 1U)
      << "decoder errors must remain visible independently of payload filtering";
  ASSERT_TRUE(payloadIndependentDiagnostics.failureCount() == 1U)
      << "decoder errors must fail validation independently of payload filtering";

  CollectingDiagnosticSink diagnostics;
  TraceIssueReporter reporter(diagnostics);

  reporter.append(overflowPacket(10));
  reporter.append(overflowPacket(20));
  reporter.finish();
  reporter.finish();

  TraceEvent dataLoss = issuePacket(TraceIssueCode::DataLoss);
  dataLoss.index = 12U;
  std::get<TraceIssueEvent>(dataLoss.payload).rawBytesConsumed = 3U;
  reporter.append(dataLoss);
  reporter.append(dataLoss);

  TraceEvent warning = issuePacket(TraceIssueCode::OpenCsdDecodeError, "decoder warning", TraceIssueSeverity::Warning);
  reporter.append(warning);

  TraceEvent initializationError = issuePacket(TraceIssueCode::OpenCsdInitializationError, "decoder setup failed");
  reporter.append(initializationError);

  ASSERT_TRUE(diagnostics.events().size() == 5U)
      << "TraceIssueReporter should report every error and warning occurrence";
  ASSERT_TRUE(diagnostics.events()[0].severity == DiagnosticSink::Severity::Warning)
      << "TraceIssueReporter overflow severity mismatch";
  EXPECT_EQ(diagnostics.events()[0].message, "Trace overflow; timestamp discontinuity; 1 more occurred");
  EXPECT_NE(diagnostics.events()[0].detailedMessage.value().find("1 more occurred"), std::string::npos);
  EXPECT_TRUE(diagnostics.events()[0].context.empty());
  ASSERT_TRUE(diagnostics.events()[1].severity == DiagnosticSink::Severity::Error)
      << "TraceIssueReporter data-loss severity mismatch";
  EXPECT_TRUE(diagnostics.events()[1].context.empty());
  EXPECT_TRUE(diagnostics.events()[1].detailedContext.empty());
  ASSERT_TRUE(diagnostics.events()[1].rawLocation.has_value());
  EXPECT_EQ(diagnostics.events()[1].rawLocation->offset, 12U);
  ASSERT_TRUE(diagnostics.events()[1].message.find("3 raw bytes") != std::string::npos)
      << "TraceIssueReporter should include the lost byte count";
  ASSERT_TRUE(diagnostics.events()[2].message == diagnostics.events()[1].message)
      << "TraceIssueReporter repeated data-loss message mismatch";
  ASSERT_TRUE(diagnostics.events()[3].severity == DiagnosticSink::Severity::Warning)
      << "TraceIssueReporter should preserve warning severity";
  EXPECT_EQ(diagnostics.events()[3].message, "OpenCSD warning");
  EXPECT_EQ(diagnostics.events()[4].message, "OpenCSD initialization failed");
  EXPECT_EQ(diagnostics.events()[3].detailedMessage, "decoder warning");
  EXPECT_EQ(diagnostics.events()[4].detailedMessage, "decoder setup failed");
  EXPECT_TRUE(diagnostics.events()[3].detailedContext.empty());
  EXPECT_TRUE(diagnostics.events()[4].detailedContext.empty());
  ASSERT_TRUE(diagnostics.events()[3].rawLocation.has_value());
  ASSERT_TRUE(diagnostics.events()[4].rawLocation.has_value());
  EXPECT_EQ(diagnostics.events()[3].rawLocation->offset, 0U);
  EXPECT_EQ(diagnostics.events()[4].rawLocation->offset, 0U);
  ASSERT_TRUE(diagnostics.failureCount() == 3U) << "TraceIssueReporter should classify decoder errors as failing";
}

TEST(CtraceUnitTests, testTraceIssueReporterFormatsUnknownOverflowTimestamp)
{
  CollectingDiagnosticSink diagnostics;
  TraceIssueReporter reporter(diagnostics);
  reporter.append(TraceEvent{OverflowTraceEvent{"overflow"}});
  reporter.finish();

  ASSERT_EQ(diagnostics.events().size(), 1U);
  EXPECT_EQ(diagnostics.events().front().message, "Trace overflow; timestamp discontinuity");
  ASSERT_TRUE(diagnostics.events().front().detailedMessage.has_value());
  EXPECT_NE(diagnostics.events().front().detailedMessage->find("unknown cycle timestamp"), std::string::npos);
  EXPECT_EQ(diagnostics.events().front().detailedMessage->find("cycle timestamp 0"), std::string::npos);
  EXPECT_EQ(diagnostics.events().front().detailedMessage->find("0 more occurred"), std::string::npos);
  ASSERT_TRUE(diagnostics.events().front().rawLocation.has_value());
  EXPECT_FALSE(diagnostics.events().front().rawLocation->previousSyncOffset.has_value());
  EXPECT_FALSE(diagnostics.events().front().rawLocation->nextSyncOffset.has_value());
  EXPECT_EQ(diagnostics.events().front().detailedContext,
            (std::vector<std::pair<std::string, std::string>>{{"overflow_count", "1"},
                                                           {"first_raw_offset", "0"},
                                                           {"last_raw_offset", "0"},
                                                           {"sample_raw_offsets", "[0]"},
                                                           {"omitted_offsets", "0"}}));
}

TEST(CtraceUnitTests, testTraceIssueReporterPartitionsIssuesAndOverflowByRoute)
{
  CollectingDiagnosticSink diagnostics;
  TraceIssueReporter reporter(diagnostics);
  const TraceRouteIdentity firstRoute{TraceRouteId{10U}, 1U};
  const TraceRouteIdentity secondRoute{TraceRouteId{20U}, 111U};
  const TraceRouteIdentity noBusA{TraceRouteId{30U}, std::nullopt};
  const TraceRouteIdentity noBusB{TraceRouteId{31U}, std::nullopt};

  reporter.append(onRoute(overflowPacket(10U), firstRoute));
  reporter.append(onRoute(overflowPacket(100U), secondRoute));
  reporter.append(onRoute(overflowPacket(20U), firstRoute));
  reporter.append(onRoute(issuePacket(TraceIssueCode::DecodeError), firstRoute));
  reporter.append(onRoute(issuePacket(TraceIssueCode::OpenCsdDecodeError, "route warning", TraceIssueSeverity::Warning),
                          secondRoute));
  reporter.append(onRoute(overflowPacket(30U), noBusA));
  reporter.append(onRoute(overflowPacket(31U), noBusB));
  reporter.finish();

  ASSERT_EQ(diagnostics.events().size(), 6U);
  EXPECT_EQ(diagnostics.events()[0].context,
            (std::vector<std::pair<std::string, std::string>>{{"stream", "1"}}));
  EXPECT_EQ(diagnostics.events()[1].context,
            (std::vector<std::pair<std::string, std::string>>{{"stream", "111"}}));
  EXPECT_TRUE(diagnostics.events()[0].detailedContext.empty());
  EXPECT_EQ(diagnostics.events()[1].detailedContext, diagnostics.events()[0].detailedContext);
  ASSERT_TRUE(diagnostics.events()[0].rawLocation.has_value());
  ASSERT_TRUE(diagnostics.events()[1].rawLocation.has_value());
  EXPECT_EQ(diagnostics.events()[0].rawLocation->offset, 0U);
  EXPECT_EQ(diagnostics.events()[1].rawLocation->offset, 0U);
  EXPECT_EQ(diagnostics.events()[2].context, (std::vector<std::pair<std::string, std::string>>{{"stream", "1"}}));
  EXPECT_NE(diagnostics.events()[2].detailedMessage.value().find("cycle timestamp 10; 1 more occurred"), std::string::npos);
  EXPECT_EQ(diagnostics.events()[3].context, (std::vector<std::pair<std::string, std::string>>{{"stream", "111"}}));
  EXPECT_NE(diagnostics.events()[3].detailedMessage.value().find("cycle timestamp 100"), std::string::npos);
  EXPECT_TRUE(diagnostics.events()[4].context.empty());
  EXPECT_TRUE(diagnostics.events()[5].context.empty());
  EXPECT_NE(diagnostics.events()[4].detailedMessage.value().find("cycle timestamp 30"), std::string::npos);
  EXPECT_NE(diagnostics.events()[5].detailedMessage.value().find("cycle timestamp 31"), std::string::npos)
      << "distinct no-bus route IDs must not collapse into one overflow summary";
}

TEST(CtraceUnitTests, testTraceIssueReporterFormatsEveryErrorKind)
{
  CollectingDiagnosticSink diagnostics;
  TraceIssueReporter reporter(diagnostics);

  /** @brief Describes one diagnostic formatting test case. */
  struct Case {
    TraceIssueCode code;
    const char* message;
    const char* compact;
  };
  constexpr Case cases[]{
      {TraceIssueCode::OpenCsdBadPacketSequence, "invalid ITM packet sequence at raw offset 42", "Invalid ITM packet sequence"},
      {TraceIssueCode::OpenCsdInvalidPacketHeader, "invalid ITM packet header at raw offset 42", "Invalid ITM packet header"},
      {TraceIssueCode::OpenCsdIncompleteTail, "incomplete ITM packet starting at raw offset 42 at end of input", "Incomplete ITM packet at EOF"},
      {TraceIssueCode::OpenCsdMissingSync, "no hardware ITM SYNC before end of input", "No ITM SYNC before EOF"},
      {TraceIssueCode::OpenCsdNoProgress, "OpenCSD made no decode progress at raw offset 42", "No decode progress"},
      {TraceIssueCode::OpenCsdWaitTimeout, "OpenCSD remained blocked while flushing pending data", "OpenCSD flush timeout"},
      {TraceIssueCode::OpenCsdInitializationError, "OpenCSD initialization failed", "OpenCSD initialization failed"},
      {TraceIssueCode::DecodeError, "trace decode error at raw offset 42", "Trace decode error"},
      {static_cast<TraceIssueCode>(255U), "trace decode error at raw offset 42", "Trace decode error"},
  };
  for (const auto& testCase : cases) {
    auto event = issuePacket(testCase.code);
    event.index = 42U;
    reporter.append(event);
  }

  auto dataLoss = issuePacket(TraceIssueCode::DataLoss);
  dataLoss.index = 43U;
  reporter.append(dataLoss);

  auto warningDataLoss = issuePacket(TraceIssueCode::DataLoss, "warning loss", TraceIssueSeverity::Warning);
  reporter.append(warningDataLoss);
  reporter.append(issuePacket(TraceIssueCode::DecodeError));

  ASSERT_EQ(diagnostics.events().size(), std::size(cases) + 3U);
  for (std::size_t index = 0U; index < std::size(cases); ++index) {
    EXPECT_EQ(diagnostics.events()[index].message, cases[index].compact);
    EXPECT_EQ(diagnostics.events()[index].detailedMessage, cases[index].message);
    EXPECT_TRUE(diagnostics.events()[index].context.empty());
    EXPECT_TRUE(diagnostics.events()[index].detailedContext.empty());
    ASSERT_TRUE(diagnostics.events()[index].rawLocation.has_value());
    EXPECT_EQ(diagnostics.events()[index].rawLocation->offset, 42U);
  }
  EXPECT_EQ(diagnostics.events()[std::size(cases)].message, "ITM data loss");
  EXPECT_NE(diagnostics.events()[std::size(cases)].detailedMessage.value().find("raw offset 43"), std::string::npos);
  EXPECT_EQ(diagnostics.events()[std::size(cases) + 1U].severity, DiagnosticSink::Severity::Warning);
  EXPECT_EQ(diagnostics.events()[std::size(cases) + 1U].detailedMessage, "warning loss");
  EXPECT_EQ(diagnostics.events().back().message, "Trace decode error");
  EXPECT_EQ(diagnostics.events().back().detailedMessage, "trace decode error at raw offset 0");
}

TEST(CtraceUnitTests, testTraceIssueReporterPreservesMissingSyncDetailsAndFailure)
{
  CollectingDiagnosticSink diagnostics;
  TraceIssueReporter reporter(diagnostics);
  const std::string message = "no hardware ITM SYNC before end of input; "
                              "first formatter group at raw offset 32";
  auto event = onStream(issuePacket(TraceIssueCode::OpenCsdMissingSync, message), 1U);
  event.index = 32U;
  reporter.append(event);

  ASSERT_EQ(diagnostics.events().size(), 1U);
  EXPECT_EQ(diagnostics.events().front().message, "No ITM SYNC before EOF");
  EXPECT_EQ(diagnostics.events().front().detailedMessage, message);
  EXPECT_EQ(diagnostics.events().front().severity, DiagnosticSink::Severity::Error);
  EXPECT_EQ(diagnostics.events().front().impact, DiagnosticSink::Impact::Failing);
  EXPECT_TRUE(diagnostics.containsContext("stream", "1"));
  EXPECT_TRUE(diagnostics.events().front().detailedContext.empty());
  ASSERT_TRUE(diagnostics.events().front().rawLocation.has_value());
  EXPECT_EQ(diagnostics.events().front().rawLocation->offset, 32U);
  EXPECT_EQ(diagnostics.failureCount(), 1U);
}

TEST(CtraceUnitTests, testTraceIssueReporterPreservesDetailsForEveryIssueKind)
{
  constexpr TraceIssueCode codes[]{
      TraceIssueCode::DecodeError,
      TraceIssueCode::DataLoss,
      TraceIssueCode::InvalidExceptionAction,
      TraceIssueCode::UnsupportedDwtEventCounterPayload,
      TraceIssueCode::UnsupportedPmuEventCounterPayload,
      TraceIssueCode::UnsupportedDwtAddressPayload,
      TraceIssueCode::UnsupportedDwtPcSamplePayload,
      TraceIssueCode::OpenCsdDecodeError,
      TraceIssueCode::OpenCsdBadPacketSequence,
      TraceIssueCode::OpenCsdInvalidPacketHeader,
      TraceIssueCode::OpenCsdIncompleteTail,
      TraceIssueCode::OpenCsdMissingSync,
      TraceIssueCode::OpenCsdNoProgress,
      TraceIssueCode::OpenCsdWaitTimeout,
      TraceIssueCode::OpenCsdInitializationError,
      static_cast<TraceIssueCode>(255U),
  };
  CollectingDiagnosticSink diagnostics;
  TraceIssueReporter reporter(diagnostics);
  const std::string detail = "OpenCSD failed at raw offset 42. OCSD_ERR_BAD_PACKET_SEQ; invalid async sequence";
  for (const auto code : codes) {
    auto event = onStream(issuePacket(code, detail), 111U);
    event.index = 42U;
    std::get<TraceIssueEvent>(event.payload).rawBytesConsumed = 9U;
    reporter.append(event);
  }

  ASSERT_EQ(diagnostics.events().size(), std::size(codes));
  for (const auto& diagnostic : diagnostics.events()) {
    EXPECT_EQ(diagnostic.detailedMessage, detail);
    EXPECT_NE(diagnostic.message, detail);
    EXPECT_EQ(diagnostic.message.find("raw offset"), std::string::npos);
    EXPECT_EQ(diagnostic.severity, DiagnosticSink::Severity::Error);
    EXPECT_EQ(diagnostic.impact, DiagnosticSink::Impact::Failing);
    EXPECT_EQ(diagnostic.context,
              (std::vector<std::pair<std::string, std::string>>{{"stream", "111"}}));
    EXPECT_TRUE(diagnostic.detailedContext.empty());
    ASSERT_TRUE(diagnostic.rawLocation.has_value());
    EXPECT_EQ(diagnostic.rawLocation->offset, 42U);
  }
  EXPECT_EQ(diagnostics.failureCount(), std::size(codes));
}

TEST(CtraceUnitTests, testTraceIssueReporterKeepsRawOffsetsAlongsidePayloadDetails)
{
  CollectingDiagnosticSink diagnostics;
  TraceIssueReporter reporter(diagnostics);
  const std::string detail = "unsupported DWT event counter payload size 2";
  auto error = issuePacket(TraceIssueCode::UnsupportedDwtEventCounterPayload, detail);
  error.index = 0x10000002aULL;
  reporter.append(error);

  const std::string warningDetail = "unsupported PMU event counter payload size 1";
  auto warning = onStream(issuePacket(TraceIssueCode::UnsupportedPmuEventCounterPayload, warningDetail,
                                       TraceIssueSeverity::Warning),
                            111U);
  warning.index = 73U;
  reporter.append(warning);

  ASSERT_EQ(diagnostics.events().size(), 2U);
  EXPECT_EQ(diagnostics.events()[0].message, "Invalid DWT counter");
  EXPECT_EQ(diagnostics.events()[0].detailedMessage, detail);
  EXPECT_TRUE(diagnostics.events()[0].context.empty());
  EXPECT_TRUE(diagnostics.events()[0].detailedContext.empty());
  ASSERT_TRUE(diagnostics.events()[0].rawLocation.has_value());
  EXPECT_EQ(diagnostics.events()[0].rawLocation->offset, 0x10000002aULL);
  EXPECT_EQ(diagnostics.events()[0].severity, DiagnosticSink::Severity::Error);
  EXPECT_EQ(diagnostics.events()[1].message, "Invalid PMU counter");
  EXPECT_EQ(diagnostics.events()[1].detailedMessage, warningDetail);
  EXPECT_EQ(diagnostics.events()[1].context,
            (std::vector<std::pair<std::string, std::string>>{{"stream", "111"}}));
  EXPECT_TRUE(diagnostics.events()[1].detailedContext.empty());
  ASSERT_TRUE(diagnostics.events()[1].rawLocation.has_value());
  EXPECT_EQ(diagnostics.events()[1].rawLocation->offset, 73U);
  EXPECT_EQ(diagnostics.events()[1].severity, DiagnosticSink::Severity::Warning);
  EXPECT_EQ(diagnostics.failureCount(), 1U);
}

TEST(CtraceUnitTests, testTraceIssueReporterRetainsRouteLocalSyncAndBoundedOverflowLocations)
{
  CollectingDiagnosticSink diagnostics;
  TraceIssueReporter reporter(diagnostics);
  const TraceRouteIdentity firstRoute{TraceRouteId{10U}, 1U};
  const TraceRouteIdentity secondRoute{TraceRouteId{20U}, 111U};
  const auto appendAt = [&](TraceEvent event, const TraceRouteIdentity& route, std::uint64_t offset) {
    event.route = route;
    event.index = offset;
    reporter.append(event);
  };

  appendAt(TraceEvent{SyncTraceEvent{}}, firstRoute, 4U);
  appendAt(TraceEvent{SyncTraceEvent{}}, secondRoute, 6U);
  appendAt(overflowPacket(100U), firstRoute, 10U);
  appendAt(issuePacket(TraceIssueCode::OpenCsdBadPacketSequence), firstRoute, 11U);
  appendAt(overflowPacket(120U), secondRoute, 12U);
  appendAt(TraceEvent{SyncTraceEvent{}}, secondRoute, 14U);
  appendAt(issuePacket(TraceIssueCode::OpenCsdInvalidPacketHeader), secondRoute, 18U);
  appendAt(overflowPacket(200U), firstRoute, 20U);
  appendAt(TraceEvent{SyncTraceEvent{}}, firstRoute, 24U);
  appendAt(overflowPacket(300U), firstRoute, 30U);
  appendAt(TraceEvent{SyncTraceEvent{}}, firstRoute, 40U);
  appendAt(overflowPacket(500U), firstRoute, 50U);
  appendAt(overflowPacket(600U), firstRoute, 60U);
  appendAt(softwarePacket(0U), secondRoute, 62U);
  reporter.finish();

  ASSERT_EQ(diagnostics.events().size(), 4U);
  const auto& firstIssue = diagnostics.events()[0];
  ASSERT_TRUE(firstIssue.rawLocation.has_value());
  EXPECT_EQ(firstIssue.rawLocation->previousSyncOffset, 4U);
  EXPECT_FALSE(firstIssue.rawLocation->nextSyncOffset.has_value())
      << "immediate issues must not invent a future synchronization anchor";
  const auto& secondIssue = diagnostics.events()[1];
  ASSERT_TRUE(secondIssue.rawLocation.has_value());
  EXPECT_EQ(secondIssue.rawLocation->previousSyncOffset, 14U);

  const auto& firstOverflow = diagnostics.events()[2];
  EXPECT_EQ(firstOverflow.message, "Trace overflow; timestamp discontinuity; 4 more occurred");
  EXPECT_EQ(firstOverflow.detailedContext,
            (std::vector<std::pair<std::string, std::string>>{{"overflow_count", "5"},
                                                           {"first_raw_offset", "10"},
                                                           {"last_raw_offset", "60"},
                                                           {"sample_raw_offsets", "[10,20,30]"},
                                                           {"omitted_offsets", "2"}}));
  ASSERT_TRUE(firstOverflow.rawLocation.has_value());
  EXPECT_EQ(firstOverflow.rawLocation->kind, RawDiagnosticLocation::Kind::Decoder);
  EXPECT_EQ(firstOverflow.rawLocation->offset, 10U);
  EXPECT_EQ(firstOverflow.rawLocation->previousSyncOffset, 4U);
  EXPECT_EQ(firstOverflow.rawLocation->nextSyncOffset, 24U)
      << "the first later same-route synchronization must remain the overflow anchor";

  const auto& secondOverflow = diagnostics.events()[3];
  ASSERT_TRUE(secondOverflow.rawLocation.has_value());
  EXPECT_EQ(secondOverflow.rawLocation->offset, 12U);
  EXPECT_EQ(secondOverflow.rawLocation->previousSyncOffset, 6U);
  EXPECT_EQ(secondOverflow.rawLocation->nextSyncOffset, 14U);
  EXPECT_EQ(diagnostics.failureCount(), 2U) << "overflow summaries must remain non-failing warnings";
}

TEST(CtraceUnitTests, testTraceIssueReporterDoesNotUseOutOfOrderSyncAsLocationAnchor)
{
  CollectingDiagnosticSink diagnostics;
  TraceIssueReporter reporter(diagnostics);
  auto laterSync = TraceEvent{SyncTraceEvent{}};
  laterSync.index = 100U;
  reporter.append(laterSync);
  auto earlierIssue = issuePacket(TraceIssueCode::OpenCsdBadPacketSequence);
  earlierIssue.index = 10U;
  reporter.append(earlierIssue);
  auto overflow = overflowPacket(200U);
  overflow.index = 20U;
  reporter.append(overflow);
  auto earlierSync = TraceEvent{SyncTraceEvent{}};
  earlierSync.index = 15U;
  reporter.append(earlierSync);
  reporter.finish();

  ASSERT_EQ(diagnostics.events().size(), 2U);
  ASSERT_TRUE(diagnostics.events()[0].rawLocation.has_value());
  EXPECT_FALSE(diagnostics.events()[0].rawLocation->previousSyncOffset.has_value());
  ASSERT_TRUE(diagnostics.events()[1].rawLocation.has_value());
  EXPECT_FALSE(diagnostics.events()[1].rawLocation->previousSyncOffset.has_value());
  EXPECT_FALSE(diagnostics.events()[1].rawLocation->nextSyncOffset.has_value());
}

TEST(CtraceUnitTests, testTraceIssueReporterRetainsRecoverySpansWithoutInventingSync)
{
  CollectingDiagnosticSink diagnostics;
  TraceIssueReporter reporter(diagnostics);
  auto sync = TraceEvent{SyncTraceEvent{}};
  sync.index = 2U;
  reporter.append(sync);

  constexpr TraceRecoveryKind kinds[]{TraceRecoveryKind::Generic, TraceRecoveryKind::Consumed,
                                       TraceRecoveryKind::Resumed, TraceRecoveryKind::Unresolved};
  for (const auto kind : kinds) {
    auto event = issuePacket(TraceIssueCode::DataLoss);
    event.index = 10U;
    std::get<TraceIssueEvent>(event.payload).message = TraceRecovery{kind, 10U, 64U, 54U};
    reporter.append(event);
  }

  ASSERT_EQ(diagnostics.events().size(), std::size(kinds));
  for (std::size_t index = 0U; index < std::size(kinds); ++index) {
    const auto& diagnostic = diagnostics.events()[index];
    ASSERT_TRUE(diagnostic.rawLocation.has_value());
    EXPECT_EQ(diagnostic.rawLocation->kind, RawDiagnosticLocation::Kind::Decoder);
    EXPECT_EQ(diagnostic.rawLocation->offset, 10U);
    EXPECT_EQ(diagnostic.rawLocation->previousSyncOffset, 2U);
    EXPECT_EQ(diagnostic.rawLocation->endOffset,
              kinds[index] == TraceRecoveryKind::Generic ? std::nullopt : std::optional<std::uint64_t>{64U});
    EXPECT_EQ(diagnostic.rawLocation->nextSyncOffset,
              kinds[index] == TraceRecoveryKind::Resumed ? std::optional<std::uint64_t>{64U} : std::nullopt);
  }
}

TEST(CtraceUnitTests, testTraceIssueReporterPreservesPacketSizeForRawLocation)
{
  CollectingDiagnosticSink diagnostics;
  TraceIssueReporter reporter(diagnostics);
  auto event = issuePacket(TraceIssueCode::OpenCsdBadPacketSequence);
  event.index = 10U;
  std::get<TraceIssueEvent>(event.payload).message = TracePacketDiagnostic{TracePacketDiagnosticKind::BadSequence};
  std::get<TraceIssueEvent>(event.payload).message.packet =
      TracePacketContext{TracePacketKind::Async, 2U, std::vector<std::uint8_t>{0x00U, 0xfeU}};
  reporter.append(event);

  ASSERT_EQ(diagnostics.events().size(), 1U);
  ASSERT_TRUE(diagnostics.events().front().rawLocation.has_value());
  EXPECT_EQ(diagnostics.events().front().rawLocation->packetSize, 2U);
  EXPECT_FALSE(diagnostics.events().front().rawLocation->endOffset.has_value())
      << "only the input wrapper knows whether packet bytes occupy a contiguous raw interval";
  EXPECT_NE(diagnostics.events().front().detailedMessage->find("bytes=[00 fe]"), std::string::npos);
}
