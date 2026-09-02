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
  setActiveContext(kThreadModeNumber, RecordAction::Enter, RecordOrigin::Synthetic, emit);
}

void CtfExceptionLaneTracker::resetForDiscontinuity(const RecordEmitter& emit)
{
  m_contextStack.clear();
  closeActiveContext(RecordOrigin::Synthetic, emit);
}

void CtfExceptionLaneTracker::consume(const ExceptionTraceEvent& event, const RecordEmitter& emit)
{
  const auto number = event.number;
  switch (event.action) {
  case ExceptionAction::Entered:
    enterContext(number);
    updateActiveContext(RecordAction::Enter, RecordOrigin::Trace, emit);
    return;
  case ExceptionAction::Exited:
    if (exitContext(number)) {
      closeActiveContext(RecordOrigin::Trace, emit);
    }
    return;
  case ExceptionAction::Returned:
    returnToContext(number);
    updateActiveContext(RecordAction::Return, RecordOrigin::Trace, emit);
    return;
  case ExceptionAction::Unknown:
    return;
  }
}

const std::vector<ExceptionNumber>& CtfExceptionLaneTracker::observedExceptionNumbers() const
{
  return m_observedExceptionNumbers;
}

void CtfExceptionLaneTracker::setActiveContext(ExceptionNumber number, RecordAction action, RecordOrigin origin,
                                               const RecordEmitter& emit)
{
  if (m_activeContextNumber.has_value() && *m_activeContextNumber == number) {
    if (action == RecordAction::Return) {
      emitRecord(number, action, origin, emit);
    }
    return;
  }
  if (m_activeContextNumber.has_value()) {
    emitRecord(*m_activeContextNumber, RecordAction::Exit, RecordOrigin::Synthetic, emit);
  }
  emitRecord(number, action, origin, emit);
  m_activeContextNumber = number;
}

void CtfExceptionLaneTracker::closeActiveContext(RecordOrigin origin, const RecordEmitter& emit)
{
  if (!m_activeContextNumber.has_value()) {
    return;
  }
  emitRecord(*m_activeContextNumber, RecordAction::Exit, origin, emit);
  m_activeContextNumber.reset();
}

void CtfExceptionLaneTracker::updateActiveContext(RecordAction action, RecordOrigin origin, const RecordEmitter& emit)
{
  setActiveContext(m_contextStack.empty() ? kThreadModeNumber : m_contextStack.back().number, action, origin, emit);
}

void CtfExceptionLaneTracker::emitRecord(ExceptionNumber number, RecordAction action, RecordOrigin origin,
                                         const RecordEmitter& emit)
{
  if (std::find(m_observedExceptionNumbers.begin(), m_observedExceptionNumbers.end(), number) ==
      m_observedExceptionNumbers.end()) {
    m_observedExceptionNumbers.push_back(number);
  }
  emit(number, action, origin);
}

void CtfExceptionLaneTracker::enterContext(ExceptionNumber number)
{
  if (!m_contextStack.empty() && m_contextStack.back().state == ContextState::Running) {
    m_contextStack.back().state = ContextState::Preempted;
  }
  m_contextStack.push_back({number, ContextState::Running});
}

bool CtfExceptionLaneTracker::exitContext(ExceptionNumber number)
{
  if (m_contextStack.empty() || m_contextStack.back().number != number ||
      m_contextStack.back().state != ContextState::Running) {
    return false;
  }
  m_contextStack.pop_back();
  return true;
}

void CtfExceptionLaneTracker::returnToContext(ExceptionNumber number)
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
