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
#include <vector>

/** @brief Parses and validates the ctrace command line. */
class CliParser final {
public:
  /** @brief Parses command-line arguments into structured options. */
  static CliOptions parse(const std::vector<std::string>& arguments);
  /** @brief Validates relationships between parsed command-line options. */
  static void validate(const CliOptions& options);
  /** @brief Returns the formatted command-line help text. */
  static std::string helpString();
  /** @brief Returns the formatted ctrace version string. */
  static std::string versionString();

private:
  CliParser() = delete;
};

#endif  // CTRACE_SRC_CLI_CLIPARSER_H
