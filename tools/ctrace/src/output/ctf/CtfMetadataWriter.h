/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_OUTPUT_CTF_CTFMETADATAWRITER_H
#define CTRACE_SRC_OUTPUT_CTF_CTFMETADATAWRITER_H

#include "TraceOutputConfig.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

class CtfMetadataWriter final {
public:
  static void write(const std::filesystem::path& outputDir, const std::string& uuidString, std::uint64_t coreClockHz,
                    const std::vector<ResolvedTraceSource>& sources,
                    const std::vector<std::uint32_t>& observedExceptionNumbers);

private:
  CtfMetadataWriter() = delete;
};

#endif  // CTRACE_SRC_OUTPUT_CTF_CTFMETADATAWRITER_H
