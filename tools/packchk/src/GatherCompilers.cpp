/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "GatherCompilers.h"

#include "ErrLog.h"
#include "RteModel.h"

using namespace std;

/**
 * @brief visitor class constructor
*/
GatherCompilersVisitor::GatherCompilersVisitor() :
  m_target(nullptr, &m_filteredModel, "CondTest", EMPTY_MAP)
{
}

/**
* @brief visitor class destructor
*/
GatherCompilersVisitor::~GatherCompilersVisitor()
{
}

/**
 * @brief visitor callback
 * @param item RteItem item to work on
 * @return VISIT_RESULT
*/
VISIT_RESULT GatherCompilersVisitor::Visit(RteItem* item)
{
  RteCondition* cond = dynamic_cast<RteCondition*>(item);
  if(cond && cond->IsValid()) {
    AddCompiler(cond);
  }

  return VISIT_RESULT::CONTINUE_VISIT;
}

/**
 * @brief adds all compilers found in conditions
 * @param cond condition to start from
 * @return passed / failed
*/
bool GatherCompilersVisitor::AddCompiler(RteCondition* cond)
{
  bool bOk = true;

  string filter = "Tcompiler";
  bOk = FilterConditions(filter, cond);

  return bOk;
}

/**
 * @brief returns compiler name including compiler options
 * @param compiler compiler_s object
 * @return string compiler name
*/
string GatherCompilersVisitor::GetCompilerName(const compiler_s& compiler)
{
  string compilerName = compiler.tcompiler + " [" + compiler.toptions + "]";

  return compilerName;
}

/**
 * @brief filter conditions to a given filter name
 * @param filter string name to filter
 * @param cond RteCondition condition to start from
 * @return passed / failed
*/
bool GatherCompilersVisitor::FilterConditions(const string& filter, RteCondition* cond)
{
  if(!cond || !cond->IsValid()) {
    return true;
  }

  RteItem::ConditionResult res = cond->Evaluate(m_target.GetFilterContext());
  if(res == RteItem::R_ERROR) {
    LogMsg("M384", NAME(cond->GetName()), NAME2(filter), cond->GetLineNumber());
    cond->Invalidate();
    return false;
  }

  for(auto exprItem : cond->GetChildren()) {
    RteConditionExpression* expression = dynamic_cast<RteConditionExpression*> (exprItem);
    if(!expression) {
      continue;
    }

    if(!FilterConditions(filter, (expression)->GetCondition())) {
      return false;
    }

    string result = expression->GetAttribute(filter);
    RteConditionExpression::RTEConditionExpressionType type = expression->GetExpressionType();

    if(filter == "Tcompiler" && !result.empty() && type != RteConditionExpression::DENY) {
      compiler_s compiler;
      compiler.tcompiler = result;
      compiler.toptions = expression->GetAttribute("Toptions");
      string compilerName = GetCompilerName(compiler);

      if(m_compilerMap.find(compilerName) == m_compilerMap.end()) {
        m_compilerMap[compilerName] = compiler;
      }
    }
  }

  return true;
}

