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
    : m_failure(failure),
      m_target(std::move(target))
  {
  }

  /** @brief Creates a test output that records lifecycle calls. */
  explicit TestTraceOutput(std::vector<std::string>& calls)
    : m_calls(&calls)
  {
  }

  /** @brief Records start and optionally throws the configured failure. */
  void start() override
  {
    record("start");
    if (m_failure == TestTraceOutputFailure::NonStandardStart) {
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
    m_aborted = true;
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
    return m_target.empty() ? TraceOutput::targetPath() : m_target;
  }

  /** @brief Reports whether abort completed successfully. */
  bool aborted() const
  {
    return m_aborted;
  }

private:
  /** @brief Records a lifecycle call when call collection is enabled. */
  void record(const char* call)
  {
    if (m_calls != nullptr) {
      m_calls->emplace_back(call);
    }
  }

  /** @brief Throws when the supplied operation is the configured failure point. */
  void failAt(TestTraceOutputFailure point, const char* message) const
  {
    if (m_failure == point) {
      throw std::runtime_error(message);
    }
  }

  TestTraceOutputFailure m_failure = TestTraceOutputFailure::None;
  std::string m_target;
  std::vector<std::string>* m_calls = nullptr;
  bool m_aborted = false;
};

} // namespace TraceOutputTestSupport

#endif  // CTRACE_TEST_UNIT_SUPPORT_TRACEOUTPUTTESTSUPPORT_H
