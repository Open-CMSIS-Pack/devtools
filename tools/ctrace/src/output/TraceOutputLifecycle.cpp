/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TraceOutputLifecycle.hpp"

#include "DiagnosticSink.hpp"
#include "TraceEvent.hpp"
#include "TraceOutput.hpp"

#include <cstddef>
#include <exception>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

std::string exceptionMessage(const std::exception_ptr& error)
{
  try {
    if (error) {
      std::rethrow_exception(error);
    }
  } catch (const std::exception& ex) {
    return ex.what();
  } catch (...) {
    // Preserve noexcept error reporting even for non-standard exceptions.
    (void)0;
  }
  return "unknown exception";
}

} // namespace

TraceOutputLifecycle::TraceOutputLifecycle(std::vector<std::unique_ptr<TraceOutput>> outputs,
                                           DiagnosticSink& diagnostics)
  : outputs_(std::move(outputs)), diagnostics_(diagnostics), states_(outputs_.size(), State::Inactive)
{
  for (std::size_t index = 0; index < outputs_.size(); ++index) {
    try {
      outputs_[index]->start();
      states_[index] = State::Active;
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
  for (std::size_t index = 0; index < outputs_.size(); ++index) {
    if (states_[index] != State::Active) {
      continue;
    }
    try {
      outputs_[index]->writeEvent(event);
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
  if (finished_) {
    return;
  }

  for (std::size_t index = outputs_.size(); index > 0U; --index) {
    const auto outputIndex = index - 1U;
    if (states_[outputIndex] != State::Active) {
      continue;
    }
    try {
      outputs_[outputIndex]->stop();
      states_[outputIndex] = State::Completed;
    } catch (...) {
      fail(outputIndex, "stop", std::current_exception());
      abortNoexcept(outputIndex);
    }
  }
  finished_ = true;
}

void TraceOutputLifecycle::fail(std::size_t index, const char* phase, const std::exception_ptr& error) noexcept
{
  states_[index] = State::Failed;
  try {
    const auto message = exceptionMessage(error);
    const auto backend = std::string(outputs_[index]->backendName());
    const auto target = outputs_[index]->targetPath();
    diagnostics_.report({
        DiagnosticSink::Severity::Error,
        DiagnosticSink::Category::Output,
        "output-failed",
        "trace output failed",
        {
            {"outputIndex", std::to_string(index)},
            {"backend", backend},
            {"path", target},
            {"phase", phase},
            {"error", message},
        },
        backend + " output" + (target.empty() ? std::string() : " '" + target + "'") + " failed during " + phase +
            ": " + message,
    });
  } catch (...) {
    // Diagnostics must never escape this noexcept failure path.
    (void)0;
  }
}

void TraceOutputLifecycle::abortNoexcept(std::size_t index) noexcept
{
  try {
    outputs_[index]->abort();
  } catch (...) {
    fail(index, "abort", std::current_exception());
  }
  states_[index] = State::Failed;
}

void TraceOutputLifecycle::abortActiveNoexcept() noexcept
{
  for (std::size_t index = 0; index < outputs_.size(); ++index) {
    if (states_[index] == State::Active) {
      abortNoexcept(index);
    }
  }
  finished_ = true;
}
