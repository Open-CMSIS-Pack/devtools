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
  /** @brief Reads and validates the ctrace-relevant YAML fields. */
  TraceRunConfig read(const std::string& path) const override;
};

#endif  // CTRACE_SRC_TRACERUN_YMLTRACERUNCONFIGREADER_H
