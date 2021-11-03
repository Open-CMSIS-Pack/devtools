/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CHECKCONDITIONS_H
#define CHECKCONDITIONS_H

#include "RteModel.h"

class CheckConditions
{
public:
  CheckConditions(RteGlobalModel& model) : m_model(model) {}
  ~CheckConditions() {};

  RteGlobalModel& GetModel() { return m_model; };
  void SetWorkingDir(const std::string& path) { m_workingDir = path; }
  const std::string& GetWorkingDir() { return m_workingDir; }
  bool AddDefinedCondition(RteCondition* condition);
  bool AddUsedCondition(RteCondition* condition);
  bool AddUsedCondition(const std::string& conditionId, int lineNo);
  bool CheckForUnused();
  bool TestSubConditions(RteItem* expression);
  bool IsVisited(RteCondition* cond) const { return m_visitedConditions.find(cond) != m_visitedConditions.end(); }

private:
  std::map<std::string, RteCondition*> m_definedConditions;
  std::map<std::string, RteCondition*> m_unusedConditions;
  std::list<RteCondition*> m_usedConditions;
  std::set<RteCondition*> m_visitedConditions;
  RteGlobalModel& m_model;
  std::string m_workingDir;
};

class DefinedConditionsVisitor : public RteVisitor
{
public:
  DefinedConditionsVisitor(CheckConditions* conditions);
  ~DefinedConditionsVisitor();

  virtual VISIT_RESULT Visit(RteItem* item);

private:
  CheckConditions* m_Conditions;
  RteTarget* m_target;           // for expression evaluation
  RteModel* m_filteredModel;     // needed to test conditions
};

class UsedConditionsVisitor : public RteVisitor
{
public:
  UsedConditionsVisitor(CheckConditions* conditions);
  ~UsedConditionsVisitor();

  virtual VISIT_RESULT Visit(RteItem* item);
  void GetUsedSubConditions(RteCondition* cond);

private:
  CheckConditions* m_Conditions;
};

#endif // CHECKCONDITIONS_H
