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

  /** @brief Starts the initial thread-mode context. */
  void startThreadMode(const RecordEmitter& emit);
  /** @brief Closes the active lane and clears state at a discontinuity. */
  void resetForDiscontinuity(const RecordEmitter& emit);
  /** @brief Applies one exception transition and emits resulting lane records. */
  void consume(const ExceptionTraceEvent& event, const RecordEmitter& emit);
  /** @brief Returns exception numbers observed by this tracker. */
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

  /** @brief Switches the emitted active lane to one context. */
  void setActiveContext(std::uint32_t number, const RecordEmitter& emit);
  /** @brief Closes the currently emitted active lane. */
  void closeActiveContext(const RecordEmitter& emit);
  /** @brief Reconciles emitted lane state with the context stack. */
  void updateActiveContext(const RecordEmitter& emit);
  /** @brief Emits and records one lane transition. */
  void emitRecord(std::uint32_t number, RecordAction action, const RecordEmitter& emit);
  /** @brief Pushes or reactivates an entered exception context. */
  void enterContext(std::uint32_t number);
  /** @brief Removes an exited exception context. */
  void exitContext(std::uint32_t number);
  /** @brief Returns the stack to a previously active context. */
  void returnToContext(std::uint32_t number);

  std::vector<ContextFrame> m_contextStack;
  std::optional<std::uint32_t> m_activeContextNumber;
  std::vector<std::uint32_t> m_observedExceptionNumbers;
};

#endif  // CTRACE_SRC_OUTPUT_CTF_CTFEXCEPTIONLANETRACKER_H
