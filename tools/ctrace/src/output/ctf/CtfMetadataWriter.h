/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_OUTPUT_CTF_CTFMETADATAWRITER_H
#define CTRACE_SRC_OUTPUT_CTF_CTFMETADATAWRITER_H

#include "TraceEvent.h"
#include "TraceOutputConfig.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

/** @brief Writes the CTF metadata description for a completed trace bundle. */
class CtfMetadataWriter final {
public:
  /** @brief Writes metadata for clock, event schemas, sources, and exception lanes. */
  static void write(const std::filesystem::path& outputDir, const std::string& uuidString, std::uint64_t coreClockHz,
                    const std::vector<ResolvedTraceSource>& sources,
                    const std::vector<ExceptionNumber>& observedExceptionNumbers);

private:
  /** @brief Prevents construction of this stateless metadata utility. */
  CtfMetadataWriter() = delete;
};

#endif  // CTRACE_SRC_OUTPUT_CTF_CTFMETADATAWRITER_H
