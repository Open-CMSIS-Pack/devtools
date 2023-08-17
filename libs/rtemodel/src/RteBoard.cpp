/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteBoard.cpp
* @brief CMSIS RTE Data Model
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/
#include "RteBoard.h"
#include "RteDevice.h"

#include "XMLTree.h"

using namespace std;

RteBoard::RteBoard(RteItem* parent) :
  RteItem(parent)
{
  RteBoard::Clear();
}

RteBoard::~RteBoard()
{
  RteBoard::Clear();
}

string RteBoard::GetDisplayName() const {
  string name = GetName();
  const string& rev = GetRevision();
  if (!rev.empty()) {
    name += " (" + rev + ")";
  }
  return name;
}

const string& RteBoard::GetName() const
{
  if (HasAttribute("Bname")) {
    return GetAttribute("Bname");
  }
  return GetAttribute("name");
}

const string& RteBoard::GetVendorString() const {
  if (HasAttribute("Bvendor")) {
    return GetAttribute("Bvendor");
  }
  return GetAttribute("vendor");
}

const string& RteBoard::GetRevision() const {
  if (HasAttribute("Brevision")) {
    return GetAttribute("Brevision");
  }
  if (HasAttribute("Bversion")) {
    return GetAttribute("Bversion");
  }
  return GetAttribute("revision");
}

void RteBoard::GetDevices(Collection<RteItem*>& devices, bool bCompatible, bool bMounted) const {
  for (auto rteItem : m_children) {
    const string& tag = rteItem->GetTag();
    if ((bMounted && tag == "mountedDevice") || (bCompatible && tag == "compatibleDevice")) {
      devices.push_back(rteItem);
    }
  }
}

void RteBoard::GetMountedDevices(Collection<RteItem*>& mountedDevices) const {
  GetDevices(mountedDevices, false, true);
}

void RteBoard::GetCompatibleDevices(Collection<RteItem*>& compatibleDevices) const {
  GetDevices(compatibleDevices, true, false);
}

// get vendor name for a mounted/compatible device (e.g. if no device aggregate available)
string RteBoard::GetDeviceVendorName(const string& devName) const
{
  if (devName.empty()) {
    return (EMPTY_STRING);
  }

  Collection<RteItem*> devices;
  GetDevices(devices);
  for (auto item: devices) {
    if (item == 0) {
      continue;
    }
    if (item->GetAttribute("Dname") == devName
      || item->GetAttribute("Dfamily") == devName
      || item->GetAttribute("DsubFamily") == devName)
    {
      const string& vendor = item->GetAttribute("Dvendor");
      return DeviceVendor::GetCanonicalVendorName(vendor);
    }
  }

  return (EMPTY_STRING);
}

bool RteBoard::HasMountedDevice(const XmlItem& deviceAttributes) const
{
  return HasCompatibleDevice(deviceAttributes, true);
}

bool RteBoard::HasMCU() const
{
  for (auto rteItem : m_children) {
    if (rteItem->GetTag() == "mountedDevice") {
      const string& dname = rteItem->GetAttribute("Dname");
      if (!dname.empty() && dname != "NO_MCU") {
        return true;
      }
    }
  }
  return false;
}

bool RteBoard::HasCompatibleDevice(const XmlItem& deviceAttributes, bool bOnlyMounted) const
{
  for (auto rteItem : m_children) {
    const string& tag = rteItem->GetTag();
    if (tag == "mountedDevice" || (!bOnlyMounted && tag == "compatibleDevice")) {
      if (IsDeviceCompatible(deviceAttributes, *rteItem)) {
        return true;
      }
    }
  }
  return false;
}

bool RteBoard::IsDeviceCompatible(const XmlItem& deviceAttributes, const RteItem& boardDeviceAttributes)
{
  const string& dname = deviceAttributes.GetAttribute("Dname");
  const string& dvariant = deviceAttributes.GetAttribute("Dvariant");
  if (!dname.empty() && (dname == boardDeviceAttributes.GetAttribute("Dname") || dname == boardDeviceAttributes.GetAttribute("Dvariant"))) {
    return true;
  }
  if (!dvariant.empty() && (dvariant == boardDeviceAttributes.GetAttribute("Dname") || dvariant == boardDeviceAttributes.GetAttribute("Dvariant"))) {
    return true;
  }

  return boardDeviceAttributes.MatchDeviceAttributes(deviceAttributes.GetAttributes());
}

// collects board books as name-title collection
void RteBoard::GetBooks(map<string, string>& books) const
{
  for (auto rteItem : m_children) {
    if (rteItem->GetTag() == "book") {
      string title = rteItem->GetAttribute("title");
      if (title.empty())
        continue;
      title += " (" + GetName() + ")";

      string doc = rteItem->GetDocFile();
      if (doc.empty())
        continue;
      books[doc] = title;
    }
  }
}

RteItem* RteBoard::GetDebugProbe(const string& pname, int deviceIndex)
{
  for (auto rteItem : m_children) {
    if (rteItem->GetTag() == "debugProbe" && rteItem->GetAttributeAsInt("deviceIndex", -1) == deviceIndex) {
      const string& procName = rteItem->GetProcessorName();
      if (pname.empty() || procName.empty() || procName == pname) {
        return rteItem;
      }
    }
  }
  return nullptr;
}

Collection<RteItem*>& RteBoard::GetAlgorithms(Collection<RteItem*>& algos) const
{
  return GetChildrenByTag("algorithm", algos);
}

Collection<RteItem*>& RteBoard::GetMemories(Collection<RteItem*>& mems) const
{
  return GetChildrenByTag("memory", mems);
}


void RteBoard::Clear()
{
  m_mountedDevs.clear();
  m_rom.clear();
  m_ram.clear();
  RteItem::Clear();
}

RteItem* RteBoard::CreateItem(const std::string& tag)
{
if (tag == "algorithm") {
    return new RteDeviceAlgorithm(this);
  } else if (tag == "memory") {
    return  new RteDeviceMemory(this);
  }
  return RteItem::CreateItem(tag);
}

void RteBoard::Construct()
{
  RteItem::Construct();
  for (auto child : GetChildren()) {
    const string& tag = child->GetTag();
    if (tag == "mountedDevice") {
      const string& dname = child->GetAttribute("Dname");
      const string& dvendor = child->GetAttribute("Dvendor");
      if (!dname.empty()) {
        if (!m_mountedDevs.empty()) {
          m_mountedDevs += ", ";
        }
        if (!m_mountedDevsVendor.empty()) {
          m_mountedDevsVendor += ", ";
        }
        m_mountedDevs += dname;
        m_mountedDevsVendor += dname;
        if (!dvendor.empty()) {
          m_mountedDevsVendor += " (" + DeviceVendor::GetCanonicalVendorName(dvendor) + ")";
        }
      }
    } else if (tag == "feature") {
      const string& ftype = child->GetAttribute("type");
      if (ftype == "ROM") {
        AddMemStr(m_rom, child);
      } else if (ftype == "RAM") {
        AddMemStr(m_ram, child);
      }
    }
  }
}


string RteBoard::ConstructID()
{
  return GetDisplayName();
}

void RteBoard::AddMemStr(string& memStr, const XmlItem* xmlItem) {
  const string& name = xmlItem->GetAttribute("name");
  if (name.empty()) {
    return;
  }
  const string& num = xmlItem->GetAttribute("n");
  if (!memStr.empty()) {
    memStr += ", ";
  }
  if (!num.empty()) {
    memStr += num;
    memStr += " x ";
  }
  memStr += name;
}


///////////////////////////////////////////////////////
RteBoardContainer::RteBoardContainer(RteItem* parent) :
  RteItem(parent)
{

}

RteBoard* RteBoardContainer::GetBoard(const string& id) const
{
  return dynamic_cast<RteBoard*>(GetItem(id));
}

RteItem* RteBoardContainer::CreateItem(const string& tag)
{
  if (tag == "board") {
    return new RteBoard(this);
  }
  return RteItem::CreateItem(tag);
}

// End of RteBoard.cpp
