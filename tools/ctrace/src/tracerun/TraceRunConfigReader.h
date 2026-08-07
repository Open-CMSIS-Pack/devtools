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

  /**
   * @brief Reads and validates a trace-run configuration file.
   * @param path Configuration file path.
   * @return Parsed configuration including all ctrace-relevant fields.
   * @throws std::runtime_error If the file cannot be read or is invalid.
   */
  virtual TraceRunConfig read(const std::string& path) const = 0;
};

#endif  // CTRACE_SRC_TRACERUN_TRACERUNCONFIGREADER_H
