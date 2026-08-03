/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_CLI_CLIPARSER_H
#define CTRACE_SRC_CLI_CLIPARSER_H

#include "CliOptions.h"

#include <string>

class CliParser final {
public:
  static CliOptions parse(int argc, const char* const argv[]);
  static void validate(const CliOptions& options);
  static std::string helpString();
  static std::string versionString();

private:
  CliParser() = delete;
};

#endif  // CTRACE_SRC_CLI_CLIPARSER_H
