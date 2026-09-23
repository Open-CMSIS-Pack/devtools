/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_OUTPUT_OUTPUTPATH_H
#define CTRACE_SRC_OUTPUT_OUTPUTPATH_H

#include <filesystem>

/** @brief Shared filesystem safety checks before replacing generated outputs. */
namespace OutputPath {

/** @brief Rejects empty, root-like, or directory-only target paths. */
void requireSpecificTarget(const std::filesystem::path& path, const char* description);

/** @brief Rejects a target whose nearest existing parent is not a directory. */
void validateParent(const std::filesystem::path& path, const char* description);

} // namespace OutputPath

#endif // CTRACE_SRC_OUTPUT_OUTPUTPATH_H
