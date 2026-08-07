/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_CTRACEMAIN_H
#define CTRACE_SRC_CTRACEMAIN_H

#include <string>
#include <vector>

/**
 * @brief Runs the ctrace command-line application.
 *
 * @param arguments Complete command line, including the executable name.
 * @return Zero on success; nonzero when parsing, decoding, or output reports a
 *         failing diagnostic.
 */
int CtraceMain(const std::vector<std::string>& arguments);

#endif  // CTRACE_SRC_CTRACEMAIN_H
