/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "CheckConditions.h"

#include "RteModel.h"
#include "RteTarget.h"
#include "ErrLog.h"

using namespace std;

/**
 * @brief visitor class constructor for defined conditions
 * @param conditions
*/
DefinedConditionsVisitor::DefinedConditionsVisitor(CheckConditions* conditions) :
  m_Conditions(conditions)
{
  static const map<string, string> EMPTY_MAP;
  m_filteredModel = new RteModel();
  m_target = new RteTarget(&conditions->GetModel(), m_filteredModel, "CondTest", EMPTY_MAP);
  m_target->SetTargetSupported(true);
  m_target->UpdateFilterModel();
};

/**
 * @brief visitor class destructor for defined conditions
*/
DefinedConditionsVisitor::~DefinedConditionsVisitor()
{
  delete m_target;
  delete m_filteredModel;
}

/**
* @brief visitor callback for defined condition items
* @param item one element from container
* @return VISIT_RESULT
*/
VISIT_RESULT DefinedConditionsVisitor::Visit(RteItem* item)
{
  if(!m_target) {
    return VISIT_RESULT::CANCEL_VISIT;
  }

  RteCondition* cond = dynamic_cast<RteCondition*>(item);
  if(cond) {
    RteItem::ConditionResult result = cond->Evaluate(m_target->GetFilterContext());
    if(result == RteItem::R_ERROR) {
      LogMsg("M390", NAME(cond->GetName()), MSG("Skipping condition for further checks."), cond->GetLineNumber());
      cond->Invalidate();
      return VISIT_RESULT::CONTINUE_VISIT;
    }

    m_Conditions->AddDefinedCondition(cond);

    return VISIT_RESULT::CONTINUE_VISIT;
  }


  RteConditionExpression* expr = dynamic_cast<RteConditionExpression*>(item);
  if(expr && expr->IsDependencyExpression()) {
    set<RteComponentAggregate*> components;
    m_target->GetComponentAggregates(*expr, components);
    if (components.empty()) {
      LogMsg("M317", NAME(expr->GetParent()->GetID()), NAME2(expr->GetID()), expr->GetLineNumber());
    }
  }

  return VISIT_RESULT::CONTINUE_VISIT;
}

/**
* @brief visitor class constructor for used conditions
* @param conditions
*/
UsedConditionsVisitor::UsedConditionsVisitor(CheckConditions* conditions) :
  m_Conditions(conditions)
{
};

/**
* @brief visitor class destructor for used conditions
*/
UsedConditionsVisitor::~UsedConditionsVisitor()
{
}

/**
* @brief visitor callback for used condition items
* @param item one element from container
* @return VISIT_RESULT
*/
VISIT_RESULT UsedConditionsVisitor::Visit(RteItem* item)
{
  RteCondition* cond = item->GetCondition();
  if(!cond) {
    const string& condId = item->GetConditionID();
    if(!condId.empty()) {
      m_Conditions->AddUsedCondition(condId, item->GetLineNumber());
    }
    return VISIT_RESULT::CONTINUE_VISIT;
  }

  if(!cond->IsValid()) {
    return VISIT_RESULT::CONTINUE_VISIT;
  }

  if(cond != item) {                // item is not a condition, but something that can use a condition
    GetUsedSubConditions(cond);
    const string& condId = item->GetConditionID();

    m_Conditions->AddUsedCondition(condId, item->GetLineNumber());
  }

  return VISIT_RESULT::CONTINUE_VISIT;
}

/**
 * @brief recursively iterate through conditions
 * @param cond RteCondition
*/
void UsedConditionsVisitor::GetUsedSubConditions(RteCondition* cond)
{
  if(!cond) {
    return;
  }
  if(m_Conditions->IsVisited(cond)) {
    return;
  }

  m_Conditions->AddUsedCondition(cond);

  for(auto expression : cond->GetChildren()) {
    RteCondition* c = expression->GetCondition();
    if(c) {
      GetUsedSubConditions(c);
    }
    else {
      m_Conditions->TestSubConditions(expression);
    }
  }
}

/**
 * @brief Iterate and test through all conditions starting from an RteItem expression element
 * @param expression RteItem
 * @return passed / failed
 */
bool CheckConditions::TestSubConditions(RteItem* expression)
{
  if(!expression) {
    return false;
  }

  string pname = expression->GetAttribute("Pname");
  string dname = expression->GetAttribute("Dname");
  int lineNo = expression->GetLineNumber();

  if(dname.empty() && !pname.empty()) {
    LogMsg("M398", NAME(pname), lineNo);
  }

  if(dname.empty()) {
    return true;
  }

  RteGlobalModel& model = GetModel();
  string dvendor = expression->GetAttribute("Dvendor");

  LogMsg("M094", COND(dname));
  list<RteDevice*> devices;
  model.GetDevices(devices, dname, dvendor, RteDeviceItem::VARIANT); // get devices including variants
  if(!devices.empty()) {
    LogMsg("M010");
    return true;
  }
  else {
    if(dvendor.empty()) {
      dvendor = "<no vendor>";
    }
    LogMsg("M364", COND(expression->GetName()), VENDOR(dvendor), MCU(dname), lineNo);
  }

  return false;
}

/**
 * @brief adds defined condition to a container to test against
 * @param condition RteCondition
 * @return passed / failed
*/
bool CheckConditions::AddDefinedCondition(RteCondition* condition)
{
  bool ok = true;
  const string id = condition->GetID();

  LogMsg("M082", COND(condition->GetName()));

  RteItem* existingCond = 0;
  auto itexisting = m_definedConditions.find(id);
  if(itexisting != m_definedConditions.end()) {
    existingCond = itexisting->second;
    LogMsg("M330", COND(existingCond->GetName()), LINE(existingCond->GetLineNumber()), condition->GetLineNumber());
  }

  m_definedConditions[id] = condition;

  if(ok) {
    LogMsg("M010");
  }

  return ok;
}

/**
* @brief adds used condition to a container to test against
* @param condition RteCondition
* @return passed / failed
*/
bool CheckConditions::AddUsedCondition(RteCondition* condition)
{
  bool ok = true;
  const string id = condition->GetName();

  LogMsg("M083", COND(condition->GetName()));

  string idUsed = condition->GetID();

  m_visitedConditions.insert(condition);

  if(m_definedConditions.find(idUsed) == m_definedConditions.end()) {
    LogMsg("M332", COND(condition->GetName()), condition->GetLineNumber());
  }
  else {
    m_usedConditions.push_back(condition);      // check same conditions on different occurring on line numbers
    LogMsg("M010");
  }

  return ok;
}

/**
 * @brief adds used condition by id to a container to test against
 * @param id string, calculated id
 * @param lineNo line number for error reporting
 * @return passed / failed
*/
bool CheckConditions::AddUsedCondition(const string& id, int lineNo)
{
  bool ok = true;

  LogMsg("M083", COND(id));

  if(m_definedConditions.find(id) == m_definedConditions.end()) {
    LogMsg("M332", COND(id), lineNo);
    ok = false;
  }

  if(ok) {
    LogMsg("M010");
  }

  return ok;
}

/**
 * @brief check if there are unused conditions
 * @return passed / failed
*/
bool CheckConditions::CheckForUnused()
{
  bool ok = true;
  m_unusedConditions = m_definedConditions;

  for(auto cond : m_usedConditions) {
    string idUsed = cond->GetID();
    if(m_definedConditions.find(idUsed) != m_definedConditions.end()) {
      m_unusedConditions.erase(idUsed);
    }
    else {
      ok = false;
    }
  }

  if(!m_unusedConditions.empty()) {
    map<int, RteCondition*> printList;
    for(auto &[key, cond] : m_unusedConditions) {
      printList[cond->GetLineNumber()] = cond;
    }

    for(auto &[key, cond] : printList) {
      if(!cond) {
        continue;
      }

      LogMsg("M331", COND(cond->GetName()), cond->GetLineNumber());
    }
  }

  return ok;
}
