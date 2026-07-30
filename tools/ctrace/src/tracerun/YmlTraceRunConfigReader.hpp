/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#pragma once

#include "TraceRunConfig.hpp"
#include "TraceRunConfigReader.hpp"

#include <string>

class YmlTraceRunConfigReader final : public TraceRunConfigReader {
public:
  TraceRunConfig read(const std::string& path) const override;
};
