/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CtfExceptionLaneTracker.hpp"

#include "TraceEvent.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

void CtfExceptionLaneTracker::reset()
{
  contextStack_.clear();
  activeContextNumber_.reset();
  observedExceptionNumbers_.clear();
}

void CtfExceptionLaneTracker::startThreadMode(const RecordEmitter& emit)
{
  setActiveContext(kThreadModeNumber, emit);
}

void CtfExceptionLaneTracker::resetForDiscontinuity(const RecordEmitter& emit)
{
  contextStack_.clear();
  closeActiveContext(emit);
}

void CtfExceptionLaneTracker::consume(const ExceptionTraceEvent& event, const RecordEmitter& emit)
{
  const auto number = event.number & 0x1ffU;
  switch (event.action) {
  case ExceptionAction::Entered:
    enterContext(number);
    break;
  case ExceptionAction::Exited:
    exitContext(number);
    break;
  case ExceptionAction::Returned:
    returnToContext(number);
    break;
  case ExceptionAction::Unknown:
    return;
  }
  updateActiveContext(emit);
}

const std::vector<std::uint32_t>& CtfExceptionLaneTracker::observedExceptionNumbers() const
{
  return observedExceptionNumbers_;
}

void CtfExceptionLaneTracker::setActiveContext(std::uint32_t number, const RecordEmitter& emit)
{
  if (activeContextNumber_.has_value() && *activeContextNumber_ == number) {
    return;
  }
  if (activeContextNumber_.has_value()) {
    emitRecord(*activeContextNumber_, RecordAction::Exit, emit);
  }
  emitRecord(number, RecordAction::Enter, emit);
  activeContextNumber_ = number;
}

void CtfExceptionLaneTracker::closeActiveContext(const RecordEmitter& emit)
{
  if (!activeContextNumber_.has_value()) {
    return;
  }
  emitRecord(*activeContextNumber_, RecordAction::Exit, emit);
  activeContextNumber_.reset();
}

void CtfExceptionLaneTracker::updateActiveContext(const RecordEmitter& emit)
{
  setActiveContext(contextStack_.empty() ? kThreadModeNumber : contextStack_.back().number, emit);
}

void CtfExceptionLaneTracker::emitRecord(std::uint32_t number, RecordAction action, const RecordEmitter& emit)
{
  if (std::find(observedExceptionNumbers_.begin(), observedExceptionNumbers_.end(), number) ==
      observedExceptionNumbers_.end()) {
    observedExceptionNumbers_.push_back(number);
  }
  emit(number, action);
}

void CtfExceptionLaneTracker::enterContext(std::uint32_t number)
{
  if (!contextStack_.empty() && contextStack_.back().state == ContextState::Running) {
    contextStack_.back().state = ContextState::Preempted;
  }
  contextStack_.push_back({number, ContextState::Running});
}

void CtfExceptionLaneTracker::exitContext(std::uint32_t number)
{
  if (contextStack_.empty() || contextStack_.back().number != number ||
      contextStack_.back().state != ContextState::Running) {
    return;
  }
  contextStack_.pop_back();
}

void CtfExceptionLaneTracker::returnToContext(std::uint32_t number)
{
  while (!contextStack_.empty() && contextStack_.back().number != number) {
    contextStack_.pop_back();
  }
  if (!contextStack_.empty()) {
    contextStack_.back().state = ContextState::Running;
  } else if (number != kThreadModeNumber) {
    contextStack_.push_back({number, ContextState::Running});
  }
}
