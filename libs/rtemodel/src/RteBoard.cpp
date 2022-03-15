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
  const string& rev = GetAttribute("revision");
  if (!rev.empty()) {
    name += " (" + rev + ")";
  }
  return name;
}

void RteBoard::GetDevices(list<RteItem*>& devices) const {
  for (auto it = m_children.begin(); it != m_children.end(); it++) {
    const string& tag = (*it)->GetTag();
    if (tag == "mountedDevice" || tag == "compatibleDevice") {
      devices.push_back(*it);
    }
  }
}

void RteBoard::GetMountedDevices(list<RteItem*>& mountedDevices) const {
  for (auto it = m_children.begin(); it != m_children.end(); it++) {
    if ((*it)->GetTag() == "mountedDevice") {
      mountedDevices.push_back(*it);
    }
  }
}

void RteBoard::GetCompatibleDevices(list<RteItem*>& compatibleDevices) const {
  for (auto it = m_children.begin(); it != m_children.end(); it++) {
    if ((*it)->GetTag() == "compatibleDevice") {
      compatibleDevices.push_back(*it);
    }
  }
}

// get vendor name for a mounted/compatible device (e.g. if no device aggregate available)
string RteBoard::GetDeviceVendorName(const string& devName) const
{
  if (devName.empty()) {
    return (EMPTY_STRING);
  }

  list <RteItem*> devices;
  GetDevices(devices);

  for (auto it = devices.begin(); it != devices.end(); it++) {
    RteItem* item = (*it);
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

// returns true if mounted or compatible devices match supplied device attributes
bool RteBoard::HasCompatibleDevice(const map<string, string>& deviceAttributes) const
{
  RteAttributes da(deviceAttributes);
  const string& dname = da.GetAttribute("Dname");
  const string& dvariant = da.GetAttribute("Dvariant");

  for (auto it = m_children.begin(); it != m_children.end(); it++) {
    RteItem* rteItem = (*it);

    const string& tag = rteItem->GetTag();
    if (tag == "mountedDevice" || tag == "compatibleDevice") {
      if (!dname.empty() && (dname == rteItem->GetAttribute("Dname") || dname == rteItem->GetAttribute("Dvariant"))) {
        return true;
      }
      if (!dvariant.empty() && (dvariant == rteItem->GetAttribute("Dname") || dvariant == rteItem->GetAttribute("Dvariant"))) {
        return true;
      }
      if (rteItem->MatchDeviceAttributes(deviceAttributes))
        return true;
    }
  }
  return false;
}

// collects board books as name-title collection
void RteBoard::GetBooks(map<string, string>& books) const
{
  for (auto it = m_children.begin(); it != m_children.end(); it++) {
    RteItem* item = *it;
    if (item->GetTag() == "book") {
      string title = item->GetAttribute("title");
      if (title.empty())
        continue;
      title += " (" + GetName() + ")";

      string doc = item->GetDocFile();
      if (doc.empty())
        continue;
      books[doc] = title;
    }
  }
}

RteItem* RteBoard::GetDebugProbe(const string& pname, int deviceIndex)
{
  for (auto it = m_children.begin(); it != m_children.end(); it++) {
    RteItem* item = *it;
    if (item->GetTag() == "debugProbe" &&
      item->GetProcessorName() == pname &&
      item->GetAttributeAsInt("deviceIndex") == deviceIndex) {
      return item;
    }
  }
  return nullptr;
}


void RteBoard::Clear()
{
  m_mountedDevs.clear();
  m_rom.clear();
  m_ram.clear();
  RteItem::Clear();
}

bool RteBoard::ProcessXmlElement(XMLTreeElement* xmlElement)
{
  const string& tag = xmlElement->GetTag();
  if (tag == "mountedDevice") {
    const string& dname = xmlElement->GetAttribute("Dname");
    const string& dvendor = xmlElement->GetAttribute("Dvendor");
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
    const string& ftype = xmlElement->GetAttribute("type");
    if (ftype == "ROM") {
      AddMemStr(m_rom, xmlElement);
    } else if (ftype == "RAM") {
      AddMemStr(m_ram, xmlElement);
    }
  } else if (tag == "algorithm") {
    RteDeviceAlgorithm* algo = new RteDeviceAlgorithm(this);
    if (algo && algo->Construct(xmlElement)) {
      AddItem(algo);
      return true;
    } else {
      return false;
    }
  } else if (tag == "debugProbe") {
    RteItem* dp = new RteItem(this);
    if (dp && dp->Construct(xmlElement)) {
      AddItem(dp);
      return true;
    } else {
      return false;
    }
  }
  return RteItem::ProcessXmlElement(xmlElement);
}

string RteBoard::ConstructID()
{
  return GetDisplayName();
}

void RteBoard::AddMemStr(string& memStr, XMLTreeElement* xmlElement) {
  const string& name = xmlElement->GetAttribute("name");
  const string& num = xmlElement->GetAttribute("n");

  if (name.empty()) {
    return;
  }
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


bool RteBoardContainer::ProcessXmlElement(XMLTreeElement* xmlElement)
{
  const string& tag = xmlElement->GetTag();
  if (tag == "board") {
    RteBoard* board = new RteBoard(this);
    if (board->Construct(xmlElement)) {
      AddItem(board);
      return true;
    }
    delete board;
    return false;
  }
  return RteItem::ProcessXmlElement(xmlElement);
}


// End of RteBoard.cpp
