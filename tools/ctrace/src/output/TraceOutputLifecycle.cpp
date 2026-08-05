/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TraceOutputLifecycle.h"

#include "DiagnosticSink.h"
#include "TraceEvent.h"
#include "TraceOutput.h"

#include <cstddef>
#include <exception>
#include <memory>
#include <string>
#include <utility>
#include <vector>

/** @brief Extracts a stable message from a captured backend exception. */
static std::string exceptionMessage(const std::exception_ptr& error)
{
  try {
    std::rethrow_exception(error);
  } catch (const std::exception& ex) {
    return ex.what();
  } catch (...) {
    // Preserve noexcept error reporting even for non-standard exceptions.
    (void)0;
  }
  return "unknown exception";
}

TraceOutputLifecycle::TraceOutputLifecycle(std::vector<std::unique_ptr<TraceOutput>> outputs,
                                           DiagnosticSink& diagnostics)
  : m_outputs(std::move(outputs)),
    m_diagnostics(diagnostics),
    m_states(m_outputs.size(), State::Inactive)
{
  for (std::size_t index = 0; index < m_outputs.size(); ++index) {
    try {
      m_outputs[index]->start();
      m_states[index] = State::Active;
    } catch (...) {
      fail(index, "start", std::current_exception());
      abortNoexcept(index);
    }
  }
}

TraceOutputLifecycle::~TraceOutputLifecycle() noexcept
{
  abortActiveNoexcept();
}

void TraceOutputLifecycle::append(const TraceEvent& event)
{
  for (std::size_t index = 0; index < m_outputs.size(); ++index) {
    if (m_states[index] != State::Active) {
      continue;
    }
    try {
      m_outputs[index]->writeEvent(event);
    } catch (...) {
      fail(index, "write", std::current_exception());
      abortNoexcept(index);
    }
  }
}

void TraceOutputLifecycle::abort() noexcept
{
  abortActiveNoexcept();
}

void TraceOutputLifecycle::finish() noexcept
{
  if (m_finished) {
    return;
  }

  for (std::size_t index = m_outputs.size(); index > 0U; --index) {
    const auto outputIndex = index - 1U;
    if (m_states[outputIndex] != State::Active) {
      continue;
    }
    try {
      m_outputs[outputIndex]->stop();
      m_states[outputIndex] = State::Completed;
    } catch (...) {
      fail(outputIndex, "stop", std::current_exception());
      abortNoexcept(outputIndex);
    }
  }
  m_finished = true;
}

void TraceOutputLifecycle::fail(std::size_t index, const char* phase, const std::exception_ptr& error) noexcept
{
  m_states[index] = State::Failed;
  try {
    const auto message = exceptionMessage(error);
    const auto backend = std::string(m_outputs[index]->backendName());
    const auto target = m_outputs[index]->targetPath();
    const auto displayMessage =
        backend + " output" + (target.empty() ? std::string() : " '" + target + "'") + " failed during " + phase +
        ": " + message;
    m_diagnostics.report({
        DiagnosticSink::Severity::Error,
        displayMessage,
        {
            {"outputIndex", std::to_string(index)},
            {"backend", backend},
            {"path", target},
            {"phase", phase},
            {"error", message},
        },
    });
  } catch (...) {
    // Diagnostics must never escape this noexcept failure path.
    (void)0;
  }
}

void TraceOutputLifecycle::abortNoexcept(std::size_t index) noexcept
{
  try {
    m_outputs[index]->abort();
  } catch (...) {
    fail(index, "abort", std::current_exception());
  }
  m_states[index] = State::Failed;
}

void TraceOutputLifecycle::abortActiveNoexcept() noexcept
{
  for (std::size_t index = 0; index < m_outputs.size(); ++index) {
    if (m_states[index] == State::Active) {
      abortNoexcept(index);
    }
  }
  m_finished = true;
}
