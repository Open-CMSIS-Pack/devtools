/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#pragma once

#include "CliOptions.hpp"

#include <string>

namespace CliParser {

CliOptions parse(int argc, const char* const argv[]);
void validate(const CliOptions& options);
std::string helpString();
std::string versionString();

} // namespace CliParser
