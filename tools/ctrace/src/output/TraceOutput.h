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

/** @brief Defines the lifecycle and event interface of a trace output backend. */
class TraceOutput {
public:
  /** @brief Destroys an output backend through its interface. */
  virtual ~TraceOutput() = default;

  /** @brief Returns the stable backend name used in diagnostics. */
  virtual std::string_view backendName() const noexcept
  {
    return "trace";
  }
  /** @brief Returns the primary output target path used in diagnostics. */
  virtual std::string targetPath() const
  {
    return {};
  }

  /** @brief Prepares the final output target. */
  virtual void start() {}
  /** @brief Completes the active output target. */
  virtual void stop() {}
  /** @brief Discards an incomplete active output. */
  virtual void abort() = 0;
  /** @brief Writes one event synchronously in decode order. */
  virtual void writeEvent(const TraceEvent& event) = 0;
};

#endif  // CTRACE_SRC_OUTPUT_TRACEOUTPUT_H
