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
