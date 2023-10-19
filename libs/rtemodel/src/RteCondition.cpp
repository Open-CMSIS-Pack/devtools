/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteCondition.cpp
* @brief CMSIS RTE Data model
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/
#include "RteCondition.h"

#include "RtePackage.h"
#include "RteModel.h"

#include "XMLTree.h"

#include <sstream>
using namespace std;

const std::string RteConditionExpression::ACCEPT_TAG("accept");
const std::string RteConditionExpression::DENY_TAG("deny");
const std::string RteConditionExpression::REQUIRE_TAG("require");
unsigned RteCondition::s_uVerboseFlags = 0;



RteConditionExpression::RteConditionExpression(RteCondition* parent) :
  RteItem(parent),
  m_domain(0)
{
};

RteConditionExpression::RteConditionExpression(const string& tag, RteCondition* parent) :
  RteItem(tag, parent),
  m_domain(0)
{
};


RteConditionExpression::~RteConditionExpression()
{
}

bool RteConditionExpression::IsDenyExpression() const {
  return GetExpressionType() == DENY;
}

bool RteConditionExpression::IsDependencyExpression() const
{
  return m_domain == COMPONENT_EXPRESSION;
}

bool RteConditionExpression::IsDeviceExpression() const
{
  return m_domain == DEVICE_EXPRESSION;
}

bool RteConditionExpression::IsDeviceDependent() const
{
  if (IsDeviceExpression()) {
    return !GetAttribute("Dname").empty();
  }
  if (GetExpressionDomain() == CONDITION_EXPRESSION) {
    return RteItem::IsDeviceDependent();
  }
  return false;
}

bool RteConditionExpression::IsBoardExpression() const
{
  return m_domain == BOARD_EXPRESSION;
}

bool RteConditionExpression::IsBoardDependent() const
{
  if (IsBoardExpression()) {
    return !GetAttribute("Bname").empty();
  }
  if (GetExpressionDomain() == CONDITION_EXPRESSION) {
    return RteItem::IsBoardDependent();
  }
  return false;
}


string RteConditionExpression::ConstructID()
{
  auto it = m_attributes.begin();
  if (it != m_attributes.end()) {
    m_domain = it->first.at(0);
    if (m_domain == 'P') {
      m_domain = DEVICE_EXPRESSION;
    }
  }
  if (IsDependencyExpression()) {
    return GetDependencyExpressionID();
  } else {
    return GetTag() + " " + GetAttributesString();
  }
}


string RteConditionExpression::GetDisplayName() const
{
  return GetID();
}

const set<RteComponentAggregate*>& RteConditionExpression::GetComponentAggregates(RteTarget* target) const
{
  RteDependencySolver* solver = target->GetDependencySolver();
  return solver->GetComponentAggregates(const_cast<RteConditionExpression*>(this));
}

RteComponentAggregate* RteConditionExpression::GetSingleComponentAggregate(RteTarget* target) const
{
  return GetSingleComponentAggregate(target, GetComponentAggregates(target));
}

RteComponentAggregate* RteConditionExpression::GetSingleComponentAggregate(RteTarget* target, const set<RteComponentAggregate*>& components)
{
  if (components.empty()) {
    return nullptr;
  }
  RtePackage* devicePack = target->GetEffectiveDevicePackage();

  RteComponentAggregate* singleAggregate = nullptr;
  RtePackage* singleAggregatePack = nullptr;
  int nAggregates = 0;
  for (auto a : components) {
    if (a->IsFiltered()) {
      RtePackage* aPack = a->GetPackage();
      if (!singleAggregate) {
        singleAggregate = a;
        singleAggregatePack = aPack;
        nAggregates = 1;
      } else if (a != singleAggregate) {
        if (aPack == devicePack) {
          if (singleAggregatePack == devicePack) {
            // more than one aggregate comes from device package
            return nullptr;
          }
          // aggregate comes from the same pack as device => it has preference
          singleAggregate = a;
          singleAggregatePack = aPack;
          nAggregates = 1;
        } else if (singleAggregatePack != devicePack) {
          nAggregates++;
        }
      }
    }
  }
  return nAggregates == 1 ? singleAggregate : nullptr;
}


bool RteConditionExpression::Validate()
{
  m_bValid = RteItem::Validate();
  if (m_attributes.empty()) {
    string msg = " '";
    msg += GetDisplayName() + "': Empty expression";
    m_errors.push_back(msg);
    m_bValid = false;
    return false;
  }

  set<char> domains;
  bool bComponent = false;
  for (auto [a, v] : m_attributes) {
    char ch = a.at(0);
    if (ch == COMPONENT_EXPRESSION) {
      bComponent = true;
    } else if (ch == 'P') { // processor properties belong to Device domain
      ch = 'D';
    }
    domains.insert(ch);
  }
  if (domains.size() > 1) {
    string msg = " '";
    msg += GetDisplayName() + "': mixed 'B', 'C', 'D', 'T' or 'condition' attributes";
    m_errors.push_back(msg);
    m_bValid = false;
  }
  if (bComponent && (GetCclassName().empty() || GetCgroupName().empty())) {
    string msg = " '";
    msg += GetDisplayName() + "': Cclass or Cgroup attribute is missing in expression";
    m_errors.push_back(msg);
    m_bValid = false;
  }
  return m_bValid;
}


RteItem::ConditionResult RteConditionExpression::Evaluate(RteConditionContext* context)
{
  return context->EvaluateExpression(this);
}

RteItem::ConditionResult RteConditionExpression::GetConditionResult(RteConditionContext* context) const
{
  return context->GetConditionResult(const_cast<RteConditionExpression*>(this));
}

RteItem::ConditionResult RteConditionExpression::EvaluateExpression(RteTarget* target)
{
  if (!target)
    return FAILED;
  const map<string, string>& attributes = target->GetAttributes();
  for (auto [a, v] : m_attributes) {
    if (a.empty())
      continue;
    if (a.at(0) == 'C') {
      continue; // skip check for Cclass, Cgroup, Csub, ...
    }
    if (a == "condition") {
      continue; // special handling for referred condition
    }
    auto ita = attributes.find(a);
    if (ita != attributes.end()) {
      const string& va = ita->second;
      if (a == "Dvendor" || a == "Bvendor" || a == "vendor") {
        if (!DeviceVendor::Match(va, v))
          return FAILED;
        continue;
      }
      if (a == "Dcdecp") {
        unsigned long uva = RteUtils::ToUL(va);
        unsigned long uv = RteUtils::ToUL(v);
        if ((uva & uv) == 0) // alternatively we have considered if ((uva & uv) == uv)
          return FAILED;
        continue;
      }
      // all other attributes
      if (!WildCards::Match(va, v))
        return FAILED;
    } else if (GetExpressionType() == DENY) {
      return FAILED; // for denied attributes, all must be given
    }
  }
  return FULFILLED;
}


RteItem::ConditionResult RteConditionExpression::GetDepsResult(map<const RteItem*, RteDependencyResult>& results, RteTarget* target) const
{
  ConditionResult r = RteDependencyResult::GetResult(this, results);
  if (r != UNDEFINED)
    return r;
  RteItem::ConditionResult result = GetConditionResult(target->GetDependencySolver());
  if (result < FULFILLED && result > FAILED) {
    RteCondition* cond = GetCondition();
    if (cond) {
      return cond->GetDepsResult(results, target);
    } else if (IsDependencyExpression()) {
      // check if we already have result
      if (HasDepsResult(results))
        return result;
      RteDependencyResult depRes(this, result);
      const set<RteComponentAggregate*>& components = GetComponentAggregates(target);
      for (auto a : components) {
        if(a) {
          depRes.AddComponentAggregate(a);
        }
      }
      results[this] = depRes;
    }
  }
  return result;
}


bool RteConditionExpression::HasDepsResult(map<const RteItem*, RteDependencyResult>& results) const
{
  if (results.empty())
    return false;
  if (results.find(this) != results.end())
    return true;

  for (auto [item, res] : results) {
    if (item->GetTag() == GetTag() && item->EqualAttributes(this))
      return true;
  }

  return false;
}


RteItem::ConditionResult RteDenyExpression::Evaluate(RteConditionContext* context)
{
  ConditionResult result = RteConditionExpression::Evaluate(context);
  if (IsDependencyExpression()) {
    return result; // already denied
  }

  if (context->IsDependencyContext()) {
    switch (result) {
    case FULFILLED:
      return INCOMPATIBLE;

    case INSTALLED:
    case INCOMPATIBLE:
    case INCOMPATIBLE_VARIANT:
    case INCOMPATIBLE_VERSION:
    case SELECTABLE:
    case UNAVAILABLE:
    case UNAVAILABLE_PACK:
    case MISSING:
      return FULFILLED;
    case R_ERROR:
    case IGNORED:
      break;
    default:
      return IGNORED;
    };
  } else { // filtering
    switch (result) {
    case FULFILLED:
      return FAILED;
    case FAILED:
      return FULFILLED;
    case R_ERROR:
    case IGNORED:
    default:
      break;
    };
  }
  return result;
}


RteCondition::RteCondition(RteItem* parent) :
  RteItem(parent),
  m_bDeviceDependent(-1),
  m_bBoardDependent(-1),
  m_bInCheck(false)
{
}

RteCondition::~RteCondition()
{
  RteCondition::Clear();
}


const string& RteCondition::GetName() const
{
  return GetAttribute("id");
}

string RteCondition::GetDisplayName() const
{
  string s = GetTag() + " '" + GetName() + "'";
  return s;
}

bool RteCondition::Validate()
{
  m_bValid = RteItem::Validate();
  if (!m_bValid) {
    string msg = CreateErrorString("error", "502", "error(s) in condition definition:");
    m_errors.push_front(msg);
  }
  m_bInCheck = false;
  if (!ValidateRecursion()) {
    string msg = CreateErrorString("error", "503", "direct or indirect recursion detected");
    m_errors.push_back(msg);
    m_bValid = false;
  }
  return m_bValid;
}

bool RteCondition::ValidateRecursion()
{
  if (m_bInCheck) {
    return false;
  }
  m_bInCheck = true;
  bool noRecursion = true;
  for (auto child : GetChildren()) {
    RteConditionExpression* expr = dynamic_cast<RteConditionExpression*>(child);
    if (!expr)
      continue;

    RteCondition* cond = expr->GetCondition();
    if (!cond)
      continue;

    if (!cond->ValidateRecursion()) { // will return false if m_bInCheck == true
      noRecursion = false;
      break;
    }
  }
  m_bInCheck = false;
  return noRecursion;
}

void RteCondition::CalcDeviceAndBoardDependentFlags()
{
  if (m_bDeviceDependent < 0 || m_bBoardDependent < 0) { // not yet calculated
    m_bDeviceDependent = 0;
    m_bBoardDependent = 0;
    if (m_bInCheck) { // to prevent recursion
      return;
    }
    m_bInCheck = true;
    for (auto child : GetChildren()) {
      RteConditionExpression* expr = dynamic_cast<RteConditionExpression*>(child);
      if (!expr) {
        continue;
      }
      if (expr->IsDeviceDependent()) {
        m_bDeviceDependent = 1;
      }
      if (expr->IsBoardDependent()) {
        m_bBoardDependent = 1;
      }
      if (m_bDeviceDependent > 0 && m_bBoardDependent > 0) {
        break;
      }
    }
    m_bInCheck = false;
  }
}

bool RteCondition::IsDeviceDependent() const
{
  if (m_bDeviceDependent < 0) {
    RteCondition* that = const_cast<RteCondition*>(this);
    that->CalcDeviceAndBoardDependentFlags();
  }
  return m_bDeviceDependent > 0;
}

bool RteCondition::IsBoardDependent() const
{
  if (m_bBoardDependent < 0) {
    RteCondition* that = const_cast<RteCondition*>(this);
    that->CalcDeviceAndBoardDependentFlags();
  }
  return m_bBoardDependent > 0;
}


RteItem::ConditionResult RteCondition::GetDepsResult(map<const RteItem*, RteDependencyResult>& results, RteTarget* target) const
{
  RteDependencySolver* solver = target->GetDependencySolver();
  RteItem::ConditionResult conditionResult = GetConditionResult(solver);
  ConditionResult resultAccept = FAILED;
  if (conditionResult < FULFILLED && conditionResult > FAILED) {
    bool hasAcceptConditions = false;
    // first collect all results from require and deny expressions
    for (auto child : GetChildren()) {
      RteConditionExpression* expr = dynamic_cast<RteConditionExpression*>(child);
      if (!expr)
        continue;
      if (expr->GetExpressionType() == RteConditionExpression::ACCEPT) {
        hasAcceptConditions = true;
        ConditionResult res = solver->GetConditionResult(expr);
        if (res > resultAccept) {
          resultAccept = res;
        }
        continue;
      }
      expr->GetDepsResult(results, target);
    }

    if (hasAcceptConditions) {
      // now collect results of accept expressions
      // select only those with the results equal to acceptResult or the condition result
      map<const RteItem*, RteDependencyResult> acceptResults;
      for (auto child : GetChildren()) {
        RteConditionExpression* expr = dynamic_cast<RteConditionExpression*>(child);
        if (!expr || expr->GetExpressionType() != RteConditionExpression::ACCEPT)
          continue;
        ConditionResult res = solver->GetConditionResult(expr);
        if (res == resultAccept || res == conditionResult) {
          expr->GetDepsResult(acceptResults, target);
        }
      }
      size_t nTotalAggregates = 0; // check is multiple selection is possible
      if (!acceptResults.empty()) {
        for (auto& [item, depRes] : acceptResults) {
          nTotalAggregates += depRes.GetComponentAggregates().size();
        }
        for (auto& [item, depRes] : acceptResults) {
          if (nTotalAggregates > 1) {
            depRes.SetMultiple(true);
          }
          results[item] = depRes;
        }
      }
    }
  }
  return conditionResult;
}


RteItem::ConditionResult RteCondition::Evaluate(RteConditionContext* context)
{
  if (IsEvaluating(context)) {
    return R_ERROR; // recursion error
  }
  SetEvaluating(context, true);
  RteItem::ConditionResult result = EvaluateCondition(context);
  SetEvaluating(context, false);
  return result;
}

bool RteCondition::IsEvaluating(RteConditionContext* context) const
{
  return m_evaluating.find(context) != m_evaluating.end();
}

void RteCondition::SetEvaluating(RteConditionContext* context, bool evaluating)
{
  if (evaluating) {
    m_evaluating.insert(context);
  } else {
    m_evaluating.erase(context);
  }
}


RteItem::ConditionResult RteCondition::GetConditionResult(RteConditionContext* context) const
{
  return context->GetConditionResult(const_cast<RteCondition*>(this));
}


RteItem::ConditionResult RteCondition::EvaluateCondition(RteConditionContext* context) {
  return context->EvaluateCondition(this);
}


RteCondition* RteCondition::GetCondition() const
{
  return const_cast<RteCondition*>(this);
}

RteCondition* RteCondition::GetCondition(const string& id) const
{
  if (id == GetName())
    return GetCondition();
  return RteItem::GetCondition(id);
}

RteItem* RteCondition::CreateItem(const std::string& tag)
{
  if (tag == "accept") {
    return new RteAcceptExpression(this);
  } else if (tag == "require") {
    return new RteRequireExpression(this);
  } else if (tag == "deny") {
    return new RteDenyExpression(this);
  }
  return RteItem::CreateItem(tag);
}


RteConditionContainer::RteConditionContainer(RteItem* parent) :
  RteItem(parent)
{
}

RteItem* RteConditionContainer::CreateItem(const std::string& tag)
{
  if (tag == "condition") {
    return new RteCondition(this);
  }
  return RteItem::CreateItem(tag);
}


RteDependencyResult::RteDependencyResult(const RteItem* item, RteItem::ConditionResult result) :
  m_bMultiple(false),
  m_item(item),
  m_result(result)
{
};
RteDependencyResult::~RteDependencyResult()
{
  Clear();
};


void RteDependencyResult::Clear()
{
  m_result = RteItem::UNDEFINED;
  m_aggregates.clear();
  m_results.clear();
}


string RteDependencyResult::GetDisplayName() const
{
  string name;
  const RteComponent* c = dynamic_cast<const RteComponent*>(m_item);
  const RteComponentInstance* ci = dynamic_cast<const RteComponentInstance*>(m_item);
  const RteComponentAggregate* a = dynamic_cast<const RteComponentAggregate*>(m_item);
  if (c) {
    name = c->GetComponentAggregateID();
  } else if (a) {
    name = a->GetFullDisplayName();
  } else if (ci) {
    name = ci->GetFullDisplayName();
  } else if (m_item && m_item == m_item->GetModel()) { // [TdB: 07.07.2015] m_item can be nullptr pointer
    name = "Validate Run Time Environment";
  } else if (m_item) {
    name = m_item->GetDisplayName();
  }
  return name;
}


string RteDependencyResult::GetMessageText() const
{
  const RteComponent* c = dynamic_cast<const RteComponent*>(m_item);
  string message;
  // component or api
  if (c)
  {
    // check is we have several types of errors/warnings
    bool unresolved = false;
    bool conflicted = false;
    for (auto [item, depRes] : m_results) {
      RteItem::ConditionResult res = depRes.GetResult();
      switch (res) {
      case RteItem::INSTALLED:
      case RteItem::SELECTABLE:
      case RteItem::UNAVAILABLE:
      case RteItem::UNAVAILABLE_PACK:
      case RteItem::MISSING:
        unresolved = true;
        break;

      case RteItem::CONFLICT:
      case RteItem::INCOMPATIBLE:
      case RteItem::INCOMPATIBLE_VERSION:
      case RteItem::INCOMPATIBLE_VARIANT:
        conflicted = true;
        break;
      default:
        break;
      }
    }

    if (unresolved && conflicted) {
      message = "Component conflicts with other selected components and has unresolved dependencies";
      return message;
    }

    switch (m_result) {
    case RteItem::CONFLICT:
      message = "Conflict, select exactly one component from list:";
      break;
    case RteItem::INSTALLED:
    case RteItem::SELECTABLE:
    case RteItem::MISSING:
    case RteItem::UNAVAILABLE:
    case RteItem::UNAVAILABLE_PACK:
      message = "Additional software components required";
      break;
    case RteItem::INCOMPATIBLE:
      message = "Component is incompatible with other selected components";
      break;
    case RteItem::INCOMPATIBLE_VERSION:
      message = "Component is incompatible with versions of other selected components";
      break;
    case RteItem::INCOMPATIBLE_VARIANT:
      message = "Incompatible variant is selected";
      break;

    default:
      break;
    };
  } else if (m_item && m_item->GetModel() == m_item) {
    message = "Errors/Warnings detected:";
  } else {
    switch (m_result) {
    case RteItem::INSTALLED:
      message = "Select bundle and component from list";
      break;
    case RteItem::SELECTABLE:
      message = "Select component from list";
      break;
    case RteItem::MISSING:
      message = "Install missing component";
      break;
    case RteItem::UNAVAILABLE:
      message = "Install missing component or change target settings";
      break;
    case RteItem::UNAVAILABLE_PACK:
      message = "Update pack selection";
      break;
    case RteItem::MISSING_API:
      message = "API is missing";
      break;
    case RteItem::MISSING_API_VERSION:
      message = "API with required or compatible version is missing";
      break;
    case RteItem::CONFLICT:
      message = "Conflict, select exactly one component from list";
      break;
    case RteItem::INCOMPATIBLE:
      message = "Select compatible component or deselect incompatible one";
      break;
    case RteItem::INCOMPATIBLE_VERSION:
      message = "Select compatible component version";
      break;
    case RteItem::INCOMPATIBLE_VARIANT:
      message = "Select compatible component variant";
      break;
    default:
      break;
    }
  }
  return message;
}


string RteDependencyResult::GetErrorNum() const
{
  string errNum;
  const RteComponent* c = dynamic_cast<const RteComponent*>(m_item);
  if (c) {
    switch (m_result) {
    case RteItem::INSTALLED:
      errNum = "510";
      break;
    case RteItem::SELECTABLE:
      errNum = "515";
      break;
    case RteItem::MISSING:
      errNum = "511";
      break;
    case RteItem::CONFLICT:
      errNum = "512";
      break;
    case RteItem::INCOMPATIBLE:
      errNum = "513";
      break;
    case RteItem::INCOMPATIBLE_VERSION:
      errNum = "514";
      break;
    case RteItem::INCOMPATIBLE_VARIANT:
      errNum = "516";
      break;
    default:
      break;
    };
  }
  return errNum;
}

string RteDependencyResult::GetSeverity() const
{
  string severity;
  const RteComponent* c = dynamic_cast<const RteComponent*>(m_item);
  if (c) {
    if (m_result == RteItem::INSTALLED || m_result == RteItem::SELECTABLE)
      severity = "warning";
    else
      severity = "error";
  }
  return severity;
}


string RteDependencyResult::GetOutputMessage() const
{
  string output;
  const RteComponent* c = dynamic_cast<const RteComponent*>(m_item);
  if (c)
  {
    output = "'";
    output += c->GetFullDisplayName();

    output += "': " + GetSeverity() + " #" + GetErrorNum() + ": " + GetMessageText();
  } else if (m_item && m_item->GetModel() == m_item) {
    output = "Validate Run Time Environment: errors/warnings detected:";
    return output;
  } else if (m_item) {
    output = " ";
    output += m_item->GetDisplayName();
    output += ": " + GetMessageText();
  }
  return output;
}


void RteDependencyResult::AddComponentAggregate(RteComponentAggregate* a)
{
  m_aggregates.insert(a);
}

RteItem::ConditionResult RteDependencyResult::GetResult(const RteItem* item, const map<const RteItem*, RteDependencyResult>& results)
{
  auto it = results.find(item);
  if (it != results.end())
    return it->second.GetResult();

  return RteItem::UNDEFINED;
}


RteConditionContext::RteConditionContext(RteTarget* target) :
  m_target(target),
  m_result(RteItem::UNDEFINED),
  m_verboseIndent(0)
{
}

RteConditionContext::~RteConditionContext()
{
  RteConditionContext::Clear();
}

void RteConditionContext::Clear()
{
  m_result = RteItem::IGNORED;
  m_cachedResults.clear();
}


bool RteConditionContext::IsVerbose() const
{
  return (RteCondition::GetVerboseFlags() & VERBOSE_FILTER) == VERBOSE_FILTER;
}

void RteConditionContext::VerboseIn(RteItem* item)
{
  if (IsVerbose()) {
    m_verboseIndent++;
    stringstream ss;
    ss << RteUtils::GetIndent(m_verboseIndent) << item->GetID() << endl;
    GetTarget()->GetCallback()->OutputMessage(ss.str());
  }
}

void RteConditionContext::VerboseOut(RteItem* item, RteItem::ConditionResult res)
{
  if (IsVerbose()) {
    stringstream ss;
    ss << RteUtils::GetIndent(m_verboseIndent) << "<--- " <<
      RteItem::ConditionResultToString(res) << " (" << item->GetID() << ")" << endl;
    GetTarget()->GetCallback()->OutputMessage(ss.str());
    m_verboseIndent--;
  }
}

RteItem::ConditionResult RteConditionContext::Evaluate(RteItem* item)
{
  VerboseIn(item);
  RteItem::ConditionResult res = GetConditionResult(item);
  if (res == RteItem::UNDEFINED) {
    res = item->Evaluate(this);
    m_cachedResults[item] = res;
  }
  VerboseOut(item, res);
  return res;
}

RteItem::ConditionResult RteConditionContext::GetConditionResult(RteItem* item) const
{
  if (!item)
    return RteItem::R_ERROR;
  auto it = m_cachedResults.find(item);
  if (it != m_cachedResults.end())
    return it->second;
  return RteItem::UNDEFINED;
}

RteItem::ConditionResult RteConditionContext::EvaluateCondition(RteCondition* condition)
{
  RteItem::ConditionResult resultRequire = RteItem::IGNORED;
  RteItem::ConditionResult resultAccept = RteItem::UNDEFINED;
  // first check require and deny expressions
  for (auto child : condition->GetChildren()) {
    RteConditionExpression* expr = dynamic_cast<RteConditionExpression*>(child);
    if (!expr)
      continue;
    RteItem::ConditionResult res = Evaluate(expr);
    if (res == RteItem::R_ERROR)
      return res;
    if (res == RteItem::IGNORED || res == RteItem::UNDEFINED)
      continue;

    if (expr->GetExpressionType() == RteConditionExpression::ACCEPT) {
      if (res > resultAccept) {
        resultAccept = res;
      }
    } else { // deny or require
      if (res < resultRequire) {
        resultRequire = res;
      }
    }
  }

  if (resultAccept != RteItem::UNDEFINED && resultAccept < resultRequire) {
    return resultAccept;
  }
  return resultRequire;
}

RteItem::ConditionResult RteConditionContext::EvaluateExpression(RteConditionExpression* expr)
{
  if (!expr)
    return RteItem::R_ERROR;
  char domain = expr->GetExpressionDomain();
  switch (domain)
  {
  case BOARD_EXPRESSION:
  case DEVICE_EXPRESSION:
  case TOOLCHAIN_EXPRESSION:
    return expr->EvaluateExpression(GetTarget());

  case CONDITION_EXPRESSION: // evaluate referenced condition
    return Evaluate(expr->GetCondition());

  case COMPONENT_EXPRESSION: // component expressions are ignored by filtering
  case HW_EXPRESSION: // reserved for future, currently ignored
  default:  // any unknown expression is ignored
    return RteItem::IGNORED;
  };
}


RteDependencySolver::RteDependencySolver(RteTarget* target) :
  RteConditionContext(target)
{
}

RteDependencySolver::~RteDependencySolver()
{
  RteDependencySolver::Clear();
}


void RteDependencySolver::Clear()
{
  RteConditionContext::Clear();
  m_componentAggregates.clear();
}

bool RteDependencySolver::IsVerbose() const
{
  return (RteCondition::GetVerboseFlags() & VERBOSE_DEPENDENCY) == VERBOSE_DEPENDENCY;
}


RteItem::ConditionResult RteDependencySolver::EvaluateCondition(RteCondition* condition)
{
  // new behavior - first check if filtering condition evaluates to FULFILLED or IGNORED
  RteConditionContext* filterContext = GetTarget()->GetFilterContext();
  RteItem::ConditionResult res = filterContext->Evaluate(condition);
  switch (res) {
  case RteItem::FAILED: //  ignore dependencies if filter context failed
    return RteItem::IGNORED;
  case RteItem::R_ERROR:
    return RteItem::R_ERROR;
  default:
    break;
  }
  return RteConditionContext::EvaluateCondition(condition);
}

RteItem::ConditionResult RteDependencySolver::EvaluateExpression(RteConditionExpression* expr)
{
  if (!expr)
    return RteItem::R_ERROR;
  char domain = expr->GetExpressionDomain();
  switch (domain)
  {
  case COMPONENT_EXPRESSION:
    return CalculateDependencies(expr);

  case CONDITION_EXPRESSION:
    return Evaluate(expr->GetCondition());  // evaluate referenced condition

  case BOARD_EXPRESSION:    // ignored in dependency context
  case DEVICE_EXPRESSION:   // ignored in dependency context
  case TOOLCHAIN_EXPRESSION:// ignored in dependency context
  case HW_EXPRESSION:       // ignored in dependency context
  default:  // any unknown expression is ignored
    return RteItem::IGNORED;
  };
}

RteItem::ConditionResult RteDependencySolver::CalculateDependencies(RteConditionExpression* expr)
{
  set<RteComponentAggregate*> components;
  RteItem::ConditionResult result;
  if (expr->IsDenyExpression()) {
    result = RteItem::FULFILLED;
    const map<RteComponentAggregate*, int>& selectedComponents = m_target->GetSelectedComponentAggregates();
    for (auto [a, n] : selectedComponents) {
      RteItem* c = a->GetComponent();
      if (!c)
        c = a->GetComponentInstance();
      if (c && c->MatchComponentAttributes(expr->GetAttributes())) {
        components.insert(a);
        result = RteItem::INCOMPATIBLE;
      }
    }
  } else {
    result = m_target->GetComponentAggregates(*expr, components);
    if (components.size() > 1) {
      // leave only the component if it can be resolved automatically (current bundle, DFP)
      RteComponentAggregate* a = expr->GetSingleComponentAggregate(m_target, components);
      if (a) {
        components.clear();
        components.insert(a);
      }
    }
  }
  m_componentAggregates[expr] = components;
  return result;
}

const set<RteComponentAggregate*>& RteDependencySolver::GetComponentAggregates(RteConditionExpression* expr) const
{
  auto it = m_componentAggregates.find(expr);
  if (it != m_componentAggregates.end())
    return it->second;

  static set<RteComponentAggregate*> emptyComponentSet;
  return emptyComponentSet;
}


RteItem::ConditionResult RteDependencySolver::EvaluateDependencies()
{
  Clear();
  const map<RteComponentAggregate*, int>& selectedComponents = m_target->GetSelectedComponentAggregates();
  for (auto [a, n] : selectedComponents) {
    RteItem::ConditionResult res = a->Evaluate(this);
    if (res > RteItem::UNDEFINED && m_result > res)
      m_result = res;
  }
  return GetConditionResult();
}

RteItem::ConditionResult RteDependencySolver::ResolveDependencies()
{
  for (RteItem::ConditionResult res = GetConditionResult(); res < RteItem::FULFILLED; res = GetConditionResult()) {
    if (ResolveIteration() == false)
      break;
  }
  return GetConditionResult();
}

/**
* Tries to resolve SELECTABLE dependencies
* @return true if one of dependencies gets resolved => the state changes
*/
bool RteDependencySolver::ResolveIteration()
{
  if (!m_target || !m_target->GetClasses())
    return false;
  map<const RteItem*, RteDependencyResult> results;
  m_target->GetSelectedDepsResult(results, m_target);

  for (auto [item, depsRes] : results) {
    RteItem::ConditionResult r = depsRes.GetResult();
    if (r != RteItem::SELECTABLE)
      continue;
    if (ResolveDependency(depsRes))
      return true;
  }
  return false;
}


bool RteDependencySolver::ResolveDependency(const RteDependencyResult& depsRes)
{
  // add sub-items if any
  const map<const RteItem*, RteDependencyResult>& results = depsRes.GetResults();
  for (auto [_, dRes] : results) {
    RteItem::ConditionResult r = dRes.GetResult();
    if (r != RteItem::SELECTABLE || dRes.IsMultiple()) {
      continue;
    }
    const RteItem* item = dRes.GetItem();

    const RteConditionExpression* expr = dynamic_cast<const RteConditionExpression*>(item);
    RteComponentAggregate* a = expr->GetSingleComponentAggregate(m_target);
    if (a) {
      RteComponent* c = a->GetComponent();
      if (!c || c->IsCustom()) {
        // Disable "Resolve" function for components with 'custom=1' attribute
        continue;
      }
      if (!c->MatchComponentAttributes(expr->GetAttributes())) {
        c = a->FindComponent(expr->GetAttributes());
        if (c) {
          a->SetSelectedVariant(c->GetCvariantName());
          a->SetSelectedVersion(c->GetVersionString());
        }
      }
      m_target->SelectComponent(a, 1, true); // will trigger EvaluateDependencies()
      return true;
    }
  }
  return false;
}

// End of RteCondition.cpp
