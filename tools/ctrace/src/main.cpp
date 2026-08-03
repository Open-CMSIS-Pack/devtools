/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "CtraceMain.h"

#include <string>
#include <vector>

int main(int argc, char* argv[])
{
  return CtraceMain(std::vector<std::string>(argv, argv + argc));
}
