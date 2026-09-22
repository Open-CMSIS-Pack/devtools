/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CtfTestSupport.h"
#include "TestPath.h"
#include "TestSupport.h"
#include "TraceOutputTestSupport.h"
#include <gtest/gtest.h>
#include "csv/CsvFileOutput.h"
#include "ctf/CtfBundleOutput.h"
#include "DiagnosticSink.h"
#include "TraceEvent.h"
#include "TraceOutput.h"
#include "TraceOutputConfig.h"
#include "TraceOutputLifecycle.h"
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using TraceOutputTestSupport::TestTraceOutput;
using TraceOutputTestSupport::TestTraceOutputFailure;

/** @brief Simulates failure while recording an output diagnostic. */
class ThrowingDiagnosticSink final : public DiagnosticSink {
protected:
  /** @brief Throws the synthetic diagnostic failure. */
  void write(const Event&) override
  {
    throw std::runtime_error("synthetic diagnostic failure");
  }
};

TEST(CtraceUnitTests, testTraceOutputOwnsBackendTransaction)
{
  std::vector<std::string> calls;
  TestTraceOutput output(calls);

  output.writeEvent(softwarePacket(1U));
  output.writeByteSkip({0U, 1U});
  output.stop();
  output.abort();
  output.start();
  output.writeEvent(softwarePacket(2U));
  output.writeByteSkip({16U, 5U});
  output.start();
  output.stop();
  output.stop();
  output.abort();
  output.writeEvent(softwarePacket(3U));
  output.writeByteSkip({32U, 2U});

  EXPECT_EQ(calls, (std::vector<std::string>{"start", "write", "write-byte-skip", "abort", "start", "stop"}));
}

TEST(CtraceUnitTests, testTraceOutputCleansUpFailuresAfterActivation)
{
  TestTraceOutput prepareFailure(TestTraceOutputFailure::Prepare);
  EXPECT_THROW(prepareFailure.start(), std::runtime_error);
  EXPECT_FALSE(prepareFailure.aborted());

  TestTraceOutput startFailure(TestTraceOutputFailure::Start);
  EXPECT_THROW(startFailure.start(), std::runtime_error);
  EXPECT_TRUE(startFailure.aborted());

  TestTraceOutput writeFailure(TestTraceOutputFailure::Write);
  writeFailure.start();
  EXPECT_THROW(writeFailure.writeEvent(softwarePacket(1U)), std::runtime_error);
  EXPECT_TRUE(writeFailure.aborted());

  TestTraceOutput skipWriteFailure(TestTraceOutputFailure::ByteSkipWrite);
  skipWriteFailure.start();
  EXPECT_THROW(skipWriteFailure.writeByteSkip({0U, 1U}), std::runtime_error);
  EXPECT_TRUE(skipWriteFailure.aborted());

  TestTraceOutput stopFailure(TestTraceOutputFailure::Stop);
  stopFailure.start();
  EXPECT_THROW(stopFailure.stop(), std::runtime_error);
  EXPECT_TRUE(stopFailure.aborted());
}

TEST(CtraceUnitTests, testTraceOutputKeepsFailedAbortRetryable)
{
  TestTraceOutput output;
  output.start();
  output.setFailure(TestTraceOutputFailure::Abort);
  EXPECT_THROW(output.abort(), std::runtime_error);
  EXPECT_FALSE(output.aborted());
  output.setFailure(TestTraceOutputFailure::None);
  EXPECT_NO_THROW(output.abort());
  EXPECT_TRUE(output.aborted());

  std::vector<std::string> destructorCalls;
  {
    TestTraceOutput destructorOutput(destructorCalls);
    destructorOutput.start();
    destructorOutput.setFailure(TestTraceOutputFailure::Abort);
  }
  EXPECT_EQ(destructorCalls, (std::vector<std::string>{"start", "abort"}));
}

TEST(CtraceUnitTests, testTraceOutputLifecycleCompletesIndependentOutputs)
{
  const TemporaryTestPath temporaryPath("ctrace-output-lifecycle-test.csv");
  const auto& path = temporaryPath.path();
  writeTestFile(path, "old-output\n");

  std::vector<std::unique_ptr<TraceOutput>> outputs;
  outputs.push_back(std::make_unique<TestTraceOutput>(TestTraceOutputFailure::Start));
  auto csv = std::make_unique<CsvFileOutput>(path);
  outputs.push_back(std::move(csv));
  outputs.push_back(std::make_unique<TestTraceOutput>(TestTraceOutputFailure::Stop));

  CollectingDiagnosticSink diagnostics;
  TraceOutputLifecycle lifecycle(std::move(outputs), diagnostics);
  TraceEvent packet = softwarePacket(1U, 1U, 'A');
  lifecycle.append(packet);
  lifecycle.finish();
  ASSERT_TRUE(diagnostics.failureCount() == 2U) << "output lifecycle should report start and finalization failures";

  const auto contents = readTestTextFile(path);
  ASSERT_TRUE(contents.find("cycles,stream,type,source,value,pc,address,note\n") == 0U &&
              contents.find(",,itm,1,0x41,,,\n") != std::string::npos)
      << "output lifecycle should complete successful outputs despite another output failure";
}

TEST(CtraceUnitTests, testTraceOutputLifecycleCompletesCsvAfterLaterCtfStreamFailure)
{
  const TemporaryTestPath temporaryPath("ctrace-output-lifecycle-real-ctf-failure-test");
  const auto& root = temporaryPath.createDirectory();
  const auto ctfDirectory = root / "output.ctf";
  const auto xmlPath = root / "output.SWO.traceanalysis.xml";
  const auto csvPath = root / "output.csv";
  const TraceRouteIdentity firstRoute{TraceRouteId{10U}, 1U};
  const TraceRouteIdentity lastRoute{TraceRouteId{20U}, 111U};
  CtfMetadataTopology topology{
      {{CtfClockDomainId{1U}, "shared_clock", CtfTestSupport::testUuid(1U), 1000000U, false}},
      {
          {CtfStreamClassId{1U}, firstRoute, "core-one", CtfClockDomainId{1U}},
          {CtfStreamClassId{111U}, lastRoute, "core-last", CtfClockDomainId{1U}},
      },
      {},
  };

  CollectingDiagnosticSink diagnostics;
  std::vector<std::unique_ptr<TraceOutput>> outputs;
  outputs.push_back(
      std::make_unique<CtfBundleOutput>(CtfOutputConfig(ctfDirectory, xmlPath, {}, std::move(topology),
                                                        std::vector<TraceRouteIdentity>{firstRoute, lastRoute}),
                                        &diagnostics));
  outputs.push_back(std::make_unique<CsvFileOutput>(csvPath));
  TraceOutputLifecycle lifecycle(std::move(outputs), diagnostics);

  std::filesystem::create_directory(ctfDirectory / "stream_111");
  lifecycle.append(onRoute(softwarePacket(1U, 1U, 'A'), firstRoute));
  lifecycle.append(onRoute(softwarePacket(2U, 1U, 'B'), lastRoute));
  lifecycle.append(onRoute(softwarePacket(3U, 1U, 'C'), firstRoute));
  lifecycle.finish();

  EXPECT_FALSE(std::filesystem::exists(ctfDirectory));
  EXPECT_FALSE(std::filesystem::exists(xmlPath));
  EXPECT_EQ(diagnostics.failureCount(), 1U);
  EXPECT_TRUE(diagnostics.containsContext("backend", "ctf"));
  EXPECT_TRUE(diagnostics.containsContext("phase", "write"));
  const auto lines = readTestLines(csvPath);
  ASSERT_EQ(lines.size(), 4U);
  EXPECT_EQ(lines[1], ",1,itm,1,0x41,,,");
  EXPECT_EQ(lines[2], ",111,itm,2,0x42,,,");
  EXPECT_EQ(lines[3], ",1,itm,3,0x43,,,");
}

TEST(CtraceUnitTests, testTraceOutputLifecycleCompletesCtfAfterRealCsvStartFailure)
{
  const TemporaryTestPath temporaryPath("ctrace-output-lifecycle-real-csv-failure-test");
  const auto& root = temporaryPath.createDirectory();
  const auto ctfDirectory = root / "output.ctf";
  const auto xmlPath = root / "output.SWO.traceanalysis.xml";
  const auto csvPath = root / "blocked.csv";
  std::filesystem::create_directory(csvPath);

  CollectingDiagnosticSink diagnostics;
  std::vector<std::unique_ptr<TraceOutput>> outputs;
  outputs.push_back(std::make_unique<CtfBundleOutput>(
      CtfOutputConfig(ctfDirectory, xmlPath, {}, CtfTestSupport::legacyTopology(1000000U))));
  outputs.push_back(std::make_unique<CsvFileOutput>(csvPath));
  TraceOutputLifecycle lifecycle(std::move(outputs), diagnostics);
  lifecycle.append(softwarePacket(1U, 1U, 'A'));
  lifecycle.append(TraceEvent{PcSampleTraceEvent{0U, PcSampleKind::Sleep}});
  lifecycle.finish();

  EXPECT_TRUE(std::filesystem::is_regular_file(ctfDirectory / "metadata"));
  EXPECT_TRUE(std::filesystem::is_regular_file(ctfDirectory / "stream_0"));
  EXPECT_TRUE(std::filesystem::is_regular_file(xmlPath));
  EXPECT_EQ(diagnostics.failureCount(), 1U);
  EXPECT_TRUE(diagnostics.containsContext("backend", "csv"));
  EXPECT_TRUE(diagnostics.containsContext("phase", "start"));
}

TEST(CtraceUnitTests, testTraceOutputLifecycleReportsDestructorAbortFailures)
{
  std::vector<std::unique_ptr<TraceOutput>> outputs;
  outputs.push_back(std::make_unique<TestTraceOutput>(TestTraceOutputFailure::Abort));
  CollectingDiagnosticSink diagnostics;
  {
    TraceOutputLifecycle lifecycle(std::move(outputs), diagnostics);
  }

  ASSERT_TRUE(diagnostics.failureCount() == 1U && diagnostics.containsContext("phase", "abort"))
      << "output lifecycle must report a failed direct-output cleanup";
}

TEST(CtraceUnitTests, testTraceOutputLifecycleReportsWriteFailuresAndFinishesOnce)
{
  std::vector<std::unique_ptr<TraceOutput>> outputs;
  auto failing = std::make_unique<TestTraceOutput>(TestTraceOutputFailure::Write, "synthetic.trace");
  auto* failingPointer = failing.get();
  outputs.push_back(std::move(failing));
  CollectingDiagnosticSink diagnostics;
  TraceOutputLifecycle lifecycle(std::move(outputs), diagnostics);
  lifecycle.append(softwarePacket(1U));
  lifecycle.append(softwarePacket(2U));
  lifecycle.finish();
  lifecycle.finish();

  ASSERT_EQ(diagnostics.events().size(), 1U);
  EXPECT_TRUE(failingPointer->aborted());
  EXPECT_EQ(diagnostics.events().front().message,
            "trace output 'synthetic.trace' failed during write: intentional write failure");
}

TEST(CtraceUnitTests, testTraceOutputLifecycleIsolatesByteSkipWriteFailures)
{
  const TemporaryTestPath temporaryPath("ctrace-output-lifecycle-byte-skip-test.csv");
  std::vector<std::unique_ptr<TraceOutput>> outputs;
  std::vector<std::string> failingCalls;
  auto failing = std::make_unique<TestTraceOutput>(failingCalls);
  failing->setFailure(TestTraceOutputFailure::ByteSkipWrite);
  outputs.push_back(std::move(failing));
  outputs.push_back(std::make_unique<CsvFileOutput>(temporaryPath.path()));
  CollectingDiagnosticSink diagnostics;
  TraceOutputLifecycle lifecycle(std::move(outputs), diagnostics);

  lifecycle.appendByteSkip({0U, 1U});
  lifecycle.appendByteSkip({16U, 2U});
  lifecycle.append(softwarePacket(1U, 1U, 'A'));
  lifecycle.finish();
  lifecycle.appendByteSkip({32U, 3U});

  EXPECT_EQ(failingCalls, (std::vector<std::string>{"start", "write-byte-skip", "abort"}));
  ASSERT_EQ(diagnostics.events().size(), 1U);
  EXPECT_EQ(diagnostics.failureCount(), 1U);
  EXPECT_TRUE(diagnostics.containsContext("phase", "write"));
  EXPECT_TRUE(diagnostics.containsMessage("intentional byte-skip write failure"));
  const auto lines = readTestLines(temporaryPath.path());
  ASSERT_EQ(lines.size(), 4U);
  EXPECT_NE(lines[1].find("1 bytes skipped due to missing source ID"), std::string::npos);
  EXPECT_NE(lines[2].find("2 bytes skipped due to missing source ID"), std::string::npos);
  EXPECT_EQ(lines[3], ",,itm,1,0x41,,,");
}

TEST(CtraceUnitTests, testTraceOutputLifecycleDoesNotInventCtfStreamForSkippedBytes)
{
  const TemporaryTestPath temporaryPath("ctrace-output-lifecycle-unrouteable-ctf-test");
  const auto& root = temporaryPath.createDirectory();
  const auto ctfDirectory = root / "output.ctf";
  const auto xmlPath = root / "output.traceanalysis.xml";
  const TraceRouteIdentity route{TraceRouteId{10U}, 7U};
  CtfMetadataTopology topology{
      {{CtfClockDomainId{1U}, "core_clock", CtfTestSupport::testUuid(1U), 1000000U, false}},
      {{CtfStreamClassId{7U}, route, "core", CtfClockDomainId{1U}}},
      {},
  };
  CollectingDiagnosticSink diagnostics;
  std::vector<std::unique_ptr<TraceOutput>> outputs;
  outputs.push_back(std::make_unique<CtfBundleOutput>(
      CtfOutputConfig(ctfDirectory, xmlPath, {}, std::move(topology), {route}), &diagnostics));
  TraceOutputLifecycle lifecycle(std::move(outputs), diagnostics);

  lifecycle.appendByteSkip({0U, 1U});
  lifecycle.appendByteSkip({16U, 5U, TraceByteSkipReason::NullSourceId, 0U});
  lifecycle.appendByteSkip({32U, 3U, TraceByteSkipReason::ReservedSourceId, 127U});
  lifecycle.appendByteSkip({48U, 2U, TraceByteSkipReason::UnconfiguredSourceId, 42U});
  lifecycle.appendByteSkip({64U, 8U, TraceByteSkipReason::MissingSync, 7U});
  lifecycle.finish();

  EXPECT_EQ(diagnostics.failureCount(), 0U);
  EXPECT_TRUE(std::filesystem::is_regular_file(ctfDirectory / "metadata"));
  EXPECT_FALSE(std::filesystem::exists(ctfDirectory / "stream_0"));
  EXPECT_FALSE(std::filesystem::exists(ctfDirectory / "stream_7"));
  EXPECT_FALSE(std::filesystem::exists(xmlPath));
}

TEST(CtraceUnitTests, testTraceOutputLifecycleContainsDiagnosticAndNonStandardFailures)
{
  std::vector<std::unique_ptr<TraceOutput>> outputs;
  outputs.push_back(std::make_unique<TestTraceOutput>(TestTraceOutputFailure::NonStandardStart));
  ThrowingDiagnosticSink diagnostics;
  EXPECT_NO_THROW((void)TraceOutputLifecycle(std::move(outputs), diagnostics));
  EXPECT_EQ(diagnostics.failureCount(), 1U);

  std::vector<std::unique_ptr<TraceOutput>> passiveOutputs;
  passiveOutputs.push_back(std::make_unique<TestTraceOutput>());
  CollectingDiagnosticSink passiveDiagnostics;
  TraceOutputLifecycle passive(std::move(passiveOutputs), passiveDiagnostics);
  passive.finish();
  EXPECT_EQ(passiveDiagnostics.failureCount(), 0U);
}

TEST(CtraceUnitTests, testTraceOutputLifecycleRetainsCsvDespiteFatalCleanupFailure)
{
  const TemporaryTestPath outputPath("ctrace-partial-lifecycle.csv");
  std::vector<std::unique_ptr<TraceOutput>> outputs;
  outputs.push_back(std::make_unique<TestTraceOutput>(TestTraceOutputFailure::Abort));
  outputs.push_back(std::make_unique<CsvFileOutput>(outputPath.path()));
  CollectingDiagnosticSink diagnostics;
  TraceOutputLifecycle lifecycle(std::move(outputs), diagnostics);
  lifecycle.append(softwarePacket(1U, 1U, 'A'));
  const TraceDecodeAbort failure{16U, "synthetic fatal error"};
  lifecycle.finish(&failure);
  lifecycle.finish(&failure);

  EXPECT_TRUE(diagnostics.containsContext("phase", "decode-abort"));
  EXPECT_TRUE(diagnostics.containsMessage("intentional abort cleanup failure"));
  const auto lines = readTestLines(outputPath.path());
  ASSERT_EQ(lines.size(), 3U);
  EXPECT_EQ(lines[1], ",,itm,1,0x41,,,");
  EXPECT_EQ(lines[2], ",,error,,,,,decode aborted after processing 16 input bytes; "
                      "trace is incomplete: synthetic fatal error");
}
