/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_OUTPUT_TRACEOUTPUTLIFECYCLE_H
#define CTRACE_SRC_OUTPUT_TRACEOUTPUTLIFECYCLE_H

#include "DiagnosticSink.h"
#include "TraceEvent.h"
#include "TraceOutput.h"

#include <cstddef>
#include <exception>
#include <memory>
#include <vector>

class TraceOutputLifecycle final : public TraceEventSink {
public:
  explicit TraceOutputLifecycle(std::vector<std::unique_ptr<TraceOutput>> outputs, DiagnosticSink& diagnostics);
  ~TraceOutputLifecycle() noexcept;

  void append(const TraceEvent& event) override;
  void finish() noexcept;
  void abort() noexcept;

  TraceOutputLifecycle(const TraceOutputLifecycle&) = delete;
  TraceOutputLifecycle& operator=(const TraceOutputLifecycle&) = delete;

private:
  enum class State {
    Inactive,
    Active,
    Completed,
    Failed,
  };

  void fail(std::size_t index, const char* phase, const std::exception_ptr& error) noexcept;
  void abortNoexcept(std::size_t index) noexcept;
  void abortActiveNoexcept() noexcept;

  std::vector<std::unique_ptr<TraceOutput>> outputs_;
  DiagnosticSink& diagnostics_;
  std::vector<State> states_;
  bool finished_ = false;
};

#endif  // CTRACE_SRC_OUTPUT_TRACEOUTPUTLIFECYCLE_H
