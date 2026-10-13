/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_TEST_UNIT_SUPPORT_TRACEOUTPUTTESTSUPPORT_H
#define CTRACE_TEST_UNIT_SUPPORT_TRACEOUTPUTTESTSUPPORT_H

#include "TraceOutput.h"
#include "TraceEvent.h"

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace TraceOutputTestSupport {

/** @brief Selects the lifecycle operation where a test output fails. */
enum class TestTraceOutputFailure { None, Prepare, Start, Stop, Abort, Write, ByteSkipWrite, NonStandardStart };

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

  /** @brief Cleans up an active synthetic output without throwing from destruction. */
  ~TestTraceOutput() override
  {
    abortNoexcept();
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

  /** @brief Changes the synthetic failure point for retry tests. */
  void setFailure(TestTraceOutputFailure failure)
  {
    m_failure = failure;
  }

protected:
  /** @brief Optionally rejects an output before it becomes active. */
  void prepareOutput() override
  {
    failAt(TestTraceOutputFailure::Prepare, "intentional prepare failure");
  }

  /** @brief Records start and optionally throws the configured failure. */
  void startOutput() override
  {
    record("start");
    if (m_failure == TestTraceOutputFailure::NonStandardStart) {
      throw 42;
    }
    failAt(TestTraceOutputFailure::Start, "intentional start failure");
  }

  /** @brief Records stop and optionally throws the configured failure. */
  void stopOutput() override
  {
    record("stop");
    failAt(TestTraceOutputFailure::Stop, "intentional stop failure");
  }

  /** @brief Records abort and optionally throws the configured failure. */
  void abortOutput() override
  {
    record("abort");
    failAt(TestTraceOutputFailure::Abort, "intentional abort cleanup failure");
    m_aborted = true;
  }

  /** @brief Records an event write and optionally throws the configured failure. */
  void writeOutput(const TraceEvent&) override
  {
    record("write");
    failAt(TestTraceOutputFailure::Write, "intentional write failure");
  }

  /** @brief Records skipped-byte accounting and optionally throws the configured failure. */
  void writeByteSkipOutput(const TraceByteSkip&) override
  {
    record("write-byte-skip");
    failAt(TestTraceOutputFailure::ByteSkipWrite, "intentional byte-skip write failure");
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
