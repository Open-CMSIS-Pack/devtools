/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "SvdCExpression.h"
#include "SvdDevice.h"
#include "SvdUtils.h"
#include "ErrLog.h"

#include <sstream>
using namespace std;


SvdCExpression::SvdCExpression()
{
}

SvdCExpression::~SvdCExpression()
{
}

bool SvdCExpression::CalcExpression(SvdItem* item)
{
  const auto& expr = GetText();
  if(expr.empty()) {
    return true;
  }

  const auto lineNo = GetLineNumber();

  SvdCExpressionParser parse(expr);
  parse.Parse();

  if(!LinkSymbols(item, parse.GetTokenList(), lineNo)) {
    Invalidate();
    return false;
  }

  return true;
}

bool SvdCExpression::LinkSymbols(SvdItem* item, const SvdCExpressionParser::TokenList& tokens, int32_t lineNo)
{
  if(!item || !item->IsValid()) {
    return false;
  }

  const auto device = item->GetDevice();
  if(!device) {
    return false;
  }

  m_symbolsList.clear();
  Symbols_t symbol;
  string errText;

  for(const auto& token : tokens) {
    errText += token.text;
    symbol.token = token;

    switch(token.type) {
      case SvdCExpressionParser::xme_identi:
        symbol.searchname.push_back(token.text);
        m_symbolsList.push_back(symbol);
        break;
      case SvdCExpressionParser::xme_qual:
        if(!m_symbolsList.empty())
          m_symbolsList.pop_back();
        break;
      default:
        m_symbolsList.push_back(symbol);
        symbol.clear();
        break;
    }
  }

  auto& regList = device->GetExpressionRegistersList();

  for(auto& foundSymbol : m_symbolsList) {
    switch(foundSymbol.token.type) {
      case SvdCExpressionParser::xme_identi: {
        SvdItem* from = 0;
        string lastSearchName;
        item->GetDeriveItem(from, foundSymbol.searchname, L_UNDEF, lastSearchName);
        if(!from) {
          LogMsg("M244", NAME(lastSearchName), MSG(errText), lineNo);
          return false;
        }
        foundSymbol.svdItem = from;

        string name = foundSymbol.svdItem->GetHierarchicalNameResulting();
        if(!name.empty()) {
          if(foundSymbol.svdItem->GetSvdLevel() == L_Register) {
            regList[name] = foundSymbol.svdItem;
          }
          else if(foundSymbol.svdItem->GetSvdLevel() == L_Field) {
            SvdItem* symbolItem = foundSymbol.svdItem->GetParent()->GetParent();
            if(item) {
              regList[name] = symbolItem;
            }
          }
          else {
            LogMsg("M248", lineNo);
            return false;
          }
        }
      } break;
      default:
        break;
    }
  }

  return true;
}

string SvdCExpression::GetExpressionString()
{
  string expression;

  const auto& symbols = GetSymbolsList();
  if(symbols.empty()) {
    return SvdUtils::EMPTY_STRING;
  }

  for(const auto& symbol : symbols) {
    switch(symbol.token.type) {
      case SvdCExpressionParser::xme_identi: {
        SvdItem* item = symbol.svdItem;
        if(!item) {
          return SvdUtils::EMPTY_STRING;     // skip generating disable condition. Should not be reached, as LinkSymbols() already generates an error
        }

        expression += CreateObjectExpression(item);
        break;
      }
      default:
        expression += symbol.token.text;
        break;
    }

    if(!expression.empty()) {
      expression += " ";
    }
  }

  return expression;
}

string SvdCExpression::CreateObjectExpression(SvdItem* item)
{
  string expression;

  switch(item->GetSvdLevel()) {
    case L_Register:
      expression = CreateRegisterExpression(item);
      break;
    case L_Field:
      expression = CreateFieldExpression(item);
      break;
    default:
      expression = "<Error in expression Object>";
      LogMsg("M247", GetLineNumber());
      break;
  }

  return expression;
}

string SvdCExpression::CreateRegisterExpression(SvdItem* item)
{
  return item->GetHierarchicalName();
}

string SvdCExpression::CreateFieldExpression(SvdItem* item)
{
  const auto field = dynamic_cast<SvdField*>(item);
  if(!field) {
    LogMsg("M103", VAL("REF", "Item is not an SvdField"));
    return SvdUtils::EMPTY_STRING;
  }

  const auto regName = field->GetParentRegisterNameHierarchical();
  const auto firstBit = (uint32_t)field->GetOffset();
  const auto bitWidth = field->GetEffectiveBitWidth();
  const auto bitMaxNum = (uint32_t) ((((uint64_t)(1) << bitWidth) -1));

  std::stringstream ss;
  ss << "0x" << std::hex << bitMaxNum;

  string expression;
  expression += "(";
  expression += regName;
  expression += " & (";
  expression += ss.str();
  expression += " << ";
  expression += to_string(firstBit);
  expression += "))";

  return expression;
}

bool SvdCExpression::CheckItem()
{
  if(!IsValid()) {
    return true;
  }

  //const auto& name = GetName();
  //const auto lineNo = GetLineNumber();

  // currently no checks done

  return true;
}
