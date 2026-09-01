/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CtfExceptionLaneTracker.h"

#include "TraceEvent.h"

#include <algorithm>
#include <cstdint>
#include <vector>

void CtfExceptionLaneTracker::startThreadMode(const RecordEmitter& emit)
{
  setActiveContext(kThreadModeNumber, RecordAction::Enter, emit);
}

void CtfExceptionLaneTracker::resetForDiscontinuity(const RecordEmitter& emit)
{
  m_contextStack.clear();
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
    updateActiveContext(RecordAction::Return, emit);
    return;
  case ExceptionAction::Unknown:
    return;
  }
  updateActiveContext(RecordAction::Enter, emit);
}

const std::vector<std::uint32_t>& CtfExceptionLaneTracker::observedExceptionNumbers() const
{
  return m_observedExceptionNumbers;
}

void CtfExceptionLaneTracker::setActiveContext(std::uint32_t number, RecordAction action, const RecordEmitter& emit)
{
  if (m_activeContextNumber.has_value() && *m_activeContextNumber == number) {
    if (action == RecordAction::Return) {
      emitRecord(number, action, emit);
    }
    return;
  }
  if (m_activeContextNumber.has_value()) {
    emitRecord(*m_activeContextNumber, RecordAction::Exit, emit);
  }
  emitRecord(number, action, emit);
  m_activeContextNumber = number;
}

void CtfExceptionLaneTracker::closeActiveContext(const RecordEmitter& emit)
{
  if (!m_activeContextNumber.has_value()) {
    return;
  }
  emitRecord(*m_activeContextNumber, RecordAction::Exit, emit);
  m_activeContextNumber.reset();
}

void CtfExceptionLaneTracker::updateActiveContext(RecordAction action, const RecordEmitter& emit)
{
  setActiveContext(m_contextStack.empty() ? kThreadModeNumber : m_contextStack.back().number, action, emit);
}

void CtfExceptionLaneTracker::emitRecord(std::uint32_t number, RecordAction action, const RecordEmitter& emit)
{
  if (std::find(m_observedExceptionNumbers.begin(), m_observedExceptionNumbers.end(), number) ==
      m_observedExceptionNumbers.end()) {
    m_observedExceptionNumbers.push_back(number);
  }
  emit(number, action);
}

void CtfExceptionLaneTracker::enterContext(std::uint32_t number)
{
  if (!m_contextStack.empty() && m_contextStack.back().state == ContextState::Running) {
    m_contextStack.back().state = ContextState::Preempted;
  }
  m_contextStack.push_back({number, ContextState::Running});
}

void CtfExceptionLaneTracker::exitContext(std::uint32_t number)
{
  if (m_contextStack.empty() || m_contextStack.back().number != number ||
      m_contextStack.back().state != ContextState::Running) {
    return;
  }
  m_contextStack.pop_back();
}

void CtfExceptionLaneTracker::returnToContext(std::uint32_t number)
{
  while (!m_contextStack.empty() && m_contextStack.back().number != number) {
    m_contextStack.pop_back();
  }
  if (!m_contextStack.empty()) {
    m_contextStack.back().state = ContextState::Running;
  } else if (number != kThreadModeNumber) {
    m_contextStack.push_back({number, ContextState::Running});
  }
}
