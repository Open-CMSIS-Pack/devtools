/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "PackChk.h"

#include "CrossPlatform.h"

/**
 * @brief PackChk main entry point
 * @param argc command line argument
 * @param argv command line argument
 * @param envp command line argument (not used)
 * @return 0: ok, 1: error
*/
int main(int argc, const char* argv [], const char* envp [])
{
  PackChk packChk;

  return packChk.Check(argc, argv, envp);
}
