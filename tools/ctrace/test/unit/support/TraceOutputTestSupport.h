/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_TEST_UNIT_SUPPORT_TRACEOUTPUTTESTSUPPORT_H
#define CTRACE_TEST_UNIT_SUPPORT_TRACEOUTPUTTESTSUPPORT_H

#include "TraceOutput.h"

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace TraceOutputTestSupport {

/** @brief Selects the lifecycle operation where a test output fails. */
enum class TestTraceOutputFailure { None, Start, Stop, Abort, Write, NonStandardStart };

/** @brief Implements a configurable trace output for lifecycle unit tests. */
class TestTraceOutput final : public TraceOutput {
public:
  /** @brief Creates a test output with an optional failure point and target. */
  explicit TestTraceOutput(TestTraceOutputFailure failure = TestTraceOutputFailure::None, std::string target = {})
    : failure_(failure), target_(std::move(target))
  {
  }

  /** @brief Creates a test output that records lifecycle calls. */
  explicit TestTraceOutput(std::vector<std::string>& calls) : calls_(&calls) {}

  /** @brief Records start and optionally throws the configured failure. */
  void start() override
  {
    record("start");
    if (failure_ == TestTraceOutputFailure::NonStandardStart) {
      throw 42;
    }
    failAt(TestTraceOutputFailure::Start, "intentional start failure");
    TraceOutput::start();
  }

  /** @brief Records stop and optionally throws the configured failure. */
  void stop() override
  {
    record("stop");
    failAt(TestTraceOutputFailure::Stop, "intentional stop failure");
    TraceOutput::stop();
  }

  /** @brief Records abort and optionally throws the configured failure. */
  void abort() override
  {
    record("abort");
    failAt(TestTraceOutputFailure::Abort, "intentional abort cleanup failure");
    aborted_ = true;
  }

  /** @brief Records an event write and optionally throws the configured failure. */
  void writeEvent(const TraceEvent&) override
  {
    record("write");
    failAt(TestTraceOutputFailure::Write, "intentional write failure");
  }

  /** @brief Returns the configured output target. */
  std::string targetPath() const override
  {
    return target_.empty() ? TraceOutput::targetPath() : target_;
  }

  /** @brief Reports whether abort completed successfully. */
  bool aborted() const
  {
    return aborted_;
  }

private:
  void record(const char* call)
  {
    if (calls_ != nullptr) {
      calls_->emplace_back(call);
    }
  }

  void failAt(TestTraceOutputFailure point, const char* message) const
  {
    if (failure_ == point) {
      throw std::runtime_error(message);
    }
  }

  TestTraceOutputFailure failure_ = TestTraceOutputFailure::None;
  std::string target_;
  std::vector<std::string>* calls_ = nullptr;
  bool aborted_ = false;
};

} // namespace TraceOutputTestSupport

#endif  // CTRACE_TEST_UNIT_SUPPORT_TRACEOUTPUTTESTSUPPORT_H
