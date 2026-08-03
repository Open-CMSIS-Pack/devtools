/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_TRACERUN_TRACERUNCONFIGREADER_H
#define CTRACE_SRC_TRACERUN_TRACERUNCONFIGREADER_H

#include "TraceRunConfig.h"

#include <string>

/** @brief Defines the interface for reading trace-run configuration files. */
class TraceRunConfigReader {
public:
  /** @brief Destroys a trace-run reader through its interface. */
  virtual ~TraceRunConfigReader() = default;

  /** @brief Reads and validates a trace-run configuration file. */
  virtual TraceRunConfig read(const std::string& path) const = 0;
};

#endif  // CTRACE_SRC_TRACERUN_TRACERUNCONFIGREADER_H
