/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "SvdCExpressionParser.h"
#include "SvdDevice.h"
#include "ErrLog.h"

using namespace std;

const string SvdCExpressionParser::symbol = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_";
const string SvdCExpressionParser::number = "0123456789abcdefABCDEFxX";


SvdCExpressionParser::SvdCExpressionParser(const string& exprStr) :
  m_exprStr(exprStr),
  m_strPos(0)
{
}

SvdCExpressionParser::~SvdCExpressionParser()
{
}

SvdCExpressionParser::XETYPE SvdCExpressionParser::GetChar()
{
  if(m_strPos >= m_exprStr.length()) {
    return xme_eoi;
  }

  return (XETYPE)m_exprStr[m_strPos++];
}

SvdCExpressionParser::XETYPE SvdCExpressionParser::PutChar()
{
  if(!m_strPos) {
    return xme_eoi;
  }

  return (XETYPE)m_exprStr[--m_strPos];
}

SvdCExpressionParser::XETYPE SvdCExpressionParser::PeekChar()
{
  if(m_strPos >= m_exprStr.length()) {
    return xme_eoi;
  }

  return (XETYPE)m_exprStr[m_strPos];
}

SvdCExpressionParser::XETYPE SvdCExpressionParser::SkipWhite()
{
  XETYPE c = PeekChar();

  do {
    switch((uint32_t)c) {
      case ' ':
      case '\t':
      case '\r':
      case '\n':
        GetChar();
        c = PeekChar();
        continue;
      case xme_eoi:
        return xme_eoi;
      default:
        return xme_what;
    }
  } while(1);
}

SvdCExpressionParser::XETYPE SvdCExpressionParser::GetToken(string& tokenTxt, const string& allowedSymbols)
{
  do {
    int c = GetChar();
    if(c == xme_eoi) {
      return xme_eoi;
    }

    if(allowedSymbols.find_first_of(c) == string::npos) {
      PutChar();
      break;
    }

    tokenTxt += c;
  } while(1);

  return xme_what;
}

SvdCExpressionParser::XETYPE SvdCExpressionParser::GetNextToken(token_t& token)
{
  int c = 0;

  if(SkipWhite() == xme_eoi) {
    return xme_eoi;
  }

  c = PeekChar();
  if(symbol.find_first_of(c) != string::npos) {
    token.type = xme_identi;
    GetToken(token.text, symbol + number);
  }
  else if(number.find_first_of(c) != string::npos) {
    token.type = xme_const;
    GetToken(token.text, number);
  }
  else if(c == '.') {
    token.type = xme_qual;
    token.text = c;
    GetChar();
  }
  else if(c == '-') {
    token.type = xme_minus;
    token.text = c;
    GetChar();
    c = PeekChar();
    if(c == '>') {
      token.type = xme_qual;
      token.text += c;
      GetChar();
    }
  }
  else if(c == '(') {
    token.type = xme_openbr;
    token.text = c;
    GetChar();
  }
  else if(c == ')') {
    token.type = xme_closebr;
    token.text = c;
    GetChar();
  }
  else if(c == '[') {
    token.type = xme_opendim;
    token.text = c;
    GetChar();
  }
  else if(c == ']') {
    token.type = xme_closedim;
    token.text = c;
    GetChar();
  }
  else if(c == '+') {
    token.type = xme_plus;
    token.text = c;
    GetChar();
  }
  else if(c == '*') {
    token.type = xme_mul;
    token.text = c;
    GetChar();
  }
  else if(c == '/') {
    token.type = xme_div;
    token.text = c;
    GetChar();
  }
  else if(c == '^') {
    token.type = xme_xor;
    token.text = c;
    GetChar();
  }
  else if(c == '!') {
    token.type = xme_not;
    token.text = c;
    GetChar();
  }
  else if(c == '~') {
    token.type = xme_compl;
    token.text = c;
    GetChar();
  }
  else if(c == '<') {
    token.type = xme_lt;
    token.text = c;
    GetChar();
    c = PeekChar();
    if(c == '<') {
      token.type = xme_lsh;
      token.text += c;
      GetChar();
    }
  }
  else if(c == '>') {
    token.type = xme_lt;
    token.text = c;
    GetChar();
    c = PeekChar();
    if(c == '>') {
      token.type = xme_rsh;
      token.text += c;
      GetChar();
    }
  }
  else if(c == '&') {
    token.type = xme_and;
    token.text = c;
    GetChar();
    c = PeekChar();
    if(c == '&') {
      token.type = xme_land;
      token.text += c;
      GetChar();
    }
  }
  else if(c == '|') {
    token.type = xme_or;
    token.text = c;
    GetChar();
    c = PeekChar();
    if(c == '|') {
      token.type = xme_lor;
      token.text += c;
      GetChar();
    }
  }
  else if(c == '=') {
    token.type = xme_asn;
    token.text = c;
    GetChar();
    c = PeekChar();
    if(c == '=') {
      token.type = xme_equ;
      token.text += c;
      GetChar();
    }
  }
  else {
    token.type = xme_what;
    token.text = c;
  }

  return xme_what;
}

bool SvdCExpressionParser::Parse()
{
  XETYPE type = xme_what;
  m_tokenList.clear();

  do {
    token_t token;
    type = GetNextToken(token);
    if(type != xme_eoi) {
      m_tokenList.push_back(token);
    }
  } while(type != xme_eoi);

  return true;
}
