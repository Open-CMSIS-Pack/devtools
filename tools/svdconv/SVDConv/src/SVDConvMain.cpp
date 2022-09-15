/*
 * Copyright (c) 2010-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "SVDConv.h"

/**
* @brief SVDConv main entry point
* @param argc command line argument
* @param argv command line argument
* @param envp command line argument (not used)
* @return 0: ok, 1: error
*/
int main(int argc, const char* argv [], const char* envp [])
{
  SvdConv svdConv;

  return svdConv.Check(argc, argv, envp);
}
