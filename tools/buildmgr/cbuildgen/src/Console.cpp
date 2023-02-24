/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "CbuildGen.h"

using namespace std;

int main(int argc, char **argv, char** envp)
{
  return CbuildGen::RunCbuildGen(argc, argv, envp);
}
