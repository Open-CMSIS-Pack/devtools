/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_OUTPUT_CTF_TRACECOMPASSXMLWRITER_H
#define CTRACE_SRC_OUTPUT_CTF_TRACECOMPASSXMLWRITER_H

#include <filesystem>

/** @brief Writes the Trace Compass analysis definition accompanying CTF output. */
class TraceCompassXmlWriter final {
public:
  /** @brief Selects the state-system hierarchy generated for one trace. */
  enum class PathLayout {
    Legacy,
    RoutePrefixed,
  };

  /** @brief Writes the complete analysis definition to a file. */
  static void writeFile(const std::filesystem::path& path, PathLayout layout = PathLayout::Legacy);

private:
  /** @brief Prevents construction of this stateless XML utility. */
  TraceCompassXmlWriter() = delete;
};

#endif  // CTRACE_SRC_OUTPUT_CTF_TRACECOMPASSXMLWRITER_H
