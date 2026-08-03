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

class YmlTraceRunConfigReader final : public TraceRunConfigReader {
public:
  TraceRunConfig read(const std::string& path) const override;
};

#endif  // CTRACE_SRC_TRACERUN_YMLTRACERUNCONFIGREADER_H
