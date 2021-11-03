/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CHECKCOMPONENTS_H
#define CHECKCOMPONENTS_H

#include "Validate.h"

#include "RteModel.h"

class CheckComponent
{
public:
  CheckComponent(RteGlobalModel& model) : m_rteModel(model) {};
  ~CheckComponent() {};

  RteGlobalModel& GetModel() { return m_rteModel; }
  bool CheckComp(RteItem* item);
  bool ConditionRefToDevice(RteItem* item);
  bool TestSubConditions(RteCondition* cond);

private:
  std::string m_packagePath;
  RteGlobalModel& m_rteModel;
};

class ComponentsVisitor : public RteVisitor
{
public:
  ComponentsVisitor(CheckComponent& checkComponent) : m_checkComponent(checkComponent) { };
  ~ComponentsVisitor() {};

  virtual VISIT_RESULT Visit(RteItem* item);

private:
  CheckComponent& m_checkComponent;
};

#endif // CHECKCOMPONENTS_H
