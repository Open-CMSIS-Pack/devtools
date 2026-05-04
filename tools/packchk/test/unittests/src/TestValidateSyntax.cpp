/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gtest/gtest.h"

#include "ValidateSyntax.h"

/*
 * Dummy class required to allow access to the methods under test which are protected
*/
class ValidateSyntaxExposed : public ValidateSyntax {
public:
  ValidateSyntaxExposed(RteGlobalModel &rteModel, CPackOptions& packOptions) : ValidateSyntax(rteModel, packOptions)
  {}

  using ValidateSyntax::IsURL;
};


TEST(TestValidateSyntax, IsURL) {
  EXPECT_TRUE(ValidateSyntaxExposed::IsURL("http://mysite.com"));
  EXPECT_TRUE(ValidateSyntaxExposed::IsURL("https://mysite.com"));
  EXPECT_TRUE(ValidateSyntaxExposed::IsURL("ftp://mysite.com"));
  EXPECT_TRUE(ValidateSyntaxExposed::IsURL("ftps://mysite.com"));

  EXPECT_FALSE(ValidateSyntaxExposed::IsURL(""));
  EXPECT_FALSE(ValidateSyntaxExposed::IsURL("httpx://mysite.co.uk"));
}
