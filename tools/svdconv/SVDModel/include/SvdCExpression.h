/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SvdCExpression_H
#define SvdCExpression_H

#include "SvdItem.h"
#include "SvdField.h"
#include "XMLTree.h"
#include "SvdCExpressionParser.h"

#include <string>
#include <map>
#include <list>


class SvdCExpression : public SvdElement
{
public:
  SvdCExpression();
  virtual ~SvdCExpression();

  virtual bool CheckItem();

  class Symbols_t {
  public:
    SvdCExpressionParser::token_s token;
    SvdItem* svdItem;
    std::list<std::string> searchname;

    Symbols_t() {
      clear();
    }
    void clear() {
      svdItem = nullptr;
      searchname.clear();
      token.text.clear();
      token.type = SvdCExpressionParser::xme_what;
    }
  };
  using SymbolsList = std::list<Symbols_t>;

  using RegList = std::map<std::string, SvdItem*>;

  bool CalcExpression(SvdItem* item);
  bool LinkSymbols(SvdItem* item, const SvdCExpressionParser::TokenList& tokens, int32_t lineNo);
  std::string GetExpressionString();
  std::string CreateObjectExpression(SvdItem* item);
  std::string CreateRegisterExpression(SvdItem* item);
  std::string CreateFieldExpression(SvdItem* item);
  const SymbolsList& GetSymbolsList() { return m_symbolsList;  }

protected:

private:
  SymbolsList m_symbolsList;
};

#endif // SvdCExpression_H
