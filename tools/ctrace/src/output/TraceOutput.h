/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_OUTPUT_TRACEOUTPUT_H
#define CTRACE_SRC_OUTPUT_TRACEOUTPUT_H

#include "TraceEvent.h"

#include <string>
#include <string_view>

class TraceOutput {
public:
  virtual ~TraceOutput() = default;

  virtual std::string_view backendName() const noexcept
  {
    return "trace";
  }
  virtual std::string targetPath() const
  {
    return {};
  }

  // start() prepares the final target, stop() completes it, and abort() discards
  // an incomplete active output. TraceOutputLifecycle owns this call order.
  virtual void start() {}
  virtual void stop() {}
  virtual void abort() = 0;
  // Called synchronously in decode order.
  virtual void writeEvent(const TraceEvent& event) = 0;
};

#endif  // CTRACE_SRC_OUTPUT_TRACEOUTPUT_H
