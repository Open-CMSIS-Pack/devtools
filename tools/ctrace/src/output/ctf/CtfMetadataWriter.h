/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_OUTPUT_CTF_CTFMETADATAWRITER_H
#define CTRACE_SRC_OUTPUT_CTF_CTFMETADATAWRITER_H

#include "CtfMetadataModel.h"

#include <filesystem>

/** @brief Writes the CTF metadata description for a completed trace bundle. */
class CtfMetadataWriter final {
public:
  /** @brief Serializes one complete bundle-local metadata model. */
  static void write(const std::filesystem::path& outputDir, const CtfMetadataModel& model);

private:
  /** @brief Prevents construction of this stateless metadata utility. */
  CtfMetadataWriter() = delete;
};

#endif  // CTRACE_SRC_OUTPUT_CTF_CTFMETADATAWRITER_H
