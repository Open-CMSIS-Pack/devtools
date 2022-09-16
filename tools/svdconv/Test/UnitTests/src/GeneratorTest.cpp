/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "CodeGenerator.h"

#include "gtest/gtest.h"
#include <string>

using namespace std;


TEST(CodeGenerator, Check) {
#if 0
  CodeGenerator gen;
  const string text = "This is a %i printf %s";
  int num = 42;
  const char* textInsert = "insertion test";

  const string& res = gen.FormatStr(text, num, textInsert);

  ASSERT_EQ("This is a 42 printf insertion test", res);
#endif
}




