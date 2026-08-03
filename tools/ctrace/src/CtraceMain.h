/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CTRACE_SRC_CTRACEMAIN_H
#define CTRACE_SRC_CTRACEMAIN_H

#include <string>
#include <vector>

/** @brief Runs the ctrace command-line application. */
int CtraceMain(const std::vector<std::string>& arguments);

#endif  // CTRACE_SRC_CTRACEMAIN_H
