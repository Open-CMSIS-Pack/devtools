/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#pragma once

#include "TraceEvent.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

class CtfExceptionLaneTracker {
public:
  enum class RecordAction {
    Enter,
    Exit,
  };

  using RecordEmitter = std::function<void(std::uint32_t number, RecordAction action)>;

  void reset();
  void startThreadMode(const RecordEmitter& emit);
  void resetForDiscontinuity(const RecordEmitter& emit);
  void consume(const ExceptionTraceEvent& event, const RecordEmitter& emit);
  const std::vector<std::uint32_t>& observedExceptionNumbers() const;

private:
  static constexpr std::uint32_t kThreadModeNumber = 0;

  enum class ContextState : std::uint8_t {
    Running,
    Preempted,
  };

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
