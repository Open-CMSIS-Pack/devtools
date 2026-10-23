/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_MODEL_TRACEINPUTCHANNEL_H
#define CTRACE_SRC_MODEL_TRACEINPUTCHANNEL_H

#include <string_view>

/** @brief Recognizes TB and named trace buffers. */
bool isTraceBufferChannel(std::string_view value);
/** @brief Recognizes supported raw input channels. */
bool isSupportedTraceChannel(std::string_view value);

#endif  // CTRACE_SRC_MODEL_TRACEINPUTCHANNEL_H
