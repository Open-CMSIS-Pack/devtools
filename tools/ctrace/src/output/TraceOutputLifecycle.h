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

/** @brief Coordinates start, event delivery, completion, and cleanup for outputs. */
class TraceOutputLifecycle final : public TraceEventSink {
public:
  /** @brief Creates and starts the supplied output backends. */
  explicit TraceOutputLifecycle(std::vector<std::unique_ptr<TraceOutput>> outputs, DiagnosticSink& diagnostics);
  /** @brief Aborts any outputs that remain active. */
  ~TraceOutputLifecycle() noexcept;

  /** @brief Writes one event to every active output. */
  void append(const TraceEvent& event) override;
  /** @brief Completes all active outputs without propagating failures. */
  void finish() noexcept;
  /** @brief Aborts all active outputs without propagating failures. */
  void abort() noexcept;

  /** @brief Disables copying because the lifecycle owns output backends. */
  TraceOutputLifecycle(const TraceOutputLifecycle&) = delete;
  /** @brief Disables copy assignment because the lifecycle owns output backends. */
  TraceOutputLifecycle& operator=(const TraceOutputLifecycle&) = delete;

private:
  /** @brief Tracks the lifecycle state of one output backend. */
  enum class State {
    Inactive,
    Active,
    Completed,
    Failed,
  };

  /** @brief Marks one backend failed and reports the captured exception. */
  void fail(std::size_t index, const char* phase, const std::exception_ptr& error) noexcept;
  /** @brief Aborts one backend and suppresses cleanup exceptions. */
  void abortNoexcept(std::size_t index) noexcept;
  /** @brief Aborts every backend that is still active. */
  void abortActiveNoexcept() noexcept;

  std::vector<std::unique_ptr<TraceOutput>> m_outputs;
  DiagnosticSink& m_diagnostics;
  std::vector<State> m_states;
  bool m_finished = false;
};

#endif  // CTRACE_SRC_OUTPUT_TRACEOUTPUTLIFECYCLE_H
