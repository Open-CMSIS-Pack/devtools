/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteDevice.cpp
* @brief CMSIS RTE Data model
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/
#include "RteDevice.h"

#include "RtePackage.h"
#include "RteFile.h"
#include "RteModel.h"

#include "XMLTree.h"

using namespace std;

static const list<RteDeviceProperty*> EMPTY_PROPERTY_LIST;

const string& RteDeviceElement::GetEffectiveAttribute(const string& name) const
{
  map<string, string>::const_iterator it = m_attributes.find(name);
  if (it != m_attributes.end())
    return it->second;
  // take from parent
  RteDeviceElement* parent = GetDeviceElementParent();
  if (parent)
    return parent->GetEffectiveAttribute(name);

  return EMPTY_STRING;
}

bool RteDeviceElement::HasEffectiveAttribute(const string& name) const
{
  map<string, string>::const_iterator it = m_attributes.find(name);
  if (it != m_attributes.end())
    return true;
  RteItem* parent = GetParent();
  if (parent)
    return parent->HasAttribute(name);
  return false;
}

RteDeviceItem* RteDeviceElement::GetDeviceItemParent() const
{
  for (RteItem* parent = GetParent(); parent != 0; parent = parent->GetParent())
  {
    RteDeviceItem* dParent = dynamic_cast<RteDeviceItem*>(parent);
    if (dParent)
      return dParent;
  }
  return nullptr;
}

RteDeviceElement* RteDeviceElement::GetDeviceElementParent() const
{
  return dynamic_cast<RteDeviceElement*>(GetParent());

}

void RteDeviceElement::GetEffectiveAttributes(XmlItem& attributes) const
{
  // fill in own attributes
  attributes.AddAttributes(m_attributes, false);
  // get parent attributes
  RteDeviceElement* parent = GetDeviceElementParent();
  if (parent)
    parent->GetEffectiveAttributes(attributes);
}



RteDeviceProperty* RteDeviceElement::CreateProperty(const string& tag)
{
  if (tag == "feature") {
    return new RteDeviceFeature(this);
  } else if (tag == "processor") {
    return new RteDeviceProcessor(this);
  } else if (tag == "memory") {
    return new RteDeviceMemory(this);
  } else if (tag == "debug") {
    return new RteDeviceDebug(this);
  } else if (tag == "debugport") {
    return new RteDebugPort(this);
  } else if (tag == "debugconfig") {
    return new RteDebugConfig(this);
  } else if (tag == "trace") {
    return new RteDeviceTrace(this);
  } else if (tag == "debugvars") {
    return new RteDeviceDebugVars(this);
  } else if (tag == "algorithm") {
    return new RteDeviceAlgorithm(this);
  } else if (tag == "book") {
    return new RteDeviceBook(this);
  } else if (tag == "description") {
    return new RteDeviceDescription(this);
  } else if (tag == "environment") {
    return new RteDeviceEnvironment(this);
  } else if (tag == "flashinfo") {
    return new RteFlashInfo(this);
  } else if (tag == "accessportV1") {
    return new RteAccessPortV1(this);
  } else if (tag == "accessportV2") {
    return new RteAccessPortV2(this);
  } else if (tag == "sequence") {
    return new RteSequence(this);
  }
  return new RteDeviceProperty(this);
}


string RteDeviceProperty::ConstructID()
{
  string id = GetTag();

  const string &name = GetName();
  if (!name.empty() && name != GetTag()) {
    id += string(":") + name;
  }
  return id;
}

void RteDeviceProperty::GetEffectiveAttributes(XmlItem& attributes) const
{
  // fill in own attributes
  attributes.AddAttributes(m_attributes, false);

  // get parent attributes
  RteDeviceProperty* parent = dynamic_cast<RteDeviceProperty*>(GetParent());
  if (parent)
    parent->GetEffectiveAttributes(attributes);
}


void RteDeviceProperty::CollectEffectiveContent(RteDeviceProperty* p)
{
  AddAttributes(p->GetAttributes(), false);
}


const list<RteDeviceProperty*>& RteDeviceProperty::GetEffectiveContent() const
{
  return EMPTY_PROPERTY_LIST;
}

RteDeviceProperty* RteDeviceProperty::GetEffectiveContentProperty(const string& tag) const
{
  for (auto p : GetEffectiveContent()) {
    if (p->GetTag() == tag)
      return p;
  }
  return nullptr;
}


RteDeviceProperty* RteDeviceProperty::GetPropertyFromList(const string& id, const list<RteDeviceProperty*>& properties)
{
  if (id.empty())
    return nullptr;
  for (auto pr : properties) {
    if (pr) {
      const string& pid = pr->GetID();
      if (pid == id)
        return pr;
    }
  }
  return nullptr;
}

RteDeviceProperty* RteDeviceProperty::GetPropertyFromMap(const string& tag, const string& id, const RteDevicePropertyMap& properties)
{
  if (id.empty())
    return nullptr;
  auto it = properties.find(tag);
  if (it != properties.end()) {
    return GetPropertyFromList(id, it->second);
  }
  return nullptr;
}


RteDevicePropertyGroup::RteDevicePropertyGroup(RteItem* parent, bool bOwnChldren) :
  RteDeviceProperty(parent),
  m_bOwnChildren(bOwnChldren)
{
};

RteDevicePropertyGroup::~RteDevicePropertyGroup()
{
  RteDevicePropertyGroup::Clear();
}

void RteDevicePropertyGroup::Clear()
{
  if (!m_bOwnChildren) {
    m_children.clear(); // do not destroy child objects
  }
  RteDeviceProperty::Clear();
}


RteDeviceProperty* RteDevicePropertyGroup::GetProperty(const string& id) const
{
  return dynamic_cast<RteDeviceProperty*>(GetItem(id));
}


void RteDevicePropertyGroup::CollectEffectiveContent(RteDeviceProperty* dp)
{
  RteDeviceProperty::CollectEffectiveContent(dp);
  if (!dp->IsCollectEffectiveContent())
    return;
  RteDevicePropertyGroup* pg = dynamic_cast<RteDevicePropertyGroup*>(dp);
  if (pg) {
    for (auto child : pg->GetChildren()) {
      RteDeviceProperty* p = dynamic_cast<RteDeviceProperty*>(child);
      if (p) {
        const string& id = p->GetID();
        RteDeviceProperty* pInserted = RteDeviceProperty::GetPropertyFromList(id, m_effectiveContent);
        if (p == pInserted)
          continue;
        if (pInserted == nullptr || !p->IsUnique()) {
          m_effectiveContent.push_back(p); // insert only unique properties or descriptions
        } else {
          // add missing attributes, but do not replace existing ones (we go down-up)
          // process sub-properties as well
          pInserted->CollectEffectiveContent(p);
        }
      }
    }
  }
}


RteItem* RteDevicePropertyGroup::AddChild(RteItem* child)
{
  RteDeviceProperty::AddChild(child, child->HasAttribute("Pname"));
  RteDeviceProperty* dp = dynamic_cast<RteDeviceProperty*>(child);
  if (dp) {
    m_effectiveContent.push_back(dp);
  }
  return child;
}


RteItem* RteDevicePropertyGroup::CreateItem(const std::string& tag)
{
  return CreateProperty(tag);
}

RteDeviceProperty* RteSequenceControlBlock::CreateProperty(const string& tag)
{
  if (tag == "block") {
    return new RteSequenceCommandBlock(this);
  } else if (tag == "control") {
    return new RteSequenceControlBlock(this);
  }
  return new RteDeviceProperty(this);
}

RteDeviceProperty* RteSequence::CreateProperty(const string& tag)
{
  if (tag == "block") {
    return new RteSequenceCommandBlock(this);
  } else if (tag == "control") {
    return new RteSequenceControlBlock(this);
  }
  return new RteDeviceProperty(this);
}

string RteDatapatch::ConstructID()
{
  const string& ap = GetEffectiveAttribute("__ap");
  const string& dp = GetEffectiveAttribute("__dp");
  const string& apid = GetEffectiveAttribute("__apid"); // <accessportV1>/<accessportV2>
  const string& type = GetAttribute("type");

  string id = RteDeviceProperty::ConstructID();
  if (!apid.empty()) {
    m_hasAPID = true;
    id += string(":");
    id += GetEffectiveAttribute("__apid");
  }
  if (!ap.empty()) {
    m_hasAP = true;
    id += string(":");
    id += GetEffectiveAttribute("__ap");
  }
  if (!dp.empty()) {
    m_hasDP = true;
    id += string(":");
    id += GetEffectiveAttribute("__dp");
  }
  if (!type.empty()) {
    id += string(":");
    id += GetAttribute("type");
  }
  return id;
}

RteDeviceProperty* RteDebugPort::CreateProperty(const string& tag)
{
  if (tag == "jtag") {
    return new RteDebugPortJtag(this);
  } else if (tag == "swd") {
    return new RteDebugPortSwd(this);
  }
  return new RteDeviceProperty(this);
}

string RteAccessPort::ConstructID()
{
  string id = "accessport"; // To make <accessportV1> and <accessportV2> IDs unique

  const string &name = GetName();
  if (!name.empty()) {
    id += string(":") + name;
  }
  return id;
}

RteDeviceProperty* RteDeviceDebug::CreateProperty(const string& tag)
{
  if (tag == "datapatch") {
    return new RteDatapatch(this);
  }
  return new RteDeviceProperty(this);
}

string RteTraceBuffer::ConstructID()
{
  const string& start = GetAttribute("start");
  const string& size = GetAttribute("size");

  string id = RteDeviceProperty::ConstructID();
  if (!start.empty()) {
    id += string(":");
    id += start;
  }
  if (!size.empty()) {
    id += string(":");
    id += size;
  }
  return id;
}

RteDeviceProperty* RteDeviceTrace::CreateProperty(const string& tag)
{
  if (tag == "serialwire") {
    return new RteTraceSerialware(this);
  } else if (tag == "traceport") {
    return new RteTracePort(this);
  } else if (tag == "tracebuffer") {
    return new RteTraceBuffer(this);
  }
  return new RteDeviceProperty(this);
}

/////////////////
// Flash info


RteFlashInfoBlock::RteFlashInfoBlock(RteItem* parent) :
  RteDeviceProperty(parent),
  m_start(0),
  m_size(0),
  m_totalSize(0),
  m_count(1),
  m_arg(0)
{
}

RteFlashInfo* RteFlashInfoBlock::GetRteFlashInfo() const {
  return dynamic_cast<RteFlashInfo*>(GetParent());
}

void RteFlashInfoBlock::CalculateCachedValuesForBlock(RteFlashInfoBlock* previous) {
  if (previous) {
    m_start = previous->GetStart() + previous->GetTotalSize();
  } else {
    RteFlashInfo* fi = GetRteFlashInfo();
    if (fi) {
      m_start = fi->GetStart();
    }
  }
  m_arg = GetAttributeAsULL("arg", 0L);
  m_count = GetAttributeAsULL("count", 1L);
  m_size = GetAttributeAsULL("size", 0L);
  m_totalSize = m_size * m_count;
}

RteFlashInfo::RteFlashInfo(RteItem* parent) : RteDevicePropertyGroup(parent)
{

}

void RteFlashInfo::CalculateCachedValues()
{
  RteFlashInfoBlock* previous = nullptr;
  for (auto b : m_blocks) {
    b->CalculateCachedValuesForBlock(previous);
    previous = b;
  }
}

RteDeviceProperty* RteFlashInfo::CreateProperty(const string& tag) {
  if (tag == "block" || tag == "gap") {
    RteFlashInfoBlock* b = new RteFlashInfoBlock(this);
    m_blocks.push_back(b);
    return b;
  }
  return RteDevicePropertyGroup::CreateProperty(tag);
}


/////////////////
// device tree
RteDeviceItem::RteDeviceItem(RteItem* parent) :
  RteDeviceElement(parent)
{
}

RteDeviceItem::~RteDeviceItem()
{
  RteDeviceItem::Clear();
}

void RteDeviceItem::Clear()
{
  for (auto& [tag, pg ]: m_properties) {
    delete pg;
  }
  m_properties.clear();
  m_deviceItems.clear(); // items are in m_children collection as well, do not delete here
  m_effectiveProperties.clear();
  m_processors.clear();
  RteDeviceElement::Clear();
}


void RteDeviceItem::GetDevices(list<RteDevice*>& devices, const string& searchPattern) const
{
  for (auto item : m_deviceItems) {
    item->GetDevices(devices, searchPattern);
  }
}

int RteDeviceItem::GetProcessorCount() const
{
  return (int)m_processors.size();
}


void RteDeviceItem::GetEffectiveProcessors(list<RteDeviceProperty*>& processors) const
{
  static const string PROCESSOR_TAG = "processor";
  RteDevicePropertyGroup* props = GetProperties(PROCESSOR_TAG);
  if (props) {
    for (auto child : props->GetChildren()) {
      RteDeviceProperty* p = dynamic_cast<RteDeviceProperty*>(child);
      if (!p)
        continue;
      const string& id = p->GetID();
      RteDeviceProperty* pInserted = RteDeviceProperty::GetPropertyFromList(id, processors);
      if (pInserted == nullptr) {
        processors.push_back(p); // insert only unique properties or descriptions
      } else {
        // add missing attributes, but do not replace existing ones (we go down-up)
        // process sub-properties as well
        pInserted->CollectEffectiveContent(p);
      }
    }
  }
  // get the rest from parent
  RteDeviceItem* parent = GetDeviceItemParent();
  if (parent)
    parent->GetEffectiveProcessors(processors);
}

void RteDeviceItem::GetEffectiveDeviceItems(list<RteDeviceItem*>& devices) const
{
  if (GetDeviceItemCount() == 0) {
    if (GetType() > SUBFAMILY) {
      devices.push_back(const_cast<RteDeviceItem*>(this));
    }
    return;
  }
  for (auto item : m_deviceItems) {
    if (item->GetType() > SUBFAMILY && item->GetDeviceItemCount() == 0)
      devices.push_back(item);
    else
      item->GetEffectiveDeviceItems(devices);
  }
}

bool RteDeviceItem::Validate()
{
  m_bValid = RteDeviceElement::Validate();

  // check if we have processor description for an end-leaf device
  if (GetType() == VARIANT || (GetType() == DEVICE && m_deviceItems.empty())) {
    if (m_processors.empty()) {
      m_bValid = false;
      string msg = CreateErrorString("error", "530", "device has no processor definition");
      m_errors.push_front(msg);
    }
  }

  // are there properties for unknown processor?
  for (auto [tag, props] : m_properties) {
    if (tag == "processor")
      continue;
    for (auto child : props->GetChildren()) {
      RteDeviceProperty* p = dynamic_cast<RteDeviceProperty*>(child);
      if (p) {
        const string& procName = p->GetProcessorName();
        if (!procName.empty() && m_processors.find(procName) == m_processors.end()) {
          m_bValid = false;
          string msgText = p->GetName() + " property uses undefined processor '" + procName + "'";
          string msg = CreateErrorString("error", "531", msgText.c_str());
          m_errors.push_front(msg);
        }
      }
    }
  }
  return m_bValid;
}

RteItem* RteDeviceItem::CreateItem(const std::string& tag)
{
  if (tag == "sequences") {
    // ignore sequences element because effective properties work on directly on "sequence" properties
    return this;
  }

  if (tag == "subFamily" || tag == "device" || tag == "variant") {
    RteDeviceItem* di = CreateDeviceItem(tag);
    if (di) {
      m_deviceItems.push_back(di);
      return di;
    }
  }

  RteDeviceProperty* deviceProperty = CreateProperty(tag);
  RteDevicePropertyGroup* props = GetProperties(tag);
  if (!props) {
    props = new RteDevicePropertyGroup(this, false);
    props->SetTag(tag);
    m_properties[tag] = props;
  }
  props->AddItem(deviceProperty);
  return deviceProperty;
}

RteDeviceItem* RteDeviceItem::CreateDeviceItem(const string& tag)
{
  // since our tags differ already by first item, use simple switch
  switch (tag[0]) {
  case 's':
    return new RteDeviceSubFamily(this);
  case 'd':
    return new RteDevice(this);
  case 'v':
    return new RteDeviceVariant(this);

  default:
    break;
  }
  return nullptr;
}

void RteDeviceItem::Construct()
{
  RteDeviceElement::Construct();
  // get processor names
  list<RteDeviceProperty*> processors;
  GetEffectiveProcessors(processors);
  for (auto p : processors) {
    m_processors[p->GetProcessorName()] = p;
  }
}


RteDeviceItem* RteDeviceItem::GetDeviceItemParentOfType(RteDeviceItem::TYPE type) const
{
  if (GetType() == type) {
    return const_cast<RteDeviceItem*>(this);
  }

  RteDeviceItem* dParent = GetDeviceItemParent();
  if (dParent) {
    return dParent->GetDeviceItemParentOfType(type);
  }
  return nullptr;
}


RteDeviceProperty* RteDeviceItem::GetProcessor(const string& pName) const
{
  auto it = m_processors.find(pName);
  if (it != m_processors.end()) {
    return it->second;
  }
  return nullptr;
}

string RteDeviceItem::ConstructID()
{
  string id;
  if (!GetDeviceFamilyName().empty()) {
    id += GetDeviceFamilyName();
  }

  if (!GetDeviceSubFamilyName().empty()) {
    if (!id.empty())
      id += ".";
    id += GetDeviceSubFamilyName();
  }

  if (!GetDeviceName().empty()) {
    if (!id.empty())
      id += ".";
    id += GetDeviceName();
  }

  if (!GetDeviceVariantName().empty()) {
    if (!id.empty())
      id += ".";
    id += GetDeviceVariantName();
  }
  return id;
};


RteDevicePropertyGroup* RteDeviceItem::GetProperties(const string& tag) const
{
  map<string, RteDevicePropertyGroup*>::const_iterator it = m_properties.find(tag);
  if (it != m_properties.end()) {
    return it->second;
  }
  return nullptr;
}

// more complicated, but for complete - gets all available properties
void RteDeviceItem::GetProperties(RteDevicePropertyMap& properties) const
{
  for (auto [tag, props] : m_properties) {
    auto dstIt = properties.find(tag);
    // first add top containers if not yet exist
    if (dstIt == properties.end()) {
      list<RteDeviceProperty*> v;
      properties[tag] = v;
      dstIt = properties.find(tag);
    }
    for (auto child : props->GetChildren()) {
      RteDeviceProperty* p = dynamic_cast<RteDeviceProperty*>(child);
      if (p)
      {
        dstIt->second.push_back(p);
      }
    }
  }
}


RteDeviceProperty* RteDeviceItem::GetProperty(const string& tag, const string& id) const
{
  RteItem* props;
  map<string, RteDevicePropertyGroup*>::const_iterator it = m_properties.find(tag);
  if (it != m_properties.end()) {
    props = it->second;
    if (props)
      return dynamic_cast<RteDeviceProperty*>(props->GetItem(id));
  }
  return nullptr;
}


void RteDeviceItem::CollectEffectiveProperties(const string& tag, list<RteDeviceProperty*>& properties, const string& pName, bool bRecursive) const
{
  RteDevicePropertyGroup* props = GetProperties(tag);
  if (props) {
    for (auto child : props->GetChildren()) {
      RteDeviceProperty* p = dynamic_cast<RteDeviceProperty*>(child);
      if (p) {
        const string& propPname = p->GetProcessorName();
        if (pName.empty() || propPname.empty() || propPname == pName) {
          const string& id = p->GetID();
          RteDeviceProperty* pInserted = RteDeviceProperty::GetPropertyFromList(id, properties);
          if (p == pInserted)
            continue;
          if (pInserted == nullptr || !p->IsUnique()) {
            properties.push_back(p); // insert only unique properties or descriptions
          } else {
            // add missing attributes, but do not replace existing ones (we go down-up)
            // process sub-properties as well
            pInserted->CollectEffectiveContent(p);
          }
        }
      }
    }
  }
  if (!bRecursive)
    return;
  // get the rest from parent
  RteDeviceItem* parent = GetDeviceItemParent();
  if (parent)
    parent->CollectEffectiveProperties(tag, properties, pName);
}

// more complicated - gets all available properties
void RteDeviceItem::CollectEffectiveProperties(RteDevicePropertyMap& properties, const string& pName) const
{
  RteDevicePropertyMap::iterator dstIt;
  for (auto [tag, props] : m_properties) {
    auto dstIt = properties.find(tag);
    // add top containers if not yet exist
    if (dstIt == properties.end()) {
      list<RteDeviceProperty*> v;
      properties[tag] = v;
      dstIt = properties.find(tag);
    }
    CollectEffectiveProperties(tag, dstIt->second, pName, false);
  }
  // ask parent to fill the rest
  RteDeviceItem* parent = GetDeviceItemParent();
  if (parent)
    parent->CollectEffectiveProperties(properties, pName);
}


void RteDeviceItem::CollectEffectiveProperties(const string& pName)
{
  m_effectiveProperties[pName] = RteEffectiveProperties();
  auto it = m_effectiveProperties.find(pName);
  RteDevicePropertyMap& pmap = it->second.m_propertyMap;
  CollectEffectiveProperties(pmap, pName);
  for (auto [_, l] : pmap) {
    for (auto p : l) {
      p->CalculateCachedValues();
    }
  }
}


const list<RteDeviceProperty*>& RteEffectiveProperties::GetProperties(const string& tag) const {
  auto it = m_propertyMap.find(tag);
  if (it != m_propertyMap.end()) {
    return it->second;
  }
  return EMPTY_PROPERTY_LIST;
}


const RteDevicePropertyMap& RteDeviceItem::GetEffectiveProperties(const string& pName)
{
  if (m_effectiveProperties.empty()) {
    for (auto [pn, p] : m_processors) {
      CollectEffectiveProperties(pn);
    }
  }

  auto itp = m_effectiveProperties.find(pName);
  if (itp != m_effectiveProperties.end()) {
    return itp->second.m_propertyMap;
  }

  static const RteDevicePropertyMap EMPTY_PROPERTY_MAP;
  return EMPTY_PROPERTY_MAP;
}

const list<RteDeviceProperty*>& RteDeviceItem::GetEffectiveProperties(const string& tag, const string& pName)
{
  if (m_effectiveProperties.empty()) {
    GetEffectiveProperties(pName);
  }
  auto itp = m_effectiveProperties.find(pName);
  if (itp != m_effectiveProperties.end()) {
    const RteEffectiveProperties& effectiveProps = itp->second;
    return effectiveProps.GetProperties(tag);
  }
  return EMPTY_PROPERTY_LIST;
}

RteDeviceProperty* RteDeviceItem::GetSingleEffectiveProperty(const string& tag, const string& pName)
{
  const list<RteDeviceProperty*>& props = GetEffectiveProperties(tag, pName);
  if (!props.empty())
    return *props.begin();
  return nullptr;
}


void RteDeviceItem::GetEffectiveFilterAttributes(const string& pName, XmlItem& attributes)
{
  // get effective attributes: Dname, Dfamily, Dvendor, etc.
  GetEffectiveAttributes(attributes);

  // get attributes from effective processor
  RteDeviceProperty* cpu = GetSingleEffectiveProperty("processor", pName);
  if (cpu) {
    // and combine with attributes
    cpu->GetEffectiveAttributes(attributes);
  }
}

XMLTreeElement* RteDeviceItem::CreateEffectiveXmlTree(const string& pname, XMLTreeElement* parent)
{
  RtePackage* pack = GetPackage();
  if(!pack) {
    return nullptr;
  }
  // create pack element with its properties
  XMLTreeElement* packElement = pack->CreatePackXmlTreeElement(parent);
  // create mandatory family element
  XMLTreeElement* family = packElement->CreateElement("devices")->CreateElement("family");

  RteItem effectiveAttributes;
  GetEffectiveAttributes(effectiveAttributes);

  family->AddAttribute("Dfamily", effectiveAttributes.GetDeviceFamilyName());
  family->AddAttribute("Dvendor", GetVendorString());
  XMLTreeElement* device;
  if(GetType() == FAMILY) {
    device = family;
  } else {
    device = family->CreateElement("device");
    // set Dname attribute, will use Dvariant value in case of device variant
    device->AddAttribute("Dname", GetName());
  }

  const RteDevicePropertyMap& effectivePropMap = GetEffectiveProperties(pname);
  // first insert "processor" element to be conformant with PACK.xsd
  auto it = effectivePropMap.find("processor");
  if(it != effectivePropMap.end()) {
    CreateEffectiveXmlTreeElements(device, it->second);
  }
  // second insert "debugconfig" element to be conformant with PACK.xsd
  it = effectivePropMap.find("debugconfig");
  if(it != effectivePropMap.end()) {
    CreateEffectiveXmlTreeElements(device, it->second);
  }
  // all remaining properties
  for(it = effectivePropMap.begin(); it != effectivePropMap.end() ; ++it) {
    const auto& propGroupName = it->first;
    if(propGroupName == "processor" || propGroupName == "debugconfig"){
      continue; // already inserted
    }
    XMLTreeElement* groupParent = device;
    if(propGroupName == "sequence") {
      // all "sequence" elements  must be enclosed into a "sequences" tag
       groupParent = device->CreateElement("sequences");
    }
    CreateEffectiveXmlTreeElements(groupParent, it->second);
  }
  return packElement;
}

void RteDeviceItem::CreateEffectiveXmlTreeElements(XMLTreeElement* parent, const list<RteDeviceProperty*>& properties)
{
  for (auto p : properties) {
    p->CreateXmlTreeElement(parent);
  }
}

/////////////////////////////////////////////////////////////
void RteDevice::GetDevices(list<RteDevice*>& devices, const string& searchPattern) const
{
  RteDevice* d = const_cast<RteDevice*>(this);
  if (searchPattern.empty() || WildCards::Match(searchPattern, d->GetName()))
  {
    devices.push_back(d);
  } else {
    // any of variants matches?
    for (auto variant : m_deviceItems) {
      if (WildCards::Match(searchPattern, variant->GetName())) {
        devices.push_back(d);
        return;
      }
    }
  }
}
RteDevice* RteDevice::GetDevice() const
{
  return const_cast<RteDevice*>(this);
}

RteDevice* RteDeviceVariant::GetDevice() const
{
  return dynamic_cast<RteDevice*>(GetParent());
}


/////////////////////////////////////////////////////////////
RteDeviceFamilyContainer::RteDeviceFamilyContainer(RteItem* parent) :
  RteItem(parent)
{
}


RteItem* RteDeviceFamilyContainer::CreateItem(const std::string& tag)
{
  if (tag == "family") {
    return new RteDeviceFamily(this);
  }
  return RteItem::CreateItem(tag);
}

// device aggregation
RteDeviceItemAggregate::RteDeviceItemAggregate(const string& name, RteDeviceItem::TYPE type, RteDeviceItemAggregate* parent) :
  m_name(name),
  m_type(type),
  m_bDeprecated(false),
  m_parent(parent)
{
}


RteDeviceItemAggregate::~RteDeviceItemAggregate()
{
  Clear();
}

void RteDeviceItemAggregate::Clear()
{
  m_deviceItems.clear(); // not allocated in the class
  for (auto [_, child] : m_children) { // child aggregates
    delete child;
  }
  m_children.clear();
}



RteDeviceItemAggregate* RteDeviceItemAggregate::GetDeviceAggregate(const string& name) const
{
  auto it = m_children.find(name);
  if (it != m_children.end())
    return it->second;
  return nullptr;
}

// hierarchical search
RteDeviceItemAggregate* RteDeviceItemAggregate::GetDeviceAggregate(const string& deviceName, const string& vendor) const // returns a aggregate that contains RteDevice* or RetDeviceVariant* or processor
{
  if (m_type == RteDeviceItem::VENDOR_LIST && !vendor.empty()) {
    string vendorName = DeviceVendor::GetCanonicalVendorName(vendor);
    RteDeviceItemAggregate* da = GetDeviceAggregate(vendorName);
    if (da) {
      return da->GetDeviceAggregate(deviceName, vendor);
    }
  } else {
    RteDeviceItemAggregate* da = GetDeviceAggregate(deviceName);
    if (da && da->GetType() > RteDeviceItem::SUBFAMILY) {
      return da;
    }
    for (auto it = m_children.begin(); it != m_children.end(); ++it) {
      da = it->second->GetDeviceAggregate(deviceName, vendor);
      if (da && da->GetType() > RteDeviceItem::SUBFAMILY) {
        return da;
      }
    }
  }
  return nullptr;
}

// hierarchical search
RteDeviceItemAggregate* RteDeviceItemAggregate::GetDeviceItemAggregate(const string& name, const string& vendor) const // returns an aggregate with name (including families and subfamilies)
{

  if (m_type == RteDeviceItem::VENDOR_LIST && !vendor.empty()) {
    string vendorName = DeviceVendor::GetCanonicalVendorName(vendor);
    RteDeviceItemAggregate* da = GetDeviceAggregate(vendorName);
    if (da) {
      return da->GetDeviceItemAggregate(name, vendor);
    }
  } else {
    RteDeviceItemAggregate* da = GetDeviceAggregate(name);
    if (da) {
      return da;
    }
    for (auto it = m_children.begin(); it != m_children.end(); ++it) {
      da = it->second->GetDeviceItemAggregate(name, vendor);
      if (da) {
        return da;
      }
    }
  }
  return nullptr;
}


RteDeviceItem* RteDeviceItemAggregate::GetDeviceItem() const
{
  for (auto it = m_deviceItems.begin(); it != m_deviceItems.end(); ++it) {
    RteDeviceItem* device = it->second;
    auto ps = device->GetPackageState();
    if (ps == PackageState::PS_INSTALLED || ps == PackageState::PS_GENERATED ||
        ps == PackageState::PS_EXPLICIT_PATH) {
      return device;
    }
  }
  auto it = m_deviceItems.begin();
  if (it != m_deviceItems.end()) {
    return it->second;
  }
  return nullptr;
}

// hierarchical search
RteDeviceItem* RteDeviceItemAggregate::GetDeviceItem(const string& deviceName, const string& vendor) const // returns a RteDevice* or RetDeviceVariant*
{
  RteDeviceItemAggregate* da = GetDeviceAggregate(deviceName, vendor);
  if (da) {
    return da->GetDeviceItem();
  }
  return nullptr;
}

void RteDeviceItemAggregate::GetDevices(list<RteDevice*>& devices, const string& namePattern,
                                        const string& vendor, RteDeviceItem::TYPE depth) const
{
  if (m_type > depth) {
    return; // only devices of the requested depth (default RteDeviceItem::DEVICE) are collected
  }
  if (m_type == RteDeviceItem::VENDOR_LIST && !vendor.empty()) {
    string vendorName = DeviceVendor::GetCanonicalVendorName(vendor);
    RteDeviceItemAggregate* da = GetDeviceAggregate(vendorName);
    if (da) {
      da->GetDevices(devices, namePattern, vendor, depth);
    }
    return;
  } else if (m_type > RteDeviceItem::SUBFAMILY) {
    // take only top item:
    RteDevice* d = dynamic_cast<RteDevice*>(GetDeviceItem());
    if (d && (namePattern.empty() || WildCards::Match(namePattern, d->GetName()))) {
      devices.push_back(d);
    }
  }

  for (auto [_, da] : m_children) {
    da->GetDevices(devices, namePattern, vendor, depth);
  }
}


void RteDeviceItemAggregate::AddDeviceItem(RteDeviceItem* item)
{
  if (!item) {
    return;
  }
  RteDeviceItem::TYPE type = item->GetType();

  RteDeviceItemAggregate* dia = nullptr;
  if (m_type == type || m_type == RteDeviceItem::PROCESSOR) {
    RtePackage* pack = item->GetPackage();
    if (!pack) {
      return;
    }
    string packId = pack->GetPackageID();
    bool bDeprecated = pack->IsDeprecated();
    auto itd = m_deviceItems.find(packId);
    if (itd != m_deviceItems.end()) {
      RtePackage* packExisting = itd->second->GetPackage();
      if (packExisting && ( packExisting == pack || packExisting->GetPackageState() < pack->GetPackageState()))
        return; // installed package has preference
    }

    if (m_deviceItems.empty() || m_bDeprecated) {
      // First item, or previous item was deprecated.
      // Mark a device item as deprecated if there are only deprecated packs for it
      m_bDeprecated = bDeprecated;
    } else if (bDeprecated) {
      return; // do not insert deprecated devices in non-empty list
    }
    m_deviceItems[packId] = item;
    if (m_type == RteDeviceItem::PROCESSOR) {
      return;
    }
    int n = item->GetDeviceItemCount();
    if (n) {
      const list<RteDeviceItem*>& subItems = item->GetDeviceItems();
      for (auto di : subItems) {
        AddDeviceItem(di);
      }
    } else if (type >= RteDeviceItem::DEVICE && item->GetProcessorCount() > 1) {
      // add processor leaves
      const map<string, RteDeviceProperty*>& processors = item->GetProcessors();
      for (auto it = processors.begin(); it != processors.end(); ++it) {
        string name = item->GetName() + ':' + it->first;
        dia = GetDeviceAggregate(name);
        if (!dia) {
          dia = new RteDeviceItemAggregate(name, RteDeviceItem::PROCESSOR, this);
          m_children[name] = dia;
        }
        dia->AddDeviceItem(item);
      }
    }
    return;
  } else if (m_type == RteDeviceItem::VENDOR_LIST) {
    string vendorName = item->GetVendorName();
    dia = GetDeviceAggregate(vendorName);
    if (!dia) {
      dia = new RteDeviceItemAggregate(vendorName, RteDeviceItem::VENDOR, this);
      m_children[vendorName] = dia;
    }
    dia->AddDeviceItem(item);
    return;
  } else if (m_type > type) {// should not happen if algorithm is correct
    return;
  }

  // other cases
  const string& name = item->GetName();
  dia = GetDeviceAggregate(name);
  if (!dia) {
    dia = new RteDeviceItemAggregate(name, type, this);
    m_children[name] = dia;
  }
  dia->AddDeviceItem(item);
}


size_t RteDeviceItemAggregate::GetChildCount(RteDeviceItem::TYPE type) const
{
  size_t cnt = 0;
  auto& childmap = GetChildren();
  for (auto it = childmap.begin(); it != childmap.end(); ++it) {
    if (it->second && it->second->GetType() == type) {
      cnt++;
    }
  }

  return (cnt);
}


string RteDeviceItemAggregate::GetSummaryString() const
{
  string summary;

  RteDeviceItem* item = GetDeviceItem();
  if (item == 0) {
    return RteUtils::EMPTY_STRING;
  }
  RteDeviceItem::TYPE type = GetType();
  if (type != RteDeviceItem::DEVICE && type != RteDeviceItem::VARIANT) {
    return RteUtils::EMPTY_STRING;
  }

  RteDeviceProperty* procProperties = 0;
  list<RteDeviceProperty*> processors;
  item->GetEffectiveProcessors(processors);

  // Make a copy, simpler to merge with potential processor specific memories
  list<RteDeviceProperty*> mems = item->GetEffectiveProperties("memory", RteUtils::EMPTY_STRING);

  // Iterate over processors
  for (auto it = processors.begin(); it != processors.end(); ++it) {
    procProperties = *it;
    if (procProperties == 0) {
      continue;
    }

    const string& dcore = procProperties->GetAttribute("Dcore");
    const string& dclock = procProperties->GetAttribute("Dclock");

    // Processor type
    if (!summary.empty()) {
      summary += ", ";
    }
    if (dcore.empty()) {
      summary += "Unknown Processor";
    } else {
      summary += "ARM " + dcore;
    }

    // Clock value
    if (!dclock.empty()) {
      if (!summary.empty()) {
        summary += ", ";
      }
      summary += GetScaledClockFrequency(dclock);
    }

    // Collect unique memory attributes
    const list<RteDeviceProperty*>& procMems = item->GetEffectiveProperties("memory", procProperties->GetAttribute("Pname"));
    list<RteDeviceProperty*> addMems;
    for (auto procMemIt = procMems.begin(); procMemIt != procMems.end(); ++procMemIt) {
      auto memsIt = mems.begin();
      for (; memsIt != mems.end(); ++memsIt) {
        if ((*memsIt) == (*procMemIt)) {
          break;
        }
      }
      if (memsIt == mems.end()) {
        addMems.push_back(*procMemIt);
      }
    }

    if (!addMems.empty()) {
      mems.insert(mems.end(), addMems.begin(), addMems.end());
    }
  }

  // Memory (RAM/ROM)
  unsigned int ramSize = 0, romSize = 0;

  for (auto memsIt = mems.begin(); memsIt != mems.end(); ++memsIt) {
    RteDeviceMemory* mem = dynamic_cast<RteDeviceMemory*>(*memsIt);
    if (!mem) {
      continue;
    }
    if (mem->IsWriteAccess()) {
      ramSize += mem->GetAttributeAsUnsigned("size");
    } else if (mem->IsReadAccess()) {
      romSize += mem->GetAttributeAsUnsigned("size");
    }
  }

  if (ramSize > 0) {
    if (!summary.empty()) {
      summary += ", ";
    }
    summary += GetMemorySizeString(ramSize) + " RAM";
  }

  if (romSize > 0) {
    if (!summary.empty()) {
      summary += ", ";
    }
    summary += GetMemorySizeString(romSize) + " ROM";
  }

  return summary;
}

string RteDeviceItemAggregate::GetMemorySizeString(unsigned int size)
{
  if (size == 0) {
    return RteUtils::EMPTY_STRING;
  }

  if (size < 1024) {
    return (to_string((unsigned long long)size) + " Byte");
  }

  size >>= 10; // Scale to kByte
  if (size < 1024 || size % 1024) {
    // Less than a MByte or division with rest => show kByte
    return (to_string((unsigned long long)size) + " kB");
  }

  size >>= 10; // Scale to MByte
  return (to_string((unsigned long long)size) + " MB");
}

string RteDeviceItemAggregate::GetScaledClockFrequency(const string& dclock)
{
  if (dclock.empty()) {
    return RteUtils::EMPTY_STRING;
  }

  std::size_t len = dclock.length();
  if (len > 6) {
    return (dclock.substr(0, len - 6) + " MHz");
  } else if (len > 3) {
    return (dclock.substr(0, len - 3) + " kHz");
  } else {
    return (dclock + " Hz");
  }
}



RteDeviceVendor::RteDeviceVendor(const string& name) :
  m_name(name)
{
}

RteDeviceVendor::~RteDeviceVendor()
{
  Clear();
}

void RteDeviceVendor::Clear()
{
  m_devices.clear();
}

RteDevice* RteDeviceVendor::GetDevice(const string& fullDeviceName) const
{
  auto it = m_devices.find(fullDeviceName);
  if (it != m_devices.end())
    return it->second;
  if (fullDeviceName.find(':') > 0 ) {
    // try without processor name if specified
    string deviceName = RteUtils::GetPrefix(fullDeviceName);
    it = m_devices.find(deviceName);
    if (it != m_devices.end())
      return it->second;
  }
  return nullptr;
}

bool RteDeviceVendor::HasDevice(const string& fullDeviceName) const
{
  auto it = m_devices.find(fullDeviceName);
  if (it != m_devices.end())
    return true;
  if (fullDeviceName.find(':') > 0) {
    // try without processor name if specified
    string deviceName = RteUtils::GetPrefix(fullDeviceName);
    it = m_devices.find(deviceName);
    if (it != m_devices.end())
      return true;
  }
  return false;
}

void RteDeviceVendor::GetDevices(list<RteDevice*>& devices, const string& namePattern) const
{
  for (auto it = m_devices.begin(); it != m_devices.end(); ++it) {
    if (namePattern.empty() || WildCards::Match(namePattern, it->first)) {
      devices.push_back(it->second);
    }
  }
}

bool RteDeviceVendor::AddDeviceItem(RteDeviceItem* item)
{
  if (!item)
    return false;
  RteDeviceItem::TYPE type = item->GetType();
  int n = item->GetDeviceItemCount();
  if (n) {
    bool bInserted = false;
    const list<RteDeviceItem*>& subItems = item->GetDeviceItems();
    for (auto its : subItems) {
      if (AddDeviceItem(its))
        bInserted = true;
    }
    return bInserted; // return true if at least one item has been inserted
  }
  if (type > RteDeviceItem::SUBFAMILY) {
    RteDevice* d = dynamic_cast<RteDevice*>(item);
    if (d)
      return AddDevice(d);
  }
  return false;
}

bool RteDeviceVendor::AddDevice(RteDevice* item)
{
  if (!item)
    return false;
  const string& name = item->GetName();
  if (name.empty())
    return false;
  bool inserted = false;
  if (m_devices.find(name) == m_devices.end()) {
    m_devices[name] = item;
    inserted = true;
  }
  // add full names with processors
  if (item->GetProcessorCount() > 1) {
    // add processor leaves
    const map<string, RteDeviceProperty*>& processors = item->GetProcessors();
    for (auto it = processors.begin(); it != processors.end(); ++it) {
      string fullDeviceName = item->GetName() + ':' + it->first;
      if (m_devices.find(fullDeviceName) == m_devices.end()) {
        m_devices[fullDeviceName] = item;
        inserted = true;
      }
    }
  }
  return inserted;
}

// End of RteDevice.cpp
