/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "SvdItem.h"
#include "SvdPeripheral.h"
#include "SvdRegister.h"
#include "SvdEnum.h"
#include "SvdAddressBlock.h"
#include "SvdInterrupt.h"
#include "SvdDimension.h"
#include "SvdRegister.h"
#include "SvdCluster.h"
#include "SvdField.h"
#include "SvdDerivedFrom.h"
#include "SvdDimension.h"
#include "XMLTree.h"
#include "SvdUtils.h"
#include "ErrLog.h"

using namespace std;


SvdPeripheralContainer::SvdPeripheralContainer(SvdItem* parent):
  SvdItem(parent)
{
  SetSvdLevel(L_Peripherals);
}

SvdPeripheralContainer::~SvdPeripheralContainer()
{
}

bool SvdPeripheralContainer::Construct(XMLTreeElement* xmlElement)
{
  return SvdItem::Construct(xmlElement);
}

bool SvdPeripheralContainer::ProcessXmlElement(XMLTreeElement* xmlElement)
{
  const string& tag = xmlElement->GetTag();

  if(tag == "peripheral") {
    const auto peripheral = new SvdPeripheral(this);
    AddItem(peripheral);
    return peripheral->Construct(xmlElement);
  }

  return SvdItem::ProcessXmlElement(xmlElement);
}

bool SvdPeripheralContainer::CopyItem(SvdItem *from)
{
  return SvdItem::CopyItem(from);
}

bool SvdPeripheralContainer::CheckItem()
{
  if(!IsValid()) {
    return true;
  }

  const auto& childs = GetChildren();
  for(const auto child : childs) {
    if(!child->IsValid()) {
      continue;
    }

    const auto peri = dynamic_cast<SvdPeripheral*>(child);
    if(!peri) {
      continue;
    }

    peri->CalcDisableCondition();
  }

  return true;
}


SvdPeripheral::SvdPeripheral(SvdItem* parent):
  SvdItem(parent),
  m_enumContainer(nullptr),
  m_disableCondition(nullptr),
  m_hasAnnonUnions(false),
  m_calcSize(0),
  m_resetValue(0),
  m_resetMask(0),
  m_access(SvdTypes::Access::UNDEF)
{
  SetSvdLevel(L_Peripheral);
  m_addressBlock.clear();
  m_interrupt.clear();
}

SvdPeripheral::~SvdPeripheral()
{
  for(const auto addrBlock : m_addressBlock) {
    delete addrBlock;
  }

  for(const auto interrupt : m_interrupt) {
    delete interrupt;
  }

  delete m_disableCondition;
}

bool SvdPeripheral::Construct(XMLTreeElement* xmlElement)
{
  return SvdItem::Construct(xmlElement);
}

bool SvdPeripheral::ProcessXmlElement(XMLTreeElement* xmlElement)
{
  const auto& tag   = xmlElement->GetTag();
  const auto& value = xmlElement->GetText();

  if(tag == "version") {
    m_version = value;
    return true;
  }
  else if(tag == "groupName") {
    m_groupName = value;
    return true;
  }
  else if(tag == "headerStructName") {
    m_headerStructName = value;
    SetModified();
    return true;
  }
  else if(tag == "alternatePeripheral") {
    m_alternate = value;
    return true;
  }
  else if(tag == "prependToName") {
    m_prependToName = value;
    SetModified();
    return true;
  }
  else if(tag == "appendToName") {
    m_appendToName = value;
    SetModified();
    return true;
  }
  else if(tag == "disableCondition") {
    string disableCondition;
    if(!SvdUtils::ConvertCExpression(value, disableCondition)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
      return true;
    }

    if(m_disableCondition) {
      if(IsDerived()) {
        m_disableCondition = 0;  // do not delete derived one, pointer copied
      }
      else {
        LogMsg("M246");   // only one item allowed
        return true;
      }
    }

    m_disableCondition = new SvdCExpression();

    return m_disableCondition->Construct(xmlElement);
  }
  else if(tag == "baseAddress") {
    if(!SvdUtils::ConvertNumber(value, m_address.u64)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    else {
      m_address.bValid = true;
    }
    return true;
  }
  else if(tag == "size") {
    uint32_t num = 0;
    if(!SvdUtils::ConvertNumber(value, num)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    SetBitWidth(num);
    SetModified();
    return true;
  }
  else if(tag == "access") {
    if(!SvdUtils::ConvertAccess(value, m_access, xmlElement->GetLineNumber())) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    return true;
  }
  else if(tag == "resetValue") {
    if(!SvdUtils::ConvertNumber(value, m_resetValue)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    return true;
  }
  else if(tag == "resetMask") {
    if(!SvdUtils::ConvertNumber(value, m_resetMask)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    return true;
  }
  else if(tag == "addressBlock") {
    const auto addressBlock = new SvdAddressBlock(this);
    AddAddressBlock(addressBlock);
    SetModified();
    return addressBlock->Construct(xmlElement);
  }
  else if(tag == "interrupt") {
    const auto interrupt = new SvdInterrupt(this);
    AddInterrupt(interrupt);
    return interrupt->Construct(xmlElement);
  }
  else if(tag == "registers") {
     auto registerContainer = GetRegisterContainer();
    if(!registerContainer) {
      registerContainer = new SvdRegisterContainer(this);
      AddItem(registerContainer);
    }
    SetModified();
    return registerContainer->Construct(xmlElement);
  }
  else if(tag == "dimArrayIndex") {     // enumeratedValues
    if(!m_enumContainer) {
      m_enumContainer = new SvdEnumContainer(this);
    } else {
      LogMsg("M228");   // only one item allowed
      return true;
    }
    return m_enumContainer->Construct(xmlElement);
  }

  return SvdItem::ProcessXmlElement(xmlElement);
}

bool SvdPeripheral::ProcessXmlAttributes(XMLTreeElement* xmlElement)
{
  return SvdItem::ProcessXmlAttributes(xmlElement);
}

SvdRegisterContainer* SvdPeripheral::GetRegisterContainer() const
{
  if(!GetChildCount()) {
    return nullptr;
  }

  const auto cont = dynamic_cast<SvdRegisterContainer*>(*(GetChildren().begin()));

  return cont;
}

list<SvdAddressBlock*>& SvdPeripheral::GetAddressBlock()
{
  return m_addressBlock;
}

list<SvdInterrupt*>& SvdPeripheral::GetInterrupt()
{
  return m_interrupt;
}

void SvdPeripheral::AddAddressBlock (SvdAddressBlock* addrBlock)
{
  m_addressBlock.push_back(addrBlock);
}

void SvdPeripheral::AddInterrupt (SvdInterrupt* interrupt)
{
  m_interrupt.push_back(interrupt);
}

string SvdPeripheral::GetHeaderTypeName()
{
  string name = GetHeaderDefinitionsPrefix();

  if(IsModified()) {
    if(!m_headerStructName.empty()) {
      name += m_headerStructName;
      return name;
    }

    name += GetNameCalculated();
    return name;
  }

  const auto derivedFrom = GetDerivedFrom();
  if(derivedFrom) {
    const auto item = derivedFrom->GetDerivedFromItem();
    const auto origPeri = dynamic_cast<SvdPeripheral*>(item);
    if(origPeri) {
      return origPeri->GetHeaderTypeName();
    }
  }
  else {
    const auto item = GetCopiedFrom();
    const auto origPeri = dynamic_cast<SvdPeripheral*>(item);
    if(origPeri) {
      const auto& headerDefP = origPeri->m_headerStructName;
      return headerDefP;
    }
  }

  return SvdUtils::EMPTY_STRING;
}

uint32_t SvdPeripheral::GetSize()
{
  if(!m_calcSize) {
    if(!IsModified()) {
      const auto copiedFrom = GetCopiedFrom();
      if(copiedFrom) {
        return copiedFrom->GetSize();
      }
      const auto derivedFrom = GetDerivedFrom();
      if(derivedFrom) {
        const auto item = derivedFrom->GetDerivedFromItem();
        if(item) {
          return item->GetSize();
        }
      }
    }
  }
  return m_calcSize;
}

uint32_t SvdPeripheral::SetSize(uint32_t size)
{
  m_calcSize = size;

  return true;
}

string SvdPeripheral::GetNameCalculated()
{
  auto name = SvdItem::GetNameCalculated();
  const auto& altGrp = GetAlternateGroup();
  if(!altGrp.empty()) {
    name += "_";
    name += altGrp;
  }

  return name;
}

bool SvdPeripheral::CopyItem(SvdItem *from)
{
  const auto pFrom = dynamic_cast<SvdPeripheral*>(from);
  if(!pFrom) {
    return false;
  }

  // Check if values are already set (override from derived)
  const auto& version          = GetVersion          ();
  const auto& groupName        = GetGroupName        ();
//const auto& headerStructName = GetHeaderStructName ();
  const auto& alternate        = GetAlternate        ();
  const auto& prependToName    = GetPrependToName    ();
  const auto& appendToName     = GetAppendToName     ();
  const auto  disableCondition = GetDisableCondition ();
  const auto  address          = GetAddress          ();
  const auto  resetValue       = GetResetValue       ();
  const auto  resetMask        = GetResetMask        ();
  const auto  access           = GetAccess           ();

  if(version          == "")                      { SetVersion             (pFrom->GetVersion             ()); }
  if(groupName        == "")                      { SetGroupName           (pFrom->GetGroupName           ()); }
//if(headerStructName == "")                      { SetHeaderStructName    (pFrom->GetHeaderStructName    ()); }
  if(alternate        == "")                      { SetAlternate           (pFrom->GetAlternate           ()); }
  if(prependToName    == "")                      { SetPrependToName       (pFrom->GetPrependToName       ()); }
  if(appendToName     == "")                      { SetAppendToName        (pFrom->GetAppendToName        ()); }
  if(disableCondition == nullptr)                 { SetDisableCondition    (pFrom->GetDisableCondition    ()); }  // TODO: Add a copy mechanism?
  if(address          == 0 )                      { SetAddress             (pFrom->GetAddress             ()); }
  if(resetValue       == 0 )                      { SetResetValue          (pFrom->GetResetValue          ()); }
  if(resetMask        == 0 )                      { SetResetMask           (pFrom->GetResetMask           ()); }
  if(access           == SvdTypes::Access::UNDEF) { SetAccess              (pFrom->GetAccess              ()); }

  CopyAddressBlocks(pFrom);
  SvdItem::CopyItem(pFrom);
  CalculateDim();

  return false;
}

bool SvdPeripheral::CopyAddressBlocks(SvdPeripheral *from)
{
  const auto& addrBlocks = from->GetAddressBlock();
  for(const auto addrBlock : addrBlocks) {
    if(!addrBlock || !addrBlock->IsValid()) {
      continue;
    }
    if(addrBlock->IsMerged()) {
      continue;
    }

    const auto addressBlock = new SvdAddressBlock(this);
    addressBlock->CopyItem(addrBlock);
    AddAddressBlock(addressBlock);
  }

  return true;
}

bool SvdPeripheral::CalculateMaxPaddingWidth()
{
  uint32_t maxWidth = 0;

  SvdRegisterContainer* regCont = GetRegisterContainer();
  if(regCont) {
    const auto& childs = regCont->GetChildren();
    for(const auto child : childs) {
      if(!child->IsValid()) {
        continue;
      }

      const auto width = child->GetEffectiveBitWidth();
      if(width > maxWidth) {
        maxWidth = width;
      }
    }
  }

  if(!maxWidth) {
    maxWidth = 8;
  }

  SetBitWidth(maxWidth);

  return true;
}

bool SvdPeripheral::Calculate()
{
  CalculateMaxPaddingWidth();

  return SvdItem::Calculate();
}

bool SvdPeripheral::CalculateDim()
{
  const auto dim = GetDimension();
  if(!dim) {
    return true;
  }

  const auto& childs = dim->GetChildren();
  if(!childs.empty()) {
    dim->ClearChildren();
  }

  dim->CalculateDim();

  const auto& dimIndexList = dim->GetDimIndexList();
  const auto parent = dynamic_cast<SvdPeripheral*>(dim->GetParent());
  if(!parent) {
    return false;
  }

  auto address = parent->GetAbsoluteAddress();
  uint32_t dimElementIndex = 0;
  for(const auto& dimIndexname : dimIndexList) {
    const auto newPeri = new SvdPeripheral(dim);
    dim->AddItem(newPeri);
    CopyDerivedFrom(newPeri, this);
    CopyChilds(parent, newPeri);

    newPeri->CopyItem               (parent);
    newPeri->SetName                (dim->CreateName(dimIndexname));
    newPeri->SetDisplayName         (dim->CreateDisplayName(dimIndexname));
    newPeri->SetDescription         (dim->CreateDescription(dimIndexname));
    newPeri->SetDimElementIndex     (dimElementIndex++);
    newPeri->SetAddress             (address);
    address += dim->GetDimIncrement();
  }

  string dimIndexText;
  if(dimIndexList.size() >= 1) {
    dimIndexText = *dimIndexList.begin();

    if(dimIndexList.size() > 1) {
      dimIndexText += "..";
      dimIndexText += *dimIndexList.rbegin();
    }
  }

  const auto name = dim->CreateName("");
  dim->SetName(name);

  string dName = "[";
  dName += dimIndexText;
  dName += "]";
  const auto dispName = dim->CreateDisplayName(dName);
  dim->SetDisplayName(dispName);

  string descr = "[";
  descr += dimIndexText;
  descr += "]";
  const auto description = dim->CreateDescription(descr);
  dim->SetDescription(description);

  return true;
}

bool SvdPeripheral::CopyRegisterContainer(SvdPeripheral *newPeri)
{
  const auto fromCont = GetRegisterContainer();
  if(!fromCont) {
    return true;
  }

  auto regCont = newPeri->GetRegisterContainer();
  if(!regCont) {
    regCont = new SvdRegisterContainer(this);
    newPeri->AddItem(regCont);
  }

  const auto& childs = fromCont->GetChildren();
  if(childs.empty()) {
    return true;
  }

  for(const auto child : childs) {
    const auto reg = dynamic_cast<SvdRegister*>(child);
    if(reg) {
      const auto newReg = new SvdRegister(regCont);
      newPeri->AddItem(newReg);
      newReg->CopyItem(reg);
      continue;
    }

    const auto clust = dynamic_cast<SvdCluster*>(child);
    if(clust) {
      const auto newClust = new SvdCluster(regCont);
      newPeri->AddItem(newClust);
      continue;
    }
  }

  return true;
}

bool SvdPeripheral::SearchAlternateMap(SvdRegister *reg, map<uint32_t, list<SvdRegister*> > &alternateMap)
{
  const auto offs = (uint32_t) reg->GetAbsoluteOffset();
  const auto& rList = alternateMap[offs];
  if(rList.empty()) {
    return false;
  }

  const auto& altGrpname = reg->GetAlternateGroup();
  if(!altGrpname.empty()) {
    return true;
  }

  const auto& altRegname = reg->GetAlternate();
  if(!altRegname.empty()) {
    for(const auto r : rList) {
      if(!r) {
        continue;
      }

      const auto nam = r->GetNameCalculated();
      if(altRegname.compare(nam) == 0) {
        return true;
      }
      const auto& origNam = r->GetNameOriginal();
      if(altRegname.compare(origNam) == 0) {
        return true;
      }
    }
  }

  return false;
}

SvdTypes::SvdConvV2accType SvdPeripheral::ConvertAccessToSVDConvV2(SvdTypes::Access access)
{
  switch(access) {
    case SvdTypes::Access::READONLY:
      return SvdTypes::SvdConvV2accType::READONLY;
    case SvdTypes::Access::WRITEONLY:
      return SvdTypes::SvdConvV2accType::WRITEONLY;
    case SvdTypes::Access::WRITEONCE:
      return SvdTypes::SvdConvV2accType::WRITEONLY;
    case SvdTypes::Access::READWRITEONCE:
      return SvdTypes::SvdConvV2accType::READWRITE;
    case SvdTypes::Access::READWRITE:
      return SvdTypes::SvdConvV2accType::READWRITE;
    default:
      return SvdTypes::SvdConvV2accType::EMPTY;
  }
}

SvdTypes::Access SvdPeripheral::ConvertAccessFromSVDConvV2(SvdTypes::SvdConvV2accType access)
{
  switch(access) {
    case SvdTypes::SvdConvV2accType::EMPTY:
      return SvdTypes::Access::UNDEF;
    case SvdTypes::SvdConvV2accType::READ:
      return SvdTypes::Access::READONLY;
    case SvdTypes::SvdConvV2accType::READONLY:
      return SvdTypes::Access::READONLY;
    case SvdTypes::SvdConvV2accType::WRITE:
      return SvdTypes::Access::WRITEONLY;
    case SvdTypes::SvdConvV2accType::WRITEONLY:
      return SvdTypes::Access::WRITEONLY;
    case SvdTypes::SvdConvV2accType::READWRITE:
      return SvdTypes::Access::READWRITE;
    case SvdTypes::SvdConvV2accType::UNDEF:
      return SvdTypes::Access::UNDEF;
    default:
      return SvdTypes::Access::UNDEF;
  }
}

SvdTypes::Access SvdPeripheral::CalcAccessSVDConvV2(SvdRegister* reg)
{
  const auto access = reg->GetAccess();     // if set, use
  if(access != SvdTypes::Access::UNDEF) {
    return access;
  }

  auto v2accType  = SvdTypes::SvdConvV2accType::EMPTY;
  auto v2accField = SvdTypes::SvdConvV2accType::EMPTY;

  const auto& childs = reg->GetChildren();
  for(const auto child : childs) {
    const auto field = dynamic_cast<SvdField*>(child);
    if(!field) {
      continue;
    }

    v2accField = ConvertAccessToSVDConvV2(field->GetAccess());
    if(v2accType < v2accField) {
      v2accType = v2accField;
    }
  }

  return ConvertAccessFromSVDConvV2(v2accType);
}

bool SvdPeripheral::AddToMap(SvdRegister* reg, map<uint32_t, list<SvdRegister*> > &regMap, map<uint32_t, list<SvdRegister*> > &alternateMap, bool bSilent /*= 0*/)
{
  const auto lineNo = reg->GetLineNumber();
  const auto size = reg->GetEffectiveBitWidth();
  const auto offs = (uint32_t) reg->GetAbsoluteOffset();

  const auto name = reg->GetNameCalculated();
  if(name.empty()) {
    return true;
  }

  bool ok = true;
  const auto& rList = regMap[offs];
  if(!rList.empty()) {
    const auto& altRegname = reg->GetAlternate();
    ok = false;

    const auto& altGrpname = reg->GetAlternateGroup();
    if(!altGrpname.empty()) {
      ok = true;
    }
    else {
      if(!altRegname.empty()) {
        for(auto it = rList.begin(); it != rList.end(); it++) {
          const auto r = *it;
          if(!r) {
            continue;
          }

          const auto nam = r->GetNameCalculated();
          if(altRegname.compare(nam) == 0) {
            ok = true;
            break;
          }
          const auto& origNam = r->GetNameOriginal();
          if(altRegname.compare(origNam) == 0) {
            ok = true;
            break;
          }
        }

        if(!ok) {   // search altMap
          ok = SearchAlternateMap(reg, alternateMap);
        }
      }
    }

    if(!ok) {   // alternateGroup, compatibillity to old SVDConv
      for(const auto r : rList) {
        if(!r) {
          continue;
        }

        const auto& altGrp = r->GetAlternateGroup();
        if(!altGrpname.empty() && !altGrp.empty()) {
          if(altGrpname != altGrp) {
            ok = true;
            break;
          }
        }
        else if(!altGrp.empty()) {
          ok = true;
          break;
        }
      }
    }

    if(!bSilent && !ok) {     // report first register on this address
      const auto& accStr = SvdTypes::GetAccessType(reg->GetEffectiveAccess());
      const auto r = *rList.begin();
      const auto nam = r->GetNameCalculated();
      const auto off = (uint32_t) r->GetAbsoluteOffset();
      const auto siz = r->GetEffectiveBitWidth();
      const auto& aStr = SvdTypes::GetAccessType(r->GetEffectiveAccess());

      if(!altRegname.empty()) {
        LogMsg("M348", LEVEL("Register"), NAME(altRegname), ADDR(offs), lineNo);
      }

      if(((r->GetParent() && r->GetParent()->GetSvdLevel() == L_Dim) || (reg->GetParent() && reg->GetParent()->GetSvdLevel() == L_Dim)) ||
        ((r->GetParent() && r->GetParent()->GetSvdLevel() == L_Cluster) || (reg->GetParent() && reg->GetParent()->GetSvdLevel() == L_Cluster)) ||
        (r->GetAddress() != reg->GetAddress()) ||
        (r->GetEffectiveAccess() != r->GetAccessCalculated() || reg->GetEffectiveAccess() != reg->GetAccessCalculated()) ||
        (CalcAccessSVDConvV2(r) != CalcAccessSVDConvV2(reg)) ) {      // compatibility to old SVDConv (did not check all regs, i.e. on <dim>)
        LogMsg("M365", NAME(name), ADDRSIZE(offs, size), ACCESS(accStr), NAME2(nam), ACCESS2(aStr), ADDRSIZE2(off, siz), LINE2(r->GetLineNumber()), lineNo);      // warning
      } else {
        LogMsg("M339", NAME(name), ADDRSIZE(offs, size), ACCESS(accStr), NAME2(nam), ACCESS2(aStr), ADDRSIZE2(off, siz), LINE2(r->GetLineNumber()), lineNo);      // error
      }
      //reg->Invalidate();
    }
  }

  auto width = size / 8;
  if(width > 8) {
    width = 8;
  }

  for(uint32_t i=0; i<width; i++) {
    regMap[offs + i].push_back(reg);
  }

  return ok;
}

bool SvdPeripheral::AddToMap(SvdCluster* clust, map<uint32_t, list<SvdCluster*> > &clustMap, bool bSilent /*= 0*/)
{
  bool ok = true;
  const auto name = clust->GetNameCalculated();
  const auto lineNo = clust->GetLineNumber();
  const auto offs = (uint32_t) clust->GetAbsoluteOffset();
  const auto& altRegname = clust->GetAlternate();

  if(name.empty()) {
    return true;
  }

  const auto&cList = clustMap[offs];
  if(!cList.empty()) {
    ok = false;
    if(!altRegname.empty()) {
      for(const auto c : cList) {
        if(!c) {
          continue;
        }

        const auto nam = c->GetNameCalculated();
        if(altRegname.compare(nam) == 0) {
          ok = true;
          break;
        }
      }
    }

    if(!bSilent && !ok) {                           // report first register on this address
      SvdCluster* r = *cList.begin();
      const string nam = r->GetNameCalculated();
      LogMsg("M368"/*"M343"*/, LEVEL("Cluster"), NAME(name), ADDR(offs), NAME2(nam), LINE2(r->GetLineNumber()), lineNo);    // warning
      //clust->Invalidate();
    }
  }

  clustMap[offs].push_back(clust);

  return ok;
}

// Check for conflicting names
bool SvdPeripheral::AddToMap(SvdItem *item, map<string, SvdItem*>& map)
{
  auto name = item->GetNameCalculated();
  const auto itemAltGrp = item->GetAlternateGroup();
  const auto lineNo = item->GetLineNumber();

  if(!itemAltGrp.empty()) {
    name += "_";
    name += itemAltGrp;
  }

  if(name.empty()) {
    return true;
  }

  const auto existingItem = map[name];
  if(existingItem) {
    const auto svdLevel = item->GetSvdLevel();
    const auto& svdLevelStr = GetSvdLevelStr(svdLevel);
    LogMsg("M336", LEVEL(svdLevelStr), NAME(name), LINE2(existingItem->GetLineNumber()), lineNo);
    item->Invalidate();
  }
  else {
    map[name] = item;
  }

  return true;
}

// Add to address space (do checks for: is union needed?)
bool SvdPeripheral::AddToMap(SvdItem *item, map<uint64_t, list<SvdItem*> >& map)
{
  const auto itemAddr = item->GetAbsoluteAddress();
  const auto& existingItem = map[itemAddr];
  if(!existingItem.empty()) {
    SetHasAnnonUnions();
  }

  map[itemAddr].push_back(item);

  return true;
}

// Check for conflicting displayNames
bool SvdPeripheral::AddToMap_DisplayName(SvdItem *item, map<string, SvdItem*>& map)
{
  auto name = item->GetDisplayNameCalculated();
  const auto lineNo = item->GetLineNumber();

  if(name.empty()) {
    return true;
  }

  const auto existingItem = map[name];
  if(existingItem) {
    const auto svdLevel = item->GetSvdLevel();
    const auto& svdLevelStr = GetSvdLevelStr(svdLevel);
    const auto tag = "displayName";
    LogMsg("M373", LEVEL(svdLevelStr), NAME(name), TAG(tag), LINE2(existingItem->GetLineNumber()), lineNo);
  }
  else {
    map[name] = item;
  }

  return true;
}

// Check if Register is inside peri's addressBlocks
bool SvdPeripheral::CheckRegisterAddress(SvdRegister* reg, const list<SvdAddressBlock*>& addrBlocks)
{
  if(addrBlocks.empty()) {
    return true;
  }

  const auto regOffs   = (uint32_t) reg->GetAbsoluteOffset();
  const auto regWidth  = reg->GetEffectiveBitWidth() / 8;
  const auto regMax    = regOffs + regWidth -1;

  bool found = false;
  string addrBlkText;
  uint32_t i=0;

  for(const auto addrBlock : addrBlocks) {
    if(!addrBlock) {
      continue;
    }

    const auto offs = addrBlock->GetOffset();
    const auto size = addrBlock->GetSize();
    const auto usage = addrBlock->GetUsage();

    if(!addrBlock->IsValid()) {
      if(!addrBlkText.empty()) {
        addrBlkText += "\n";
      }

      addrBlkText += "    ";
      addrBlkText += to_string(i++);
      addrBlkText += "  :   Invalid AddressBlock        ";

      addrBlkText += "Offs: ";
      if(offs != SvdItem::VALUE32_NOT_INIT) {
        addrBlkText += SvdUtils::CreateHexNum(offs, 4);
      }
      else {
        addrBlkText += " ---  ";
      }

      addrBlkText += ", Size: ";
      if(size != SvdItem::VALUE32_NOT_INIT) {
        addrBlkText += SvdUtils::CreateHexNum(size, 4);
      }
      else {
        addrBlkText += " ---  ";
      }

      addrBlkText += ", Usage: ";
      addrBlkText += SvdTypes::GetUsage(usage);

      addrBlkText += " (Line: ";
      addrBlkText += to_string(addrBlock->GetLineNumber());
      addrBlkText += ")";

      continue;
    }

    uint32_t max = offs + size -1;

    if(usage == SvdTypes::AddrBlockUsage::REGISTERS && regOffs >= offs && regMax <= max) {
      found = true;
      break;
    }

    if(!addrBlkText.empty()) {
      addrBlkText += "\n";
    }

    addrBlkText += "    ";
    addrBlkText += to_string(i++);
    addrBlkText += "  : ";
    addrBlkText += addrBlock->IsMerged()? 'M':' ';
    addrBlkText += " [";
    addrBlkText += SvdUtils::CreateHexNum(max, 8);
    addrBlkText += " ... ";
    addrBlkText += SvdUtils::CreateHexNum(offs, 8);
    addrBlkText += "] Offs: ";
    addrBlkText += SvdUtils::CreateHexNum(offs, 4);
    addrBlkText += ", Size: ";
    addrBlkText += SvdUtils::CreateHexNum(size, 4);
    addrBlkText += ", Usage: ";
    addrBlkText += SvdTypes::GetUsage(usage);
    addrBlkText += " (Line: ";
    addrBlkText += to_string(addrBlock->GetLineNumber());
    addrBlkText += ")";
  }

  if(!found) {
    const auto periname = GetNameCalculated();
    const auto lineNo = reg->GetLineNumber();
    string t = "    Reg:   ";
    t += "[";
    t += SvdUtils::CreateHexNum(regMax, 8);
    t += " ... ";
    t += SvdUtils::CreateHexNum(regOffs, 8);
    t += "] Offs: ";
    t += SvdUtils::CreateHexNum(regOffs, 4);
    t += ", Size: ";
    t += SvdUtils::CreateHexNum(regWidth, 4);
    t += "\n";
    t += addrBlkText;

    const auto name = reg->GetNameCalculated();
    LogMsg("M344", NAME(name), ADDRSIZE(regOffs, regWidth), NAME2(periname), TXT(t), lineNo);
  }

  return true;
}

/*** Memory Check:
 *
 * Registers are checked seperately for each level
 * Clusters are only checked against clusters
 *
 ***/
bool SvdPeripheral::CheckClusterRegisters(const list<SvdItem*> &childs)
{
  const auto& addrBlocks = GetAddressBlock();
  const auto& periName = GetName();

  map<string, SvdItem*>            regsMap;
  map<string, SvdItem*>            regsMap_displayName;
  map<uint32_t, list<SvdRegister*> > readMap;
  map<uint32_t, list<SvdRegister*> > writeMap;
  map<uint32_t, list<SvdRegister*> > readWriteMap;
  map<uint32_t, list<SvdCluster*>  > clustMap;
  map<uint64_t, list<SvdItem*>     > allMap;

  for(const auto item : childs) {
    if(!item) {
      continue;
    }

    const auto clust = dynamic_cast<SvdCluster*>(item);
    if(clust) {
      const auto& clustChilds = clust->GetChildren();
      if(!clustChilds.empty()) {
        CheckClusterRegisters(clustChilds);
      }

      // Check cluster dim names on the same level against other clusters / registers
      const auto dim = clust->GetDimension();
      if(dim) {
        const auto& dimChilds = dim->GetChildren();
        if(!dimChilds.empty()) {
          for(const auto dimChild : dimChilds) {
            if(!dimChild) {
              continue;
            }

            const auto clst = dynamic_cast<SvdCluster*>(dimChild);
            if(clst) {
              AddToMap(clst, regsMap);                             // Also check Cluster Names
              AddToMap_DisplayName(clst, regsMap_displayName);     // Also check Cluster Names
            }
          }
        }
        continue;
      }

      AddToMap(clust, regsMap);                             // Also check Cluster Names
      AddToMap_DisplayName(clust, regsMap_displayName);     // Also check Cluster Names
      AddToMap(clust, clustMap);
      continue;
    }

    const auto reg = dynamic_cast<SvdRegister*>(item);
    if(!reg || !reg->IsValid()) {
      continue;
    }

    const auto dim = reg->GetDimension();
    if(dim) {
      const auto& dimChilds = dim->GetChildren();
      if(!dimChilds.empty()) {
        CheckClusterRegisters(dimChilds);
      }
      continue;
    }

    // Test for Name conflict
    AddToMap(item, regsMap);
    AddToMap_DisplayName(item, regsMap_displayName);

    const string regName = reg->GetNameCalculated();
    if(regName.empty()) {     // register has no name
      continue;
    }

    const auto lineNo = reg->GetLineNumber();

    const auto pos = regName.find_first_of("_");
    if(pos != string::npos) {
      if(regName.compare(0, pos, periName) == 0) {
        LogMsg("M303", NAME(regName), NAME2(periName), lineNo);
      }
    }

    CheckRegisterAddress(reg, addrBlocks);
    AddToMap(reg, allMap);

    //AddToMap(reg, readWriteMap);
    // R, W, RW check, when no collision <alternate> is not needed
    const auto accType = reg->GetEffectiveAccess();
    switch(accType) {
      case SvdTypes::Access::READONLY:
        AddToMap(reg, readMap, writeMap);
        break;

      case SvdTypes::Access::WRITEONLY:
      case SvdTypes::Access::WRITEONCE:
        AddToMap(reg, writeMap, readMap);
        break;

      case SvdTypes::Access::READWRITE:
      case SvdTypes::Access::READWRITEONCE: {
        AddToMap(reg, readMap, writeMap);
        AddToMap(reg, writeMap, readMap);
      } break;

      case SvdTypes::Access::UNDEF:
      default:
        break;
    }
  }

  return true;
}

bool SvdPeripheral::CheckRegisters(const list<SvdItem*> &childs)
{
  const auto& addrBlocks = GetAddressBlock();
  const auto& periName = GetName();

  for(const auto item : childs) {
    if(!item) {
      continue;
    }

    const auto clust = dynamic_cast<SvdCluster*>(item);
    if(clust) {
      const auto& clustChilds = clust->GetChildren();
      if(!clustChilds.empty()) {
        CheckClusterRegisters(clustChilds);
      }

      // Check cluster dim names on the same level against other clusters / registers
      const auto dim = clust->GetDimension();
      if(dim) {
        const auto& dimChilds = dim->GetChildren();
        if(!dimChilds.empty()) {
          for(const auto dimChild : dimChilds) {
            if(!dimChild) {
              continue;
            }

            const auto clst = dynamic_cast<SvdCluster*>(dimChild);
            if(clst) {
              AddToMap(clst, m_regsMap);                             // Also check Cluster Names
              AddToMap_DisplayName(clst, m_regsMap_displayName);     // Also check Cluster Names
            }
          }
        }
        continue;
      }

      AddToMap(item, m_regsMap);                             // Also check Cluster Names
      AddToMap_DisplayName(item, m_regsMap_displayName);     // Also check Cluster Names

      AddToMap(clust, m_clustMap);
      AddToMap(clust, m_allMap);
      continue;
    }

    const auto reg = dynamic_cast<SvdRegister*>(item);
    if(!reg || !reg->IsValid()) {
      continue;
    }

    const auto dim = reg->GetDimension();
    if(dim) {
      const auto& dimChilds = dim->GetChildren();
      if(!dimChilds.empty()) {
        CheckRegisters(dimChilds);
      }
      continue;
    }

    // Test for Name conflict
    AddToMap(item, m_regsMap);
    AddToMap_DisplayName(item, m_regsMap_displayName);

    const auto regName = reg->GetNameCalculated();
    if(regName.empty())     // register has no name
      continue;

    const auto lineNo = reg->GetLineNumber();

    const auto pos = regName.find_first_of("_");
    if(pos != string::npos) {
      if(regName.compare(0, pos, periName) == 0) {
        LogMsg("M303", NAME(regName), NAME2(periName), lineNo);
      }
    }

    CheckRegisterAddress(reg, addrBlocks);
    AddToMap(reg, m_allMap);

    //AddToMap(reg, m_readWriteMap);
    // R, W, RW check, when no collision <alternate> is not needed
    const auto accType = reg->GetEffectiveAccess();
    switch(accType) {
      case SvdTypes::Access::READONLY:
        AddToMap(reg, m_readMap, m_writeMap);
        break;

      case SvdTypes::Access::WRITEONLY:
      case SvdTypes::Access::WRITEONCE:
        AddToMap(reg, m_writeMap, m_readMap);
        break;

      case SvdTypes::Access::READWRITE:
      case SvdTypes::Access::READWRITEONCE: {
        AddToMap(reg, m_readMap, m_writeMap);
        AddToMap(reg, m_writeMap, m_readMap);
      } break;

      case SvdTypes::Access::UNDEF:
      default:
        break;
    }
  }

  return true;
}

bool SvdPeripheral::CheckAddressBlockAddrSpace(SvdAddressBlock* addrBlock)
{
  const auto name = GetNameCalculated();
  const auto lineNo = addrBlock->GetLineNumber();

  const uint64_t periBaseAddr   = GetAddress();
  const uint64_t addrBlockStart = addrBlock->GetOffset();
  const uint64_t addrBlockEnd   = addrBlockStart + addrBlock->GetSize() -1;

  if((periBaseAddr + addrBlockStart > (uint64_t)(1ULL << 32ULL) -1) || (periBaseAddr + addrBlockEnd > (uint64_t)(1ULL << 32ULL) -1)) {
    string t = "[";
    t += SvdUtils::CreateHexNum(addrBlockEnd);
    t += " ... ";
    t += SvdUtils::CreateHexNum(addrBlockStart);
    t += "]";
    LogMsg("M380", NAME(name), ADDR((uint32_t)periBaseAddr), TXT(t), lineNo);
    addrBlock->Invalidate();
  }

  return true;
}

bool SvdPeripheral::CheckAddressBlockOverlap(SvdAddressBlock* addrBlock)
{
  const auto name = GetNameCalculated();
  const auto lineNo = addrBlock->GetLineNumber();
  const auto addrBlockStart = addrBlock->GetOffset();
  const auto addrBlockEnd   = addrBlockStart + addrBlock->GetSize() -1;

  const auto& addrBlocksTest = GetAddressBlock();
  for(const auto addrBlockTest : addrBlocksTest) {
    if(!addrBlockTest || !addrBlockTest->IsValid()) {
      continue;
    }

    if(addrBlock == addrBlockTest) {
      continue;   // same block
    }

    const auto addrBlockStartTest = (uint32_t)addrBlockTest->GetOffset();
    const auto addrBlockEndTest   = addrBlockStartTest + addrBlockTest->GetSize() -1;

    if((addrBlockStart >= addrBlockStartTest && addrBlockStart <= addrBlockEndTest) ||
        (addrBlockEnd   >= addrBlockStartTest && addrBlockEnd   <= addrBlockEndTest) )
    {
      // "AddressBlock of Peripheral '%NAME%' %TEXT% overlaps addressBlock %TEXT2% in same peripheral (Line: %LINE%)."
      const auto ln = addrBlockTest->GetLineNumber();
      string t = "[";
      t += SvdUtils::CreateHexNum(addrBlockEnd, 8);
      t += " ... ";
      t += SvdUtils::CreateHexNum(addrBlockStart, 8);
      t += "]";

      string tTest = "[";
      tTest += SvdUtils::CreateHexNum(addrBlockEndTest, 8);
      tTest += " ... ";
      tTest += SvdUtils::CreateHexNum(addrBlockStartTest, 8);
      tTest += "]";
      LogMsg("M358", NAME(name), TXT(t), TXT2(tTest), LINE2(ln), lineNo);
      //addrBlock->Invalidate();    // 20.01.2016: allow overlapping addressBlock for compatibillity to SVDConv V2
    }
  }

  return true;
}

bool SvdPeripheral::SortAddressBlocks(map<uint64_t, SvdAddressBlock*>& addrBlocksSort)
{
  const auto& addrBlocks = GetAddressBlock();
  for(const auto addrBlock : addrBlocks) {
    if(!addrBlock || !addrBlock->IsValid()) {
      continue;
    }
    if(addrBlock->GetUsage() != SvdTypes::AddrBlockUsage::REGISTERS) {
      continue;       // only use "register" addressBlocks
    }

    const auto block = new SvdAddressBlock(this);
    block->CopyItem(addrBlock);

    const auto offs = block->GetOffset();
    addrBlocksSort[offs] = block;
  }

  return true;
}

bool SvdPeripheral::CopyMergedAddressBlocks(map<uint64_t, SvdAddressBlock*>& addrBlocksSort)
{
  for(const auto [key, addrBlock] : addrBlocksSort) {
    if(addrBlock->IsMerged()) {
      AddAddressBlock(addrBlock);
    }
    else {
      delete addrBlock;
    }
  }

  return true;
}

bool SvdPeripheral::MergeAddressBlocks()
{
  map<uint64_t, SvdAddressBlock*> addrBlocksSort;
  SvdAddressBlock* prev = 0;

  SortAddressBlocks(addrBlocksSort);

  for(const auto [key, actual] : addrBlocksSort) {
    if(prev) {
      const auto startPrev   = prev->GetOffset();
      const auto endPrev     = startPrev + prev->GetSize() -1;
      const auto startActual = actual->GetOffset();

      if(endPrev +1 == startActual) {
        const auto sizePrev   = prev->GetSize();
        const auto sizeActual = actual->GetSize();

        prev->SetSize(sizePrev + sizeActual);
        prev->SetMerged();
        continue;
      }
    }

    prev = actual;
  }

  CopyMergedAddressBlocks(addrBlocksSort);

  return true;
}

bool SvdPeripheral::CheckAddressBlocks()
{
  bool hasAddedBlocks = false;      // Detect if a derived Peri has new AddressBlocks (do not checked the derived ones)

  auto& addrBlocks = GetAddressBlock();
  for(const auto addrBlock : addrBlocks) {
    if(!addrBlock || !addrBlock->IsValid()) {
      continue;
    }
    if(addrBlock->IsCopied()) {
      continue;
    }

    hasAddedBlocks = true;
  }

  if(hasAddedBlocks) {
    auto it = addrBlocks.begin();
    while(it != addrBlocks.end()) {
      const auto addrBlock = *it;
      if(!addrBlock) {
        continue;
      }

      if(addrBlock->IsCopied() || addrBlock->IsMerged()) {
        delete addrBlock;
        it = addrBlocks.erase(it);
      }
      else {
        it++;
      }
    }
  }

  for(const auto addrBlock : addrBlocks) {
    if(!addrBlock || !addrBlock->IsValid()) {
      continue;
    }
    if(addrBlock->IsCopied()) {
      continue;
    }

    CheckAddressBlockOverlap(addrBlock);
    CheckAddressBlockAddrSpace(addrBlock);
  }

  MergeAddressBlocks();

  return true;
}

bool SvdPeripheral::AddToMap(SvdEnum *enu, map<string, SvdEnum*> &map)
{
  const auto name = enu->GetNameCalculated();
  const auto lineNo = enu->GetLineNumber();

  const auto e = map[name];
  if(e) {
    auto nameComplete = enu->GetParent()->GetNameCalculated();
    if(!nameComplete.empty()) {
      nameComplete += ":";
    }
    nameComplete += name;

    LogMsg("M337", LEVEL("Enumerated Value"), NAME(nameComplete), LINE2(e->GetLineNumber()), lineNo);    // M545 as WARNING
    enu->Invalidate();
  }
  else {
    map[name] = enu;
  }

  return true;
}

bool SvdPeripheral::CheckEnumeratedValues()
{
  if(!m_enumContainer || !m_enumContainer->IsValid()) {
    return true;
  }

  const auto dim = GetDimension();
  if(!dim) {
    const auto name = m_enumContainer->GetNameCalculated();
    LogMsg("M229", NAME(name), m_enumContainer->GetLineNumber());
    return true;
  }

  const auto exp = dim->GetExpression();
  if(exp && exp->GetType()!= SvdTypes::Expression::ARRAY) {
    const auto name = m_enumContainer->GetNameCalculated();
    LogMsg("M229", NAME(name), m_enumContainer->GetLineNumber());
  }

  const auto dimElements = dim->GetDim();
  map<string, SvdEnum*> enumMap;

  const auto& childs = m_enumContainer->GetChildren();
  for(const auto child : childs) {
    const auto enu = dynamic_cast<SvdEnum*>(child);
    if(!enu || !enu->IsValid()) {
      continue;
    }

    const auto& enumName = enu->GetName();
    const auto enumValue = (uint32_t) enu->GetValue().u32;

    if(enumValue >= dimElements) {
      const auto regName = GetNameCalculated();
      const auto& svdLevelStr = GetSvdLevelStr(GetSvdLevel());
      auto nameComplete = enu->GetParent()->GetNameCalculated();
      if(!nameComplete.empty()) {
        nameComplete += ":";
      }
      nameComplete += enumName;
      LogMsg("M230", NAME(enumName), NUM(enumValue), LEVEL(svdLevelStr), NAME2(regName), NUM2(dimElements), enu->GetLineNumber());
      enu->Invalidate();
    }

    AddToMap(enu, enumMap);
  }

  return true;
}

bool SvdPeripheral::CalcDisableCondition()
{
  const auto disableCondition = GetDisableCondition();
  if(!disableCondition) {
    return true;
  }

  disableCondition->CalcExpression(this);

  return true;
}

bool SvdPeripheral::CheckItem()
{
  if(!IsValid()) {
    return true;
  }

  CheckEnumeratedValues();

  const auto& name = GetNameCalculated();
  auto lineNo = GetLineNumber();
  uint32_t childNum = 0;
  const auto regCont = GetRegisterContainer();
  if(regCont) {
    childNum = regCont->GetChildCount();
  }

  if(!childNum) {
    LogMsg("M328", LEVEL("Peripheral"), NAME(name), lineNo);
    Invalidate();
  }
  else if(childNum == 1) {
    const auto& childs = regCont->GetChildren();
    const auto clust = dynamic_cast<SvdCluster*>(*childs.begin());
    if(!clust) {
      LogMsg("M332", LEVEL("Peripheral"), NAME(name), lineNo);
    }
  }

  if(!GetAddressValid()) {
    LogMsg("M378", LEVEL("Peripheral"), NAME(name), lineNo);
  }

  const auto& addrBlocks = GetAddressBlock();
  if(addrBlocks.empty()) {
    LogMsg("M312", NAME(name), lineNo);
  }

  const auto dim = GetDimension();

  const auto& headerStructname = GetHeaderStructName();
  if(!headerStructname.empty()) {
    if(headerStructname.compare(name) == 0) {
      LogMsg("M318", LEVEL("Peripheral"), TAG("headerStructName"), NAME(name), lineNo);
    }

    if(headerStructname.find("%") != string::npos) {
      LogMsg("M232", TAG("headerStructName"), NAME(headerStructname), VAL("CHAR", "%"), lineNo);
      SetHeaderStructName("");
    }
  }

  const auto enumCont = GetEnumContainer();
  if(enumCont) {
    if(dim) {
      const auto expr = dim->GetExpression();
      if(expr) {
        const auto exprType = expr->GetType();
        if(exprType != SvdTypes::Expression::ARRAY) {
          LogMsg("M243", THISLEVEL(), NAME(name), lineNo);    // no array
        }
      }
    }
    else {
      lineNo = enumCont->GetLineNumber();
      LogMsg("M242", THISLEVEL(), NAME(name), lineNo);        // no dim found
    }
  }

  const auto& alternate = GetAlternate();
  if(!alternate.empty()) {
    if(name.compare(alternate) == 0) {
      const auto& svdLevelStr = GetSvdLevelStr(GetSvdLevel());
      LogMsg("M349", LEVEL(svdLevelStr), NAME(alternate), NAME2(name), lineNo);
    }
  }

  const auto& groupName = GetGroupName();
  if(!groupName.empty()) {
    if(name.compare(groupName) == 0) {
      LogMsg("M351", TYP("group name"), NAME(groupName), lineNo);
    }
    if(groupName[groupName.length()-1] == '_') {
      LogMsg("M353", NAME(groupName), lineNo);

      string gName = groupName;
      gName.erase(groupName.length()-1);
      if(name.compare(gName) == 0) {
        LogMsg("M351", TYP("group name"), NAME(groupName), lineNo);
      }
    }
  }

  const auto& prepend = GetPrependToName();
  if(!prepend.empty()) {
    if(name.compare(prepend) == 0) {
      LogMsg("M351", TYP("prepend"), NAME(prepend), lineNo);
    }
    if(prepend[prepend.length()-1] == '_') {
      string pre = prepend;
      pre.erase(pre.length()-1);
      if(name.compare(pre) == 0) {
        LogMsg("M351", TYP("prepend"), NAME(prepend), lineNo);
      }
    }
  }

  CheckAddressBlocks();

  const auto regs = GetRegisterContainer();
  if(regs) {
    const auto& childs = regs->GetChildren();
    CheckRegisters(childs);
  }

  return SvdItem::CheckItem();
}
