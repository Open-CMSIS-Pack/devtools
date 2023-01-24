/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef GATHERCOMPILERS_H
#define GATHERCOMPILERS_H

#include "RteModel.h"

struct compiler_s {
  std::string tcompiler;
  std::string toptions;
};

class GatherCompilersVisitor : public RteVisitor
{
public:
  GatherCompilersVisitor();
  ~GatherCompilersVisitor();

  virtual VISIT_RESULT Visit(RteItem* item);
  const std::map<std::string, compiler_s>& GetCompilerList() { return m_compilerMap; };
  static std::string GetCompilerName(const compiler_s& compiler);

private:
  bool AddCompiler(RteCondition* cond);
  bool FilterConditions(const std::string& filter, RteCondition* cond);

  std::map<std::string, std::string> EMPTY_MAP;
  std::map<std::string, compiler_s>   m_compilerMap;
  RteModel m_filteredModel;     // needed to test conditions
  RteTarget m_target;           // for expression evaluation
};

#endif //GATHERCOMPILERS_H
