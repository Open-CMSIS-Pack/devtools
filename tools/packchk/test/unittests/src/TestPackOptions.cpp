/*
* Copyright (c) 2020-2021 Arm Limited. All rights reserved.
*
* SPDX-License-Identifier: Apache-2.0
*/

#include "gtest/gtest.h"

#include "PackOptions.h"

#include <string>

using namespace std;


/*
* Dummy class required to allow access to the methods under test which are protected
*/
class PackOptionsExposed : public CPackOptions {
public:
  PackOptionsExposed()
  {}
};


TEST(TestPackOptions, SetUrlRef) {
  CPackOptions packOptions;

  EXPECT_TRUE(packOptions.SetUrlRef("\"http://keil.com/packs\""));
  EXPECT_FALSE(packOptions.SetUrlRef("\"\""));
  EXPECT_FALSE(packOptions.SetUrlRef(""));
}
