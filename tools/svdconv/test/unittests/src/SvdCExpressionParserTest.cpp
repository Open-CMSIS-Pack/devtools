/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "SvdCExpressionParser.h"

#include "gtest/gtest.h"
#include <string>
#include <utility>
#include <vector>

using namespace std;

namespace {

using Parser = SvdCExpressionParser;
using ExpectedToken = pair<const char*, Parser::XETYPE>;

TEST(SvdCExpressionParserTest, RecognizesOperatorsAndQualifiers) {
  const vector<ExpectedToken> cases{
    {".", Parser::xme_qual}, {"->", Parser::xme_qual}, {"-", Parser::xme_minus},
    {"(", Parser::xme_openbr}, {")", Parser::xme_closebr},
    {"[", Parser::xme_opendim}, {"]", Parser::xme_closedim},
    {"+", Parser::xme_plus}, {"*", Parser::xme_mul}, {"/", Parser::xme_div},
    {"^", Parser::xme_xor}, {"!", Parser::xme_not}, {"~", Parser::xme_compl},
    {"<", Parser::xme_lt}, {"<<", Parser::xme_lsh}, {">>", Parser::xme_rsh},
    {"&", Parser::xme_and}, {"&&", Parser::xme_land},
    {"|", Parser::xme_or}, {"||", Parser::xme_lor},
    {"=", Parser::xme_asn}, {"==", Parser::xme_equ},
  };
  for(const auto& [text, type] : cases) {
    SCOPED_TRACE(text);
    Parser parser(text);
    ASSERT_TRUE(parser.Parse());
    const auto& tokens = parser.GetTokenList();
    ASSERT_EQ(1U, tokens.size());
    EXPECT_EQ(type, tokens.front().type);
    EXPECT_EQ(text, tokens.front().text);
  }
}

TEST(SvdCExpressionParserTest, PreservesTokenOrderAndTextInRegisterExpression) {
  Parser parser(" \t(GPIO1->CTRL.value & 0x10) == _state2[3]\r\n");
  ASSERT_TRUE(parser.Parse());
  const vector<ExpectedToken> expected{
    {"(", Parser::xme_openbr}, {"GPIO1", Parser::xme_identi}, {"->", Parser::xme_qual},
    {"CTRL", Parser::xme_identi}, {".", Parser::xme_qual}, {"value", Parser::xme_identi},
    {"&", Parser::xme_and}, {"0x10", Parser::xme_const}, {")", Parser::xme_closebr},
    {"==", Parser::xme_equ}, {"_state2", Parser::xme_identi}, {"[", Parser::xme_opendim},
    {"3", Parser::xme_const}, {"]", Parser::xme_closedim},
  };
  const auto& tokens = parser.GetTokenList();
  ASSERT_EQ(expected.size(), tokens.size());
  auto actual = tokens.begin();
  for(const auto& [text, type] : expected) {
    SCOPED_TRACE(text);
    EXPECT_EQ(type, actual->type);
    EXPECT_EQ(text, actual->text);
    ++actual;
  }
}

TEST(SvdCExpressionParserTest, HandlesEmptyInputAndTokensAtEndOfInput) {
  for(const auto* text : {"", " \t\r\n"}) {
    Parser parser(text);
    ASSERT_TRUE(parser.Parse());
    EXPECT_TRUE(parser.GetTokenList().empty());
  }
  const vector<ExpectedToken> cases{
    {"REGISTER_2", Parser::xme_identi}, {"42", Parser::xme_const}, {"0XFF", Parser::xme_const},
  };
  for(const auto& [text, type] : cases) {
    SCOPED_TRACE(text);
    Parser parser(text);
    ASSERT_TRUE(parser.Parse());
    const auto& tokens = parser.GetTokenList();
    ASSERT_EQ(1U, tokens.size());
    EXPECT_EQ(type, tokens.front().type);
    EXPECT_EQ(text, tokens.front().text);
  }
}

} // namespace
