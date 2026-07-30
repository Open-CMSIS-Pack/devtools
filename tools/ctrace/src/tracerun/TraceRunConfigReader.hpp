/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#pragma once

#include "TraceRunConfig.hpp"

#include <string>

class TraceRunConfigReader {
public:
  virtual ~TraceRunConfigReader() = default;

  virtual TraceRunConfig read(const std::string& path) const = 0;
};
