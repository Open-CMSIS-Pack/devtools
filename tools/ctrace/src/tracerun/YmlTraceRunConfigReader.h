/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_TRACERUN_YMLTRACERUNCONFIGREADER_H
#define CTRACE_SRC_TRACERUN_YMLTRACERUNCONFIGREADER_H

#include "TraceRunConfig.h"
#include "TraceRunConfigReader.h"

#include <string>

/** @brief Reads trace-run configuration files encoded as YAML. */
class YmlTraceRunConfigReader final : public TraceRunConfigReader {
public:
  /**
   * @brief Reads and validates the ctrace-relevant YAML fields.
   * @param path YAML trace-run configuration path.
   * @return Parsed processors, trace sources, and setup metadata.
   * @throws std::runtime_error If YAML syntax or required fields are invalid.
   */
  TraceRunConfig read(const std::string& path) const override;
};

#endif  // CTRACE_SRC_TRACERUN_YMLTRACERUNCONFIGREADER_H
