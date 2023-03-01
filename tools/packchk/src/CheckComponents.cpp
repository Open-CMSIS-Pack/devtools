/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "CheckComponents.h"
#include "CheckFiles.h"

#include "RteModel.h"
#include "RteUtils.h"
#include "ErrLog.h"
#include "RteGenerator.h"

using namespace std;

/**
 * @brief visitor callback for component items
 * @param item one element from container
 * @return VISIT_RESULT
*/
VISIT_RESULT ComponentsVisitor::Visit(RteItem* item)
{
  m_checkComponent.CheckComp(item);

  return VISIT_RESULT::CONTINUE_VISIT;
}

/**
 * @brief Check a component. Iterate over all tests.
 * @param item element to test
 * @return passed / failed
*/
bool CheckComponent::CheckComp(RteItem* item)
{
  bool ok = true;
  if(!item) {
    return ok;
  }

  int lineNo = item->GetLineNumber();
  string tag = item->GetTag();
  if(tag != "component") {
    return true;
  }

  string cclass = item->GetAttribute("Cclass");

  // packchk to check that conditions filter device and board specific components
  string csub = item->GetAttribute("Csub");
  if(cclass == "Device" || cclass == "Board Support" || (cclass == "CMSIS Driver" && csub.empty()) ) {                 // must have condition ref to a device
    LogMsg("M087", TYP(cclass), CCLASS(item->GetAttribute("Cclass")), CGROUP(item->GetAttribute("Cgroup")), CVER(item->GetAttribute("Cversion")));

    RteItem* cond = item->GetCondition();
    if(!cond) {
      LogMsg("M335", CCLASS(item->GetAttribute("Cclass")), CGROUP(item->GetAttribute("Cgroup")), CVER(item->GetAttribute("Cversion")), lineNo);
    }
    else if(!ConditionRefToDevice(item)) {
      LogMsg("M336", CCLASS(item->GetAttribute("Cclass")), CGROUP(item->GetAttribute("Cgroup")), CVER(item->GetAttribute("Cversion")), COND(cond->GetName()), lineNo);
    }
  }

  // Check Generator ID if found
  string generatorId = item->GetGeneratorName();
  if(!generatorId.empty()) {
    bool genFound = false;
    LogMsg("M088", VAL("GENID", generatorId), CCLASS(item->GetAttribute("Cclass")), CGROUP(item->GetAttribute("Cgroup")), CVER(item->GetAttribute("Cversion")));

    const RtePackage* pKg = item->GetPackage();
    if(pKg) {
      RteGeneratorContainer* genCont = pKg->GetGenerators();
      if(genCont) {
        for(auto itm : genCont->GetChildren()) {
          RteGenerator* generator = dynamic_cast<RteGenerator*>(itm);
          if(!generator) {
            continue;
          }

          if(generatorId == generator->GetName()) {
            genFound = true;
            break;
          }
        }

        if(!genFound) {
          LogMsg("M347", VAL("GENID", generatorId), CCLASS(item->GetAttribute("Cclass")), CGROUP(item->GetAttribute("Cgroup")), CVER(item->GetAttribute("Cversion")), lineNo);
          ok = false;
        }

        if(ok) {
          LogMsg("M010");
        }
      }
    }
  }

  return ok;
}

/**
 * @brief test conditions for a connected device
 * @param cond RteCondition condition
 * @return passed / failed
*/
bool CheckComponent::TestSubConditions(RteCondition* cond)
{
  if(!cond) {
    return false;
  }

  for(auto exprItem : cond->GetChildren()) {
    if(TestSubConditions(exprItem->GetCondition())) {
      return true;    // found condition
    }

    string dname = exprItem->GetAttribute("Dname");
    if(dname.empty()) {
      continue;
    }

    string dvendor = exprItem->GetAttribute("Dvendor");
    LogMsg("M094", COND(cond->GetName()));
    list<RteDevice*> devices;
    GetModel().GetDevices(devices, dname, dvendor, RteDeviceItem::VARIANT);
    if(!devices.empty()) {
      LogMsg("M010");
      return true;
    }
    else {
      if(dvendor.empty()) {
        dvendor = "<no vendor>";
      }
      LogMsg("M364", COND(cond->GetName()), VENDOR(dvendor), MCU(dname), cond->GetLineNumber());
    }
  }

  return false;
}

/**
 * @brief entrypoint to test conditions from an RTE component item
 * @param item RteItem
 * @return passed / failed
 */
bool CheckComponent::ConditionRefToDevice(RteItem* item)
{
  RteCondition* cond = item->GetCondition();
  if(!cond) {
    return false;
  }

  if(TestSubConditions(cond)) {
    return true;
  }

  return false;
}

