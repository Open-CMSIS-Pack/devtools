/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#pragma once

#include "TraceOutputConfig.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace CtfMetadataWriter {

void write(const std::filesystem::path& outputDir, const std::string& uuidString, std::uint64_t coreClockHz,
           const std::vector<ResolvedTraceSource>& sources, const std::vector<std::uint32_t>& observedExceptionNumbers);

} // namespace CtfMetadataWriter
