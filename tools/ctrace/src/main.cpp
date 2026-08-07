/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CtraceMain.h"

#include <string>
#include <vector>

/**
 * @brief Converts the process command line and delegates to CtraceMain().
 * @param argc Number of command-line arguments.
 * @param argv Command-line argument array.
 * @return Exit status produced by CtraceMain().
 */
int main(int argc, char* argv[])
{
  return CtraceMain(std::vector<std::string>(argv, argv + argc));
}
