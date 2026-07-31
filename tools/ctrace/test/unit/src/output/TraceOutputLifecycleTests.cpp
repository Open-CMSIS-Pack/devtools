/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TestPath.hpp"
#include "TestSupport.hpp"
#include <gtest/gtest.h>
#include "csv/CsvFileOutput.hpp"
#include "TraceOutput.hpp"
#include "TraceOutputLifecycle.hpp"
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

class FailingStopOutput final : public TraceOutput {
public:
  void stop() override
  {
    throw std::runtime_error("intentional stop failure");
  }

  void abort() override
  {
    aborted = true;
  }

  void writeEvent(const TraceEvent& packet) override
  {
    (void)packet;
  }

  bool aborted = false;
};

class FailingStartOutput final : public TraceOutput {
public:
  void start() override
  {
    throw std::runtime_error("intentional start failure");
  }

  void abort() override
  {
    aborted = true;
  }

  void writeEvent(const TraceEvent& packet) override
  {
    (void)packet;
  }

  bool aborted = false;
};

class FailingAbortOutput final : public TraceOutput {
public:
  void abort() override
  {
    throw std::runtime_error("intentional abort cleanup failure");
  }

  void writeEvent(const TraceEvent&) override {}
};

class FailingWriteOutput final : public TraceOutput {
public:
  std::string targetPath() const override
  {
    return "synthetic.trace";
  }

  void abort() override
  {
    aborted = true;
  }

  void writeEvent(const TraceEvent&) override
  {
    throw std::runtime_error("intentional write failure");
  }

  bool aborted = false;
};

class NonStandardFailureOutput final : public TraceOutput {
public:
  void start() override
  {
    throw 42;
  }

  void abort() override {}
  void writeEvent(const TraceEvent&) override {}
};

class ThrowingDiagnosticSink final : public DiagnosticSink {
protected:
  void write(const Event&) override
  {
    throw std::runtime_error("synthetic diagnostic failure");
  }
};

class PassiveOutput final : public TraceOutput {
public:
  void abort() override {}
  void writeEvent(const TraceEvent&) override {}
};

TEST(CtraceUnitTests, testTraceOutputLifecycleCompletesIndependentOutputs)
{
  const TemporaryTestPath temporaryPath("ctrace-output-lifecycle-test.csv");
  const auto& path = temporaryPath.path();
  writeTestFile(path, "old-output\n");

  std::vector<std::unique_ptr<TraceOutput>> outputs;
  outputs.push_back(std::make_unique<FailingStartOutput>());
  auto csv = std::make_unique<CsvFileOutput>(path);
  outputs.push_back(std::move(csv));
  outputs.push_back(std::make_unique<FailingStopOutput>());

  CollectingDiagnosticSink diagnostics;
  TraceOutputLifecycle lifecycle(std::move(outputs), diagnostics);
  TraceEvent packet = softwarePacket(1U, 1U, 'A');
  lifecycle.append(packet);
  lifecycle.finish();
  require(diagnostics.fatalCount() == 2U, "output lifecycle should report start and finalization failures");

  const auto contents = readTestTextFile(path);
  require(contents.find("cycles,stream,type,source,value,pc,offset,note\n") == 0U &&
              contents.find(",0,itm,1,0x41,,,\n") != std::string::npos,
          "output lifecycle should complete successful outputs despite another output failure");
}

TEST(CtraceUnitTests, testTraceOutputLifecycleReportsAbortFailures)
{
  std::vector<std::unique_ptr<TraceOutput>> outputs;
  outputs.push_back(std::make_unique<FailingAbortOutput>());
  CollectingDiagnosticSink diagnostics;
  TraceOutputLifecycle lifecycle(std::move(outputs), diagnostics);
  lifecycle.abort();

  bool reportsAbortPhase = false;
  for (const auto& event : diagnostics.events()) {
    for (const auto& [key, value] : event.context) {
      if (key == "phase" && value == "abort") {
        reportsAbortPhase = true;
      }
    }
  }
  require(diagnostics.fatalCount() == 1U && reportsAbortPhase,
          "output lifecycle must report a failed direct-output cleanup");
}

TEST(CtraceUnitTests, testTraceOutputLifecycleReportsWriteFailuresAndFinishesOnce)
{
  std::vector<std::unique_ptr<TraceOutput>> outputs;
  auto failing = std::make_unique<FailingWriteOutput>();
  auto* failingPointer = failing.get();
  outputs.push_back(std::move(failing));
  CollectingDiagnosticSink diagnostics;
  TraceOutputLifecycle lifecycle(std::move(outputs), diagnostics);
  lifecycle.append(softwarePacket(1U));
  lifecycle.append(softwarePacket(2U));
  lifecycle.finish();
  lifecycle.finish();

  ASSERT_EQ(diagnostics.events().size(), 1U);
  EXPECT_TRUE(failingPointer->aborted);
  EXPECT_EQ(diagnostics.events().front().compactMessage,
            "trace output 'synthetic.trace' failed during write: intentional write failure");
}

TEST(CtraceUnitTests, testTraceOutputLifecycleContainsDiagnosticAndNonStandardFailures)
{
  std::vector<std::unique_ptr<TraceOutput>> outputs;
  outputs.push_back(std::make_unique<NonStandardFailureOutput>());
  ThrowingDiagnosticSink diagnostics;
  EXPECT_NO_THROW((void)TraceOutputLifecycle(std::move(outputs), diagnostics));
  EXPECT_EQ(diagnostics.fatalCount(), 1U);

  std::vector<std::unique_ptr<TraceOutput>> passiveOutputs;
  passiveOutputs.push_back(std::make_unique<PassiveOutput>());
  CollectingDiagnosticSink passiveDiagnostics;
  TraceOutputLifecycle passive(std::move(passiveOutputs), passiveDiagnostics);
  passive.finish();
  EXPECT_EQ(passiveDiagnostics.fatalCount(), 0U);
}
