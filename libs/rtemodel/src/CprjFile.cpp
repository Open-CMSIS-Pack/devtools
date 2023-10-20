/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file CprjFile.cpp
* @brief CMSIS RTE Data Model
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "CprjFile.h"

#include "RteDevice.h"
#include "RteFile.h"
#include "RteTarget.h"
#include "RteProject.h"

#include "RteFsUtils.h"
#include "XMLTree.h"

using namespace std;
//////////////////////////////////////////////

CprjTargetElement::CprjTargetElement(RteItem* parent) :
  RteItem(parent),
  m_debugProbeOption(0),
  m_buildOption(0),
  m_stackOption(0),
  m_heapOption(0),
  m_device(0),
  m_board(0),
  m_processor(0),
  m_startupMemory(0)
{

}
CprjTargetElement::~CprjTargetElement()
{
  CprjTargetElement::Clear();
}

void CprjTargetElement::Clear()
{
  m_debugProbeOption = 0;
  m_buildOption = 0;
  m_stackOption = 0;
  m_heapOption = 0;
  m_device = 0;
  m_processor = 0;
  m_startupMemory = 0;
  m_deviceOptions.clear();
  m_assignedMemory.clear();
  m_effectiveProperties.clear();
  RteItem::Clear();
}

RteItem* CprjTargetElement::CreateItem(const std::string& tag)
{
  if (tag == "algorithm" || tag == "memory") {
    return new RteDeviceAlgorithm(this);
  } else if ( tag == "memory") {
      return new RteDeviceMemory(this);
  } else if (tag == "output") {
    m_buildOption = new RteItem(this);
    return m_buildOption;
  } else if (tag == "debugProbe") {
    m_debugProbeOption = new RteItem(this);
    return m_debugProbeOption;
  } else if (tag == "heap") {
    m_heapOption = new RteItem(this);
    return m_heapOption;
  } else if (tag == "stack") {
    m_stackOption = new RteItem(this);
    return m_stackOption;
  }
  return RteItem::CreateItem(tag);
}

void CprjTargetElement::Construct()
{
  RteItem::Construct();
  for (auto child : GetChildren()) {
    const string& tag = child->GetTag();
    if (tag == "algorithm" || tag == "memory") {
      RteDeviceProperty* p = dynamic_cast<RteDeviceProperty*>(child);
      if (p) {
        const string& id = p->GetID();
        if (GetDeviceOption(id) == 0) {
          m_deviceOptions[id] = p;
        }
      }
    }
  }
}

int CprjTargetElement::GetStartupMemoryIndex() const
{
  if (!m_startupMemory)
    return -1;

  for (auto [idx, mem] : m_assignedMemory) {
    if (mem == m_startupMemory)
      return idx;
  }
  return -1;
}

RteDevice* CprjTargetElement::ResolveDeviceAndBoard(RteModel* targetModel)
{
  if (!targetModel) {
    return nullptr;
  }
  m_device = ResolveDevice(targetModel);

  // first resolve board since it can overwrite some device properties (algorithm and debugProbe)
  string boardName = GetBoardDisplayName();
  m_board = targetModel->FindCompatibleBoard(boardName, m_device, true);

  CollectEffectiveProperties(GetProcessorName());
  return m_device;
}

RteDevice* CprjTargetElement::ResolveDevice(RteModel* targetModel)
{
  const string& fullDeviceName = GetAttribute("Dname");
  const string& deviceVendor = GetAttribute("Dvendor");
  string procName = GetAttribute("Pname");

  string deviceName = RteUtils::GetPrefix(fullDeviceName);
  if (procName.empty())
    procName = RteUtils::GetSuffix(fullDeviceName);

  return targetModel->GetDevice(fullDeviceName, deviceVendor);
}

void CprjTargetElement::CollectEffectiveProperties(const string& procName)
{
  m_effectiveProperties.clear();
  m_assignedMemory.clear();
  m_startupMemory = 0;
  if (!m_device) {
    return;
  }

  CollectEffectiveProperties(this, procName);
  // merge properties and attributes from the board if any
  if (m_board) {
    CollectEffectiveProperties(m_board, procName);
    if (!m_debugProbeOption) {
      m_debugProbeOption = m_board->GetDebugProbe(procName, GetAttributeAsInt("deviceIndex"));
    }
  }

  // merge properties and attributes from the device
  const RteDevicePropertyMap& propMap = m_device->GetEffectiveProperties(procName);
  RteDevicePropertyMap::const_iterator itpm; // iterate through properties:
  for (itpm = propMap.begin(); itpm != propMap.end(); ++itpm) {
    const string& propType = itpm->first; // processor, feature, memory, etc.
    const list<RteDeviceProperty*>& props = itpm->second;
    if (m_effectiveProperties.find(propType) == m_effectiveProperties.end()) {
      m_effectiveProperties[propType] = list<RteDeviceProperty*>();
    }
    list<RteDeviceProperty*>& effectiveProps = m_effectiveProperties[propType];
    for (auto p : props) {
      const string& id = p->GetID();
      RteDeviceProperty* pc = GetDeviceOption(id);
      if (propType == "prosessor") {
        if (!pc) {
          pc = new RteDeviceProcessor(this);
          AddItem(pc);
        }
        pc->SetAttributes(GetAttributes());
        m_processor = pc;
      }
      if (!pc) {
        effectiveProps.push_back(p);
        continue;
      }
      if (pc->IsRemove())
        continue;
      const map<string, string>& attr = p->GetAttributes();
      pc->AddAttributes(attr, false); // merge attributes
      if (!m_startupMemory && propType == "memory" && pc->GetAttributeAsBool("startup")) {
        m_startupMemory = pc;
      }
    }
  }
}

void CprjTargetElement::CollectEffectiveProperties(RteItem* item, const string& procName)
{
  if (!item) {
    return;
  }
  // assemble effective properties
  // add memories and algos in specified order
  for (auto child : item->GetChildren()) {
    RteDeviceProperty* pc = dynamic_cast<RteDeviceProperty*>(child);
    if (!pc)
      continue;
    const string& propType = pc->GetTag(); // algorithm, memory, etc.
    if (pc->IsRemove())
      continue;
    const string& id = pc->GetID();
    if (GetDeviceOption(id))
      continue; // do not allow duplicates

    auto itp = m_effectiveProperties.find(propType);
    if (itp == m_effectiveProperties.end()) {
      m_effectiveProperties[propType] = list<RteDeviceProperty*>();
    }
    list<RteDeviceProperty*>& effectiveProps = m_effectiveProperties[propType];
    effectiveProps.push_back(pc);
    if (!m_startupMemory && propType == "memory" && pc->GetAttributeAsBool("startup")) {
      m_startupMemory = pc;
    }
  }
}

RteDeviceProperty* CprjTargetElement::GetDeviceOption(const string& id) const
{
  auto it = m_deviceOptions.find(id);
  if (it != m_deviceOptions.end())
    return it->second;
  return NULL;
}

const string & CprjTargetElement::GetDcore() const
{
  if (m_processor)
    return m_processor->GetAttribute("Dcore");
  return EMPTY_STRING;
}

string CprjTargetElement::GetBoardDisplayName() const
{
  string name = GetAttribute("Bname");
  if (!name.empty()) {
    const string &rev = HasAttribute("Bversion") ? GetAttribute("Bversion") : GetAttribute("Brevision");
    if (!rev.empty()) {
      name += " (" + rev + ")";
    }
  }
  return name;
}

string CprjTargetElement::GetPname() const
{
  string pname = GetProcessorName();
  if (pname.empty()) {
    pname = RteUtils::GetSuffix(GetFullDeviceName());
  }
  return pname;
}

const string &CprjTargetElement::GetTargetBuildFlags(const string &tag, const string &compiler) const {
  RteItem* rteItem = GetChildByTagAndAttribute(tag, "compiler", compiler);
  if (rteItem) {
    return rteItem->GetAttribute("add");
  }
  return EMPTY_STRING;
}

const string &CprjTargetElement::GetCFlags(const string &compiler) const {
  return GetTargetBuildFlags("cflags", compiler);
}

const string &CprjTargetElement::GetCxxFlags(const string &compiler) const {
  return GetTargetBuildFlags("cxxflags", compiler);
}

const string &CprjTargetElement::GetAsFlags(const string &compiler) const {
  return GetTargetBuildFlags("asflags", compiler);
}

const string &CprjTargetElement::GetArFlags(const string &compiler) const {
  return GetTargetBuildFlags("arflags", compiler);
}

const string &CprjTargetElement::GetLdFlags(const string &compiler) const {
  return GetTargetBuildFlags("ldflags", compiler);
}

const string &CprjTargetElement::GetLdCFlags(const string &compiler) const {
  return GetTargetBuildFlags("ldcflags", compiler);
}

const string &CprjTargetElement::GetLdCxxFlags(const string &compiler) const {
  return GetTargetBuildFlags("ldcxxflags", compiler);
}

void CprjTargetElement::SetTargetBuildFlags(const string &tag, const string &compiler, const string &value) {
  // if attribute does not exist, it'll be created and set with value
  // if attribute exists and value is not empty attribute is set with value
  // if attribute exists and value is empty, the existing attribute is removed
  RteItem* rteItem = GetChildByTagAndAttribute(tag, "compiler", compiler);
  if (rteItem) {
    if (value.empty()) {
      rteItem->RemoveAttribute("add");
    } else {
      rteItem->SetAttribute("add", value.c_str());
    }
  } else if (!value.empty()) { // tag not existing, create it
    rteItem = new RteItem(this);
    rteItem->SetTag(tag);
    rteItem->SetAttribute("add", value.c_str());
    rteItem->SetAttribute("compiler", compiler.c_str());
    AddItem(rteItem);
  }
}

void CprjTargetElement::SetCFlags(const string &flags, const string &compiler) {
  SetTargetBuildFlags("cflags", compiler, flags);
}

void CprjTargetElement::SetCxxFlags(const string &flags, const string &compiler) {
  SetTargetBuildFlags("cxxflags", compiler, flags);
}

void CprjTargetElement::SetAsFlags(const string &flags, const string &compiler) {
  SetTargetBuildFlags("asflags", compiler, flags);
}

void CprjTargetElement::SetArFlags(const string &flags, const string &compiler) {
  SetTargetBuildFlags("arflags", compiler, flags);
}

void CprjTargetElement::SetLdFlags(const string &flags, const string &compiler) {
  SetTargetBuildFlags("ldflags", compiler, flags);
}

void CprjTargetElement::SetLdCFlags(const string &flags, const string &compiler) {
  SetTargetBuildFlags("ldcflags", compiler, flags);
}

void CprjTargetElement::SetLdCxxFlags(const string &flags, const string &compiler) {
  SetTargetBuildFlags("ldcxxflags", compiler, flags);
}

///////////////////////////////////////////////
CprjFile::CprjFile(RteItem* parent) :
  RteRootItem(parent),
  m_cprjTargetElement(0),
  m_files(0)
{
}

CprjFile::~CprjFile()
{
  CprjFile::Clear();
}

void CprjFile::Clear()
{
  m_files = 0;
  m_cprjTargetElement = 0;
  RteRootItem::Clear();
}

RteItem* CprjFile::CreateItem(const std::string& tag)
{
  if (tag == "files") {
    m_files = new RteFileContainer(this);
    return m_files;
  } else if (tag == "target") {
    m_cprjTargetElement = new CprjTargetElement(this);
    return m_cprjTargetElement;
  }
  return RteRootItem::CreateItem(tag);
}

RteItem* CprjFile::GetProjectInfo() const
{
  return GetItemByTag("info");
}

const std::string& CprjFile::GetRteFolder() const
{
  if (m_cprjTargetElement) {
    const auto buildOption = m_cprjTargetElement->GetBuildOption();
    if (buildOption) {
      const string& folder = buildOption->GetAttribute("rtedir");
      if (!folder.empty()) {
        return folder;
      }
    }
  }
  return RteUtils::EMPTY_STRING;
}

const Collection<RteItem*>& CprjFile::GetProjectComponents() const
{
  return GetGrandChildren("components");
}

const Collection<RteItem*>& CprjFile::GetProjectLayers() const
{
  return GetGrandChildren("layers");
}

const Collection<RteItem*>& CprjFile::GetPackRequirements() const
{
  return GetGrandChildren("packages");
}

const Collection<RteItem*>& CprjFile::GetCompilerRequirements() const
{
  return GetGrandChildren("compilers");
}

string CprjFile::ConstructID()
{
  return RteUtils::EMPTY_STRING;  // no IDs for Cprj
}
// End of CprjFile.cpp
