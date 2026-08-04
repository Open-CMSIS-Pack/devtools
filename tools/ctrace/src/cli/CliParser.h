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
  /**
   * @brief Parses command-line arguments into structured options.
   * @param arguments Complete command line, including the executable name.
   * @return Parsed options without cross-option validation.
   * @throws std::runtime_error If an option or value is malformed.
   */
  static CliOptions parse(const std::vector<std::string>& arguments);
  /**
   * @brief Validates relationships between parsed command-line options.
   * @param options Options returned by parse().
   * @throws std::runtime_error If options conflict or required values are absent.
   */
  static void validate(const CliOptions& options);
  /**
   * @brief Returns the formatted command-line help text.
   * @return Self-contained usage and option documentation.
   */
  static std::string helpString();
  /**
   * @brief Returns the formatted ctrace version string.
   * @return Version text suitable for command-line output.
   */
  static std::string versionString();

private:
  /** @brief Prevents construction of this stateless parser utility. */
  CliParser() = delete;
};

#endif  // CTRACE_SRC_CLI_CLIPARSER_H
