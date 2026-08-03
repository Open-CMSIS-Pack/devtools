/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TestPath.h"
#include "TestSupport.h"
#include "TraceOutputTestSupport.h"
#include <gtest/gtest.h>
#include "csv/CsvFileOutput.h"
#include "TraceOutputLifecycle.h"
#include <memory>
#include <stdexcept>
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
  require(diagnostics.failureCount() == 2U, "output lifecycle should report start and finalization failures");

  const auto contents = readTestTextFile(path);
  require(contents.find("cycles,stream,type,source,value,pc,offset,note\n") == 0U &&
              contents.find(",0,itm,1,0x41,,,\n") != std::string::npos,
          "output lifecycle should complete successful outputs despite another output failure");
}

TEST(CtraceUnitTests, testTraceOutputLifecycleReportsAbortFailures)
{
  std::vector<std::unique_ptr<TraceOutput>> outputs;
  outputs.push_back(std::make_unique<TestTraceOutput>(TestTraceOutputFailure::Abort));
  CollectingDiagnosticSink diagnostics;
  TraceOutputLifecycle lifecycle(std::move(outputs), diagnostics);
  lifecycle.abort();

  require(diagnostics.failureCount() == 1U && diagnostics.containsContext("output-failed", "phase", "abort"),
          "output lifecycle must report a failed direct-output cleanup");
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
  EXPECT_EQ(diagnostics.events().front().compactMessage,
            "trace output 'synthetic.trace' failed during write: intentional write failure");
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
