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

/**
 * @brief Defines the lifecycle and event interface of a trace output backend.
 *
 * A caller invokes start(), writes events synchronously in decode order, and
 * then invokes stop(). If any operation fails, abort() removes incomplete output.
 */
class TraceOutput {
public:
  /** @brief Destroys an output backend through its interface. */
  virtual ~TraceOutput() = default;

  /**
   * @brief Returns the stable backend name used in diagnostics.
   * @return Non-owning backend identifier with static lifetime.
   */
  virtual std::string_view backendName() const noexcept
  {
    return "trace";
  }
  /**
   * @brief Returns the primary output target path used in diagnostics.
   * @return Displayable path, or an empty string when no target exists.
   */
  virtual std::string targetPath() const
  {
    return {};
  }

  /** @brief Prepares a new final output target before the first event. */
  virtual void start() {}
  /** @brief Flushes and completes the active output target after the last event. */
  virtual void stop() {}
  /** @brief Discards an incomplete active output without committing partial data. */
  virtual void abort() = 0;
  /**
   * @brief Writes one event synchronously in decode order.
   * @param event Decoded event whose lifetime extends through this call.
   */
  virtual void writeEvent(const TraceEvent& event) = 0;
};

#endif  // CTRACE_SRC_OUTPUT_TRACEOUTPUT_H
