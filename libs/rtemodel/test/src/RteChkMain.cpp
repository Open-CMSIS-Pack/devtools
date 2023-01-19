/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
*/

// RteChkMain.cpp : RTE Model tester executable
//

#include "RteChk.h"

//------------------------------------------------------------------
int main(int argc, char* argv[])
{
  return RteChk::CheckRte(argc, argv);
}
