/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_OUTPUT_CTF_CTFEXCEPTIONLANETRACKER_H
#define CTRACE_SRC_OUTPUT_CTF_CTFEXCEPTIONLANETRACKER_H

#include "TraceEvent.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

/** @brief Tracks active Cortex-M exception contexts for Trace Compass lanes. */
class CtfExceptionLaneTracker {
public:
  /** @brief Selects whether an emitted lane record enters or exits a context. */
  enum class RecordAction {
    Enter,
    Exit,
  };

  /** @brief Emits one exception-lane record. */
  using RecordEmitter = std::function<void(std::uint32_t number, RecordAction action)>;

  /** @brief Clears all tracked exception contexts. */
  void reset();
  /** @brief Starts the initial thread-mode context. */
  void startThreadMode(const RecordEmitter& emit);
  /** @brief Closes the active lane and clears state at a discontinuity. */
  void resetForDiscontinuity(const RecordEmitter& emit);
  /** @brief Applies one exception transition and emits resulting lane records. */
  void consume(const ExceptionTraceEvent& event, const RecordEmitter& emit);
  /** @brief Returns exception numbers observed since the last reset. */
  const std::vector<std::uint32_t>& observedExceptionNumbers() const;

private:
  static constexpr std::uint32_t kThreadModeNumber = 0;

  /** @brief Identifies whether an exception context is active or preempted. */
  enum class ContextState : std::uint8_t {
    Running,
    Preempted,
  };

  /** @brief Stores one exception context on the nesting stack. */
  struct ContextFrame {
    std::uint32_t number = 0;
    ContextState state = ContextState::Running;
  };

  void setActiveContext(std::uint32_t number, const RecordEmitter& emit);
  void closeActiveContext(const RecordEmitter& emit);
  void updateActiveContext(const RecordEmitter& emit);
  void emitRecord(std::uint32_t number, RecordAction action, const RecordEmitter& emit);
  void enterContext(std::uint32_t number);
  void exitContext(std::uint32_t number);
  void returnToContext(std::uint32_t number);

  std::vector<ContextFrame> contextStack_;
  std::optional<std::uint32_t> activeContextNumber_;
  std::vector<std::uint32_t> observedExceptionNumbers_;
};

#endif  // CTRACE_SRC_OUTPUT_CTF_CTFEXCEPTIONLANETRACKER_H
