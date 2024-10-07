/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "SvdItem.h"
#include "XMLTree.h"
#include "SvdDerivedFrom.h"
#include "SvdDimension.h"
#include "ErrLog.h"
#include "SvdEnum.h"
#include "SvdField.h"
#include "SvdRegister.h"
#include "SvdPeripheral.h"
#include "SvdCluster.h"
#include "SvdAddressBlock.h"
#include "SvdDimension.h"
#include "SvdTypes.h"

using namespace std;

#define DEFAULT_BITWIDTH     32
#define DEFAULT_RESETVALUE   0
#define DEFAULT_RESETMASK    0xffffffff

const uint32_t SvdItem::VALUE32_NOT_INIT = (uint32_t)-1;
const uint64_t SvdItem::VALUE64_NOT_INIT = (uint64_t)-1;

const string SvdItem::m_svdLevelStr[] = {
  "UNDEF",
  "Device",
  "Peripherals",
  "Peripheral",
  "Registers",
  "Cluster",
  "Register",
  "Fields",
  "Field",
  "EnumeratedValues",
  "EnumeratedValue",
  "Cpu",
  "AddressBlock",
  "Interrupt",
  "Dim",
  "DerivedFrom",
  "SauRegionsConfig",
  "SauRegion",
  "DimArrayIndex",
};


SvdElement::SvdElement() :
  m_bValid(true),     // all items are valid by default
  m_lineNumber(SvdItem::VALUE32_NOT_INIT),
  m_colNumber(SvdItem::VALUE32_NOT_INIT)
{
}

SvdElement::~SvdElement()
{
}

bool SvdElement::SetName(const string &name)
{
  m_name = name;
  return true;
}

const string &SvdElement::GetName()
{
  return m_name;
}

bool SvdElement::SetTag(const string &tag)
{
  m_tag = tag;
  return true;
}

void SvdElement::SetValid(bool bValid)
{
  m_bValid = bValid;
}

void SvdElement::Invalidate()
{
  m_bValid = false;
}

bool SvdElement::Construct(XMLTreeElement* xmlElement)
{
  if(!xmlElement) {
    return false;
  }

  // set attributes to this item
  SetLineNumber(xmlElement->GetLineNumber());
  SetColNumber(0); //xmlElement->GetColNumber();

  SetTag(xmlElement->GetTag());
  SetText(xmlElement->GetText());

  return true;
}


SvdItem::SvdItem(SvdItem* parent):
  m_parent(parent),
  m_copiedFrom(nullptr),
  m_derivedFrom(nullptr),
  m_dimension(nullptr),
  m_svdLevel(L_UNDEF),
  m_bitWidth(SvdItem::VALUE32_NOT_INIT),
  m_dimElementIndex(SvdItem::VALUE32_NOT_INIT),
  m_modified(false),
  m_bUsedForCExpression(false),
  m_protection(SvdTypes::ProtectionType::UNDEF)
{
}

SvdItem::~SvdItem()
{
  delete m_derivedFrom;
  delete m_dimension;     // Child items that are attached get deleted by ~SvdItem() !!!

  ClearChildren();
}

bool SvdItem::SetDescription(const string &descr)
{
  m_description = descr;
  return true;
}

const string &SvdItem::GetDescription()
{
  return m_description;
}

void SvdItem::Invalidate()
{
  SvdElement::Invalidate();

  auto itemName = GetNameCalculated();
  if(itemName.empty()) {
    itemName = GetName();
  }

  const auto svdLevel = GetSvdLevel();
  const auto& svdLevelStr = GetSvdLevelStr(svdLevel);
  const auto lineNo = GetLineNumber();

  string name;
  if(IsNameRequired()) {
    name = ": '";
    if(!itemName.empty()) {
      name += itemName;
    }
    else {
      name += "<unnamed>";
    }

    name += "'";
  }

  LogMsg("M211", LEVEL(svdLevelStr), NAME(name), lineNo);
}


bool SvdItem::Construct(XMLTreeElement* xmlElement)
{
	if(!xmlElement) {
		return false;
  }

  // set attributes to this item
  SetLineNumber(xmlElement->GetLineNumber());
  SetColNumber(0); //xmlElement->GetColNumber();
	SetTag(xmlElement->GetTag());
  SetText(xmlElement->GetText());
	bool success = ProcessXmlAttributes(xmlElement);
  success = ProcessXmlChildren(xmlElement);

  CalculateDim();
  Calculate();
  CheckItem();

	return success;
}

bool SvdItem::ProcessXmlChildren(XMLTreeElement* xmlElement)
{
	if(!xmlElement) {
		return false;
  }

	// process children
	const auto& childs = xmlElement->GetChildren();
	for(const auto child : childs) {
		if(!ProcessXmlElement(child)) {
			return false;
    }
	}

	return true;
}

bool SvdItem::ProcessXmlElement(XMLTreeElement* xmlElement)
{
  static uint32_t tagCnt=0;

  // default inserts element's text as attribute
	const auto& tag = xmlElement->GetTag();
	const auto& value = xmlElement->GetText();
  const auto lineNo = xmlElement->GetLineNumber();

  if(tag == "name") {
    SetName(value);
    const auto parent = GetParent();
    if(parent) {
      const auto svdLevel = parent->GetSvdLevel();
      if(svdLevel == L_EnumeratedValues || svdLevel == L_EnumeratedValue) {     // make dependent on "Generate enum lists" in future enhancement
        SetName(SvdUtils::CheckDescription(value, lineNo));
        return true;
      }
    }
    SetName(SvdUtils::CheckNameCCompliant(value, lineNo));
    return true;
	}
  else if(tag == "displayName" && GetSvdLevel() == L_Register) {
    return SetDisplayName(SvdUtils::CheckTextGeneric(value, lineNo));
	}
  else if(tag == "description") {
    return SetDescription(SvdUtils::CheckDescription(value, lineNo));
	}
  else if(tag == "protection") {
    const auto svdLevel = GetSvdLevel();
    if(svdLevel == L_Device || svdLevel == L_Peripheral || svdLevel == L_Register || svdLevel == L_Cluster) {
      if(!SvdUtils::ConvertProtectionStringType (value, m_protection, xmlElement->GetLineNumber())) {
        SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
      }
    }
    else {
      LogMsg("M201", TAG(tag), lineNo);    // report "Tag unknown"
    }
  }
  else if(tag == "SVDConv") {
    DebugModel(value);
    return true;
	}
  else if(tag.find("dim") != string::npos) { //tag == "dim" || tag == "dimIndex" || tag == "dimIncrement") {
    auto dimension = GetDimension();
    if(!dimension) {
      dimension = new SvdDimension(this);
      SetDimension(dimension);
    }
    return dimension->Construct(xmlElement);
  }
  else {    // report "Tag unknown"
    if(tagCnt++ < 10) {
      LogMsg("M201", TAG(tag), lineNo);
    }
  }

  return true;
}

bool SvdItem::ProcessXmlAttributes(XMLTreeElement* xmlElement)
{
  const auto& attributes = xmlElement->GetAttributes();
  if(!attributes.empty()) {
    for(const auto& [tag, value] : attributes) {
      if(tag == "derivedFrom") {
        if(GetDerivedFrom()) {
          LogMsg("M203", TAG(tag), VALUE(value), xmlElement->GetLineNumber());
        }

        const auto derivedFrom = new SvdDerivedFrom(this);
        SetDerivedFrom(derivedFrom);
        derivedFrom->Construct(xmlElement);
        derivedFrom->CalculateDerivedFrom();
      }
    }
  }

  return true;
}

bool SvdItem::AddAttribute(const string& name, const string& value, bool insertEmpty)
{
	if(name.empty()) {
		return false;
  }

	const auto it = m_attributes.find(name);
	if(it != m_attributes.end()) {
    if(it->second == value) {
      return false;
    }
    if(!insertEmpty && value.empty()) {
      m_attributes.erase(it);
      return true;
    }
  }

  if(insertEmpty || !value.empty()) {
    m_attributes[name] = value;
  }

  return true;
}

SvdDevice* SvdItem::GetDevice() const
{
  if(m_parent) {
    return m_parent->GetDevice();
  }

  return nullptr;
}

bool SvdItem::Validate()
{
  SetValid(true);  // assume valid

  for(const auto item : m_children) {
    if(!item) {
      continue;
    }

		if(!item->Validate()) {
      SetValid(false);
    }
	}

	return IsValid();
}

void SvdItem::AddItem(SvdItem* item)
{
  if(!item) {
    return;
  }

  m_children.push_back(item);
}

void SvdItem::ClearChildren()
{
  if(m_children.empty()) {
    return;
  }

  for(auto child : m_children) {
    delete child;
  }

  m_children.clear();
}

string SvdItem::GetHeaderTypeNameCalculated()
{
  SvdItem *item = this;

  if(!IsModified()) {
    const auto derivedFrom = GetDerivedFrom();
    if(derivedFrom) {
      item = derivedFrom->GetDerivedFromItem();
      if(!item) {
        item = this;
      }
    }
  }

  return item->GetNameCalculated();
}

string SvdItem::GetNameCalculated()
{
  const auto dim = GetDimension();
  if(dim) {
    return dim->GetExpression()->GetName();
  }

  return GetName();
}

const string &SvdItem::GetNameOriginal()
{
  auto parent = GetParent();
  if(parent) {
    const auto dim = dynamic_cast<SvdDimension*>(parent);
    if(dim) {
      parent = dim->GetParent();
      if(parent) {
        return parent->GetName();
      }
    }
  }

  return GetName();
}

string SvdItem::GetDisplayNameCalculated(bool bDataCheck /*= false*/)
{
  const auto dim = GetDimension();
  if(dim) {
    const auto& dispName = dim->GetDisplayName();
    if(!dispName.empty()) {
      return dispName;
    }
  }
  else {
    const auto& dispName = GetDisplayName();
    if(!dispName.empty()) {
      return dispName;
    }
  }

  if(bDataCheck) {
    return SvdUtils::EMPTY_STRING;
  }

  return GetNameCalculated();
}

string SvdItem::GetDescriptionCalculated(bool bDataCheck /*= false*/)
{
  const auto dim = GetDimension();
  if(dim) {
    const auto& descr = dim->GetDescription();
    if(!descr.empty()) {
      return descr;
    }
  }
  else {
    const auto& descr = GetDescription();
    if(!descr.empty()) {
      return descr;
    }
  }

  if(bDataCheck) {
    return SvdUtils::EMPTY_STRING;
  }

  return GetDisplayNameCalculated();
}

string SvdItem::GetDeriveName()
{
  const auto& name = GetName();
  string deriveName;
  const auto dim = GetDimension();
  if(dim) {
    const string& dimName = dim->GetDimName();
    deriveName += dimName;
  }

  deriveName += name;

  return deriveName;
}

bool SvdItem::SetDisplayName(const string &name)
{
  m_displayName = name;

  return true;
}

const string &SvdItem::GetDisplayName()
{
  return m_displayName;
}

const string& SvdItem::GetAlternate()
{
  return GetParent()->GetAlternate();
}

const string& SvdItem::GetPrependToName()
{
  return GetParent()->GetPrependToName();
}

const string& SvdItem::GetAppendToName()
{
  return GetParent()->GetAppendToName();
}

const string& SvdItem::GetHeaderDefinitionsPrefix()
{
  return GetParent()->GetHeaderDefinitionsPrefix();
}

string SvdItem::GetHierarchicalName()
{
  string name;
  SvdItem *parent = this;
  while(parent) {
    auto parName = parent->GetNameCalculated();
    if(!parName.empty()) {
      if(!name.empty()) name.insert(0, "_");
      name.insert(0, parName);
    }

    parent = parent->GetParent();
    if(parent && (parent->GetSvdLevel() == L_Device)) {
      break;
    }
  }

  const string altGrpName = GetAlternateGroup();
  if(!altGrpName.empty()) {
    name += "_";
    name += altGrpName;
  }

  return name;
}

string SvdItem::TryGetHeaderStructName(SvdItem *item)
{
  switch(item->GetSvdLevel()) {
    case L_Peripheral: {
      const auto peri = dynamic_cast<SvdPeripheral*>(item);
      if(peri) {
        return peri->GetHeaderStructName();
      }
    } break;
    case L_Cluster: {
      const auto clust = dynamic_cast<SvdCluster*>(item);
      if(clust) {
        return clust->GetHeaderStructName();
      }
    } break;
    default:
      return SvdUtils::EMPTY_STRING;
  }

  return SvdUtils::EMPTY_STRING;
}

string SvdItem::GetHierarchicalNameResulting()
{
  string name;

  SvdItem *parent = this;
  while(parent) {
    auto parName = TryGetHeaderStructName(parent);
    if(parName.empty()) {
      parName = parent->GetNameCalculated();
    }

    if(!parName.empty()) {
      if(!name.empty()) name.insert(0, "_");
      name.insert(0, parName);
    }

    parent = parent->GetParent();
    const auto dim = dynamic_cast<SvdDimension*>(parent);
    if(dim) {
      parent = parent->GetParent();
      if(parent) {
        parent = parent->GetParent();
      }
    }
    if(parent && (parent->GetSvdLevel() == L_Device)) {
      break;
    }
  }

  const string altGrpName = GetAlternateGroup();
  if(!altGrpName.empty()) {
    name += "_";
    name += altGrpName;
  }

  return name;
}

string SvdItem::GetParentRegisterNameHierarchical()
{
  string name;
  SvdItem *parent = this;
  while(parent) {
    const auto svdLevel = parent->GetSvdLevel();
    if(svdLevel == L_Register) {
      return parent->GetHierarchicalName();
    }

    parent = parent->GetParent();
    if(svdLevel == L_Device) {
      break;
    }
  }

  return SvdUtils::EMPTY_STRING;
}

const string& SvdItem::GetPeripheralName()
{
  SvdItem *parent = this;
  while(parent) {
    parent = parent->GetParent();
    if(parent->GetSvdLevel() == L_Peripheral) {
      return parent->GetName();
    }
  }

  return SvdUtils::EMPTY_STRING;
}

bool SvdItem::SetDerivedFrom(SvdDerivedFrom *derivedFrom)
{
  m_derivedFrom = derivedFrom;

  return true;
}

SvdDerivedFrom* SvdItem::GetDerivedFrom()
{
  return m_derivedFrom;
}

bool SvdItem::SetDimension(SvdDimension *dimension)
{
  m_dimension = dimension;

  return true;
}

SvdDimension* SvdItem::GetDimension()
{
  return m_dimension;
}

bool SvdItem::Calculate()
{
  return true;
}

bool SvdItem::IsNameRequired()
{
  const auto level = GetSvdLevel();
  switch(level) {
    case L_Device:
    case L_Peripheral:
    case L_Cluster:
    case L_Register:
    case L_Field:
    case L_EnumeratedValue:
    case L_Cpu:
    case L_Interrupt:
      return true;

    case L_UNDEF:
    case L_Peripherals:
    case L_Registers:
    case L_Fields:
    case L_EnumeratedValues:
    case L_AddressBlock:
    case L_Dim:
    case L_DerivedFrom:
    default:
      return false;
  }
}

bool SvdItem::IsDescrAllowed()
{
  const auto level = GetSvdLevel();
  switch(level) {
    case L_Device:
    case L_Peripheral:
    case L_Cluster:
    case L_Register:
    case L_Field:
    case L_EnumeratedValue:
    case L_Interrupt:
      return true;

    case L_UNDEF:
    case L_Cpu:
    case L_Peripherals:
    case L_Registers:
    case L_Fields:
    case L_EnumeratedValues:
    case L_AddressBlock:
    case L_Dim:
    case L_DerivedFrom:
    default:
      return false;
  }
}

uint64_t SvdItem::GetAbsoluteAddress  ()
{
  uint64_t addr = 0;
  SvdItem *parent = this;
  do {
    const auto svdLevel = parent->GetSvdLevel();
    if(svdLevel == L_Field) {
      parent = parent->GetParent();
      if(!parent) break;
      continue;
    }

    const auto offs = parent->GetAddress();
    addr += offs;

    if(svdLevel == L_Peripheral) {
      break;
    }

    if(svdLevel == L_Dim) {           // Dim Data
      parent = parent->GetParent();   // Dim parent
      if(!parent) {
        break;
      }

      parent = parent->GetParent();   // Dim parent
      if(!parent) {
        break;
      }
      continue;
    }

    parent = parent->GetParent();
  } while(parent);

  return addr;
}

uint64_t SvdItem::GetAbsoluteOffset  ()
{
  uint64_t addr = 0;
  SvdItem *parent = this;
  do {
    const auto offs = parent->GetAddress();
    addr += offs;
    const auto svdLevel = parent->GetSvdLevel();
    if(svdLevel == L_Registers) {
      break;
    }
    if(svdLevel == L_Dim) {           // Dim Data
      parent = parent->GetParent();   // Dim parent
      if(!parent) {
        break;
      }

      parent = parent->GetParent();   // Dim parent
      if(!parent) {
        break;
      }
      continue;
    }

    parent = parent->GetParent();
  } while(parent);

  return addr;
}

bool SvdItem::GetAbsoluteName(string &absName, char delimiter)
{
  list <SvdItem*> items;
  SvdItem *parent = this;
  while(parent) {
		items.push_back(parent);
    parent = parent->GetParent();
  }

  if(items.empty()) {
    return false;
  }

  absName = "";
  for(auto it = items.rbegin(); it != items.rend(); it++) {
    if(!absName.empty()) {
      absName += delimiter;
    }
    absName += (*it)->GetNameCalculated();
  }

  return true;
}

// visitor design pattern:
bool SvdItem::AcceptVisitor(SvdVisitor* visitor)
{
  if(visitor) {
    const auto res = visitor->Visit(this);
    if( res == VISIT_RESULT::CANCEL_VISIT) {
      return false;
    }

    if(res == VISIT_RESULT::CONTINUE_VISIT) {
      SvdDimension *dimItem = GetDimension();
      if(dimItem) {
        dimItem->AcceptVisitor(visitor);
      }
      const auto derivedItem = GetDerivedFrom();
      if(derivedItem) {
        derivedItem->AcceptVisitor(visitor);
      }

      for(const auto child : m_children) {
        if(!child->AcceptVisitor(visitor)) {
          return false;
        }
      }
    }
    return true;
  }

  return false;
}

bool SvdItem::FindChild (SvdItem *&item, const string &name)
{
  if(m_children.empty()) {
    return false;
  }

  return FindChild(m_children, item, name);
}

bool SvdItem::FindChild (const list<SvdItem*> childs, SvdItem *&item, const string &name)
{
  if(FindChildFromItem(item, name)) {
    return true;
  }

  if(childs.empty()) {
    return false;
  }

  for(const auto child : childs) {
    if(!child) {
      continue;
    }

    if(child->FindChildFromItem(item, name)) {
      return true;
    }
  }

  return false;
}

bool SvdItem::FindChildFromItem (SvdItem *&item, const string &name)
{
  // search item
  const auto thisName = GetDeriveName(); //GetName();
  if(!thisName.empty() && thisName == name) {
    item = this;
    return true;
  }

  // search dim items
  const auto dimension = GetDimension();
  if(dimension) {
    const auto& dimChilds = dimension->GetChildren();
    if(!dimChilds.empty()) {
      for(const auto child : dimChilds) {
        if(!child) {
          continue;
        }

        if(child->FindChild(item, name)) {
          return true;    // FindChild sets the item
        }
        else if(child->FindChildFromItem(item, name)) {
          return true;    // FindChild sets the item
        }
      }
    }
  }

  return false;
}

bool SvdItem::CalculateDim()
{
  return true;
}

bool SvdItem::CopyItem(SvdItem *from)
{
  const auto& name            = GetName        ();
  const auto& dispName        = GetDisplayName ();
  const auto& descr           = GetDescription ();
  const auto  lineNo          = GetLineNumber  ();
  const auto  bitWidth        = GetBitWidth    ();
  const auto  dimElementIndex = GetDimElementIndex();

  if(name     == "")                                { SetName        (from->GetName              ()); }
  if(dispName == "")                                { SetDisplayName (from->GetDisplayName       ()); }
  if(descr    == "")                                { SetDescription (from->GetDescription       ()); }
  if(lineNo   == -1)                                { SetLineNumber  (from->GetLineNumber        ()); }
  if(bitWidth == (int32_t)SvdItem::VALUE32_NOT_INIT){ SetBitWidth    (from->GetBitWidth          ()); }
  if(dimElementIndex == SvdItem::VALUE32_NOT_INIT)  { SetDimElementIndex(from->GetDimElementIndex()); }

  string tag = "Copied ";
  const string &oldTag = GetTag();
  if(oldTag == "") {
    tag += from->GetTag();
  }
  else {
    tag += oldTag;
  }

  SetTag          (tag);
  CopyDerivedFrom (this, from);
  CopyDim         (this, from);
  SetCopiedFrom   (      from);

  return true;
}

bool SvdItem::CopyChilds(SvdItem *from, SvdItem *hook)
{
  const auto& childs = from->GetChildren();
  if(childs.empty()) {
    return true;
  }

  for(const auto copy : childs) {
    if(!copy || !copy->IsValid()) {
      continue;
    }

    const auto lv = copy->GetSvdLevel();

    // Container
    if(lv == L_EnumeratedValues) {      // more <enumeratedValues> containers can be set!
      const auto nItem = new SvdEnumContainer(hook);
      hook->AddItem(nItem);
      CopyChilds(copy, nItem);
      nItem->CopyItem(copy);
    }
    else if(lv == L_Fields) {
      SvdFieldContainer *nItem;
      if(!hook->GetChildCount()) {
        nItem = new SvdFieldContainer(hook);
        hook->AddItem(nItem);
        nItem->CopyItem(copy);
      }
      else {
        nItem = (SvdFieldContainer *)*(hook->GetChildren().begin());
      }
      CopyChilds(copy, nItem);
    }
    else if(lv == L_Registers) {
      SvdRegisterContainer *nItem;
      if(!hook->GetChildCount()) {
        nItem = new SvdRegisterContainer(hook);
        hook->AddItem(nItem);
        nItem->CopyItem(copy);
      }
      else {
        nItem = (SvdRegisterContainer *)*(hook->GetChildren().begin());
      }
      CopyChilds(copy, nItem);
    }

    // Elements
    else if(lv == L_EnumeratedValue) {
      const auto nItem = new SvdEnum(hook);
      hook->AddItem(nItem);
      CopyChilds(copy, nItem);
      nItem->CopyItem(copy);
    }
    else if(lv == L_Field) {
      const auto nItem = new SvdField(hook);
      hook->AddItem(nItem);
      CopyChilds(copy, nItem);
      nItem->CopyItem(copy);
    }
    else if(lv == L_Register) {
      const auto nItem = new SvdRegister(hook);
      hook->AddItem(nItem);
      CopyChilds(copy, nItem);
      nItem->CopyItem(copy);
    }
    else if(lv == L_Peripheral) {
      const auto nItem = new SvdPeripheral(hook);
      hook->AddItem(nItem);
      CopyChilds(copy, nItem);
      nItem->CopyItem(copy);
    }
    else if(lv == L_Cluster) {
      const auto nItem = new SvdCluster(hook);
      hook->AddItem(nItem);
      CopyChilds(copy, nItem);
      nItem->CopyItem(copy);
    }
    else {
      //int ctrap = 0;
    }
  }

  return true;
}

bool SvdItem::CopyDerivedFrom(SvdItem *item, SvdItem *from)
{
  if(!from) {
    return true;
  }

  const auto copyDeriveFrom = from->GetDerivedFrom();
  if(copyDeriveFrom) {
    auto deriveFrom = item->GetDerivedFrom();
    if(!deriveFrom) {
      deriveFrom = new SvdDerivedFrom(item);
      item->SetDerivedFrom(deriveFrom);
      auto derivedFromItem = copyDeriveFrom->GetDerivedFromItem();
      if(!derivedFromItem) {
        derivedFromItem = from;
      }
      deriveFrom->SetDerivedFromItem(derivedFromItem);
      deriveFrom->CopyItem(copyDeriveFrom);
    }
  }

  return true;
}

bool SvdItem::CopyDim(SvdItem *item, SvdItem *from)
{
  bool bIsDimChild = false;
  const auto parent = GetParent();
  if(parent) {
    const auto dim = dynamic_cast<SvdDimension*>(parent);
    if(dim) {
      bIsDimChild = true;
    }
  }
  if(bIsDimChild) {
    return true;
  }

  const auto copyDim = from->GetDimension();
  if(copyDim) {
    auto dim = item->GetDimension();
    if(!dim) {
      dim = new SvdDimension(item);
      item->SetDimension(dim);
      dim->CopyItem(copyDim);
    }
  }

  return true;
}

void SvdItem::SetModified()
{
  m_modified = true;
}


// ------------------------------------------------------------------------
// ----------------------  Get Effective Properties  ----------------------
// ------------------------------------------------------------------------

uint32_t SvdItem::GetEffectiveBitWidth()
{
  for(auto parent=this; parent; parent=parent->GetParent()) {
    const auto val = parent->GetBitWidth();
    if(val != (int32_t)SvdItem::VALUE32_NOT_INIT) {
      return val;
    }
  }

  return DEFAULT_BITWIDTH;
}

uint64_t SvdItem::GetEffectiveResetValue()
{
  for(auto parent=this; parent; parent=parent->GetParent()) {
    const auto val = parent->GetResetValue();
    if(val != 0) {
      return val;
    }
  }

  return DEFAULT_RESETVALUE;
}

uint64_t SvdItem::GetEffectiveResetMask()
{
  for(auto parent=this; parent; parent=parent->GetParent()) {
    const auto val = parent->GetResetMask();
    if(val != 0) {
      return val;
    }
  }

  return DEFAULT_RESETMASK;
}

SvdTypes::Access SvdItem::GetEffectiveAccess()
{
  for(auto parent=this; parent; parent=parent->GetParent()) {
    const auto val = parent->GetAccess();
    if(val != SvdTypes::Access::UNDEF) {
      return val;
    }
  }

  return SvdTypes::Access::READWRITE;
}

SvdTypes::ModifiedWriteValue SvdItem::GetEffectiveModifiedWriteValue()
{
  for(auto parent=this; parent; parent=parent->GetParent()) {
    const auto val = parent->GetModifiedWriteValue();
    if(val != SvdTypes::ModifiedWriteValue::UNDEF) {
      return val;
    }
  }

  return SvdTypes::ModifiedWriteValue::UNDEF;
}

SvdTypes::ReadAction SvdItem::GetEffectiveReadAction()
{
  for(auto parent=this; parent; parent=parent->GetParent()) {
    const auto val = parent->GetReadAction();
    if(val != SvdTypes::ReadAction::UNDEF) {
      return val;
    }
  }

  return SvdTypes::ReadAction::UNDEF;
}

SvdTypes::ProtectionType SvdItem::GetEffectiveProtection()
{
  for(auto parent=this; parent; parent=parent->GetParent()) {
    const auto val = parent->GetProtection();
    if(val != SvdTypes::ProtectionType::UNDEF) {
      return val;
    }
  }

  return SvdTypes::ProtectionType::UNDEF;    // default, if UNDEF, then do not generate information (SFD, ...);
}

const string& SvdItem::GetSvdLevelStr(SVD_LEVEL level)
{
  return m_svdLevelStr[level];
}

const string& SvdItem::GetSvdLevelStr()
{
  return m_svdLevelStr[GetSvdLevel()];
}

void SvdItem::DebugModel(const string &value)
{
  //int ctrap = 0;
}

bool SvdItem::CheckItem()
{
  if(!IsValid()) {
    return true;
  }

  const auto svdLevel = GetSvdLevel();
  const auto& svdLevelStr = GetSvdLevelStr(svdLevel);
  string testReserved;
  const auto lineNo = GetLineNumber();
  const auto name = GetNameCalculated();

  if(!SvdUtils::CheckNameBrackets(name, lineNo)) {
    Invalidate();
  }

  if(name.empty()) {
    if(IsNameRequired()) {
      const auto dim = GetDimension();
      if(svdLevel != L_Peripheral && dim && dim->GetExpression()->GetType() == SvdTypes::Expression::EXTEND) {
        ;
      }
      else {
        LogMsg("M316", LEVEL(svdLevelStr), lineNo);
        Invalidate();
        return true;
      }
    }
  }
  else {
    if(*name.begin() == '_') {
      LogMsg("M321", LEVEL(svdLevelStr), ITEM("name"), NAME(name), lineNo);
    }

    if(name.length() > NAME_MAXLEN) {
      LogMsg("M334", LEVEL(svdLevelStr), ITEM("name"), NAME(name), lineNo);
    }

    testReserved = name;
    SvdUtils::ToLower(testReserved);
    if(testReserved.compare("reserved") == 0) {
      LogMsg("M361", LEVEL(svdLevelStr), ITEM("name"), NAME(name), lineNo);
      Invalidate();
    }

    if(svdLevel == L_Interrupt) {
      if(testReserved.find("irq") != string::npos) {
        LogMsg("M323", LEVEL(svdLevelStr), ITEM("name"), NAME(name), TXT("irq"), lineNo);
      }

      if(testReserved.find("int") != string::npos) {
        LogMsg("M323", LEVEL(svdLevelStr), ITEM("name"), NAME(name), TXT("int"), lineNo);
      }
    }
  }

  const auto dispName = GetDisplayNameCalculated(true);
  if(!dispName.empty()) {
    if(!name.empty()) {
      if(name.compare(dispName) == 0) {
        LogMsg("M318", LEVEL(svdLevelStr), TAG("displayName"), NAME(name), lineNo);
      }
    }

    if(*dispName.begin() == '_') {
      LogMsg("M321", LEVEL(svdLevelStr), ITEM("displayName"), NAME(dispName), lineNo);
    }

    testReserved = dispName;
    SvdUtils::ToLower(testReserved);
    if(testReserved.compare("reserved") == 0) {
      LogMsg("M361", LEVEL(svdLevelStr), ITEM("displayName"), NAME(dispName), lineNo);
      Invalidate();
    }
  }

  const auto descr = GetDescriptionCalculated(true);
  if(descr.empty()) {
    if(IsDescrAllowed()) {
      LogMsg("M317", LEVEL(svdLevelStr), lineNo);
    }
  }
  else {
    if(*descr.begin() == '_') {
      LogMsg("M321", LEVEL(svdLevelStr), ITEM("description"), NAME(descr), lineNo);
    }

    if(descr.length() > 2 && !descr.compare(descr.length()-2, 2, "\\n")) {
      LogMsg("M319", LEVEL(svdLevelStr), TAG("description"), NAME(descr), lineNo);
    }

    if(!name.empty()) {
      if(name.compare(descr) == 0) {
        LogMsg("M318", LEVEL(svdLevelStr), TAG("description"), NAME(name), lineNo);
      }
    }

    auto foundPos = descr.find(name);
    if(foundPos == 0) {    // found Peripheral name in description pos.0
      foundPos = name.length();
      if(descr[foundPos] == ' ') {
        if(descr.find(svdLevelStr, foundPos+1) != string::npos && descr.length() < name.length() + 2) {
          LogMsg("M320", LEVEL(svdLevelStr), NAME(descr), lineNo);
        }
      }
    }

    testReserved = descr;
    SvdUtils::ToLower(testReserved);
    if(testReserved.compare("reserved") == 0) {
      LogMsg("M362", LEVEL(svdLevelStr), ITEM("description"), NAME(descr), lineNo);
    }

    if(testReserved.compare("no description available") == 0) {
      LogMsg("M322", LEVEL(svdLevelStr), ITEM("description"), NAME(descr), lineNo);
      SetDescription("");
    }
  }

  const auto dim = GetDimension();
  if(!dim) {
    auto expr = SvdTypes::Expression::NONE;
    string newText;
    uint32_t pos=0;
    expr = SvdUtils::ParseExpression(name, newText, pos);
    if(expr != SvdTypes::Expression::NONE) {
      LogMsg("M207", NAME(newText), lineNo);
      Invalidate();
    }

    expr = SvdUtils::ParseExpression(dispName, newText, pos);
    if(expr != SvdTypes::Expression::NONE) {
      LogMsg("M207", NAME(newText), lineNo);
      Invalidate();
    }

    expr = SvdUtils::ParseExpression(descr, newText, pos);
    if(expr != SvdTypes::Expression::NONE) {
      LogMsg("M207", NAME(newText), lineNo);
      Invalidate();
    }
  }

  return true;
}
