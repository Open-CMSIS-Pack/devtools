/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "SvdItem.h"
#include "SvdDevice.h"
#include "SvdPeripheral.h"
#include "SvdCluster.h"
#include "SvdCpu.h"
#include "SvdUtils.h"
#include "SvdInterrupt.h"
#include "SvdDimension.h"
#include "XMLTree.h"
#include "ErrLog.h"
#include "SvdModel.h"
#include "SvdRegister.h"
#include "SvdField.h"
#include "SvdEnum.h"
#include "SvdAddressBlock.h"

using namespace std;


SvdDevice::SvdDevice(SvdItem* parent):
  SvdItem(parent),
  m_cpu(nullptr),
  m_hasAnnonUnions(false),
  m_addressUnitBits(0),
  m_width(0),
  m_resetValue(0),
  m_resetMask(0),
  m_access(SvdTypes::Access::UNDEF)
{
  SetSvdLevel(L_Device);
  m_interruptList.clear();
  m_clusterList.clear();
}

SvdDevice::~SvdDevice()
{
  //delete m_peripheralContainer;   // deleted in children
  delete m_cpu;
}

bool SvdDevice::Construct(XMLTreeElement* xmlElement)
{
  // we are not interested in XML attributes for package
	m_fileName = xmlElement->GetRootFileName();
  AddAttribute("schemaVersion", xmlElement->GetAttribute("schemaVersion"), false);

  return SvdItem::Construct(xmlElement);
}

bool SvdDevice::ProcessXmlElement(XMLTreeElement* xmlElement)
{
	const auto& tag   = xmlElement->GetTag();
	const auto& value = xmlElement->GetText();

  if(tag == "vendor") {
    m_vendor = value;
    return true;
  }
  else if(tag == "vendorID") {
    m_vendorId = value;
    return true;
  }
  else if(tag == "series") {
    m_series = value;
    return true;
  }
  else if(tag == "version") {
    m_version = value;
    return true;
  }
  else if(tag == "licenseText") {
    m_licenseText = value;
    return true;
  }
  else if(tag == "headerSystemFilename") {
    m_headerSystemFilename = value;
    return true;
  }
  else if(tag == "headerDefinitionsPrefix") {
    m_headerDefinitionsPrefix = value;
    return true;
  }
  else if(tag == "addressUnitBits") {
    if(!SvdUtils::ConvertNumber(value, m_addressUnitBits)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    return true;
  }
  else if(tag == "width") {
    if(!SvdUtils::ConvertNumber(value, m_width)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    return true;
  }
  else if(tag == "size") {
    uint32_t num = 0;
    if(!SvdUtils::ConvertNumber(value, num)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    SetBitWidth(num);
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
  else if(tag == "peripherals") {
    auto peripheralContainer = GetPeripheralContainer();
    if(!peripheralContainer) {
      peripheralContainer = new SvdPeripheralContainer(this);
      AddItem(peripheralContainer);
    }
		return peripheralContainer->Construct(xmlElement);
	}
  else if(tag == "vendorExtensions") {
    return true;      // ignore
  }
  else if(tag == "cpu") {
    if(!m_cpu) {
      m_cpu = new SvdCpu(this);
    }
		return m_cpu->Construct(xmlElement);
	}

  return SvdItem::ProcessXmlElement(xmlElement);
}

bool SvdDevice::ProcessXmlAttributes(XMLTreeElement* xmlElement)
{
  const auto& attributes = xmlElement->GetAttributes();
  if(!attributes.empty()) {
    for(const auto& [tag, value] : attributes) {
      if(tag == "schemaVersion") {
        SetSchemaVersion(value);
      }
    }
  }

  return true;
}

SvdDevice* SvdDevice::GetDevice() const
{
  return const_cast<SvdDevice*>(this);
}

SvdPeripheralContainer* SvdDevice::GetPeripheralContainer() const
{
  if(!GetChildCount()) {
    return nullptr;
  }

  auto cont = dynamic_cast<SvdPeripheralContainer*>(*(GetChildren().begin()));

  return cont;
}

bool SvdDevice::Calculate()
{
  CreateInterrupts();
  CreateClusters();
  CreatePeripheralTypes();

  return SvdItem::Calculate();
}

bool SvdDevice::AddInterrupt(SvdInterrupt* interrupt)
{
  if(!interrupt) {
    return false;
  }

  const auto num = interrupt->GetValue();
  const auto& name = interrupt->GetName();

  for(const auto [key, irq] : m_interruptList) {
    if(!irq) {
      continue;
    }

    if(irq->GetName() == name && irq->GetValue() != num) {
      LogMsg("M336", LEVEL("Interrupt Name"), NAME(name), LINE2(irq->GetLineNumber()), interrupt->GetLineNumber());
      return false;
    }
  }

  if(!m_interruptList[num]) {
    m_interruptList[num] = interrupt;
  }
  else {
    auto irq = m_interruptList[num];

    if(name != irq->GetName() || num != irq->GetValue()) {
      LogMsg("M301", NUM(num), NAME(interrupt->GetName()), NAME2(m_interruptList[num]->GetName()), LINE2(m_interruptList[num]->GetLineNumber()), interrupt->GetLineNumber());
    }
    else {
      LogMsg("M304", NUM(num), NAME(interrupt->GetName()), LINE2(m_interruptList[num]->GetLineNumber()), interrupt->GetLineNumber());
    }
  }

  return true;
}

bool SvdDevice::CreateInterrupts()
{
  const auto peripheralCont = GetPeripheralContainer();
  if(!peripheralCont) {
    return false;
  }

  const auto& childs = peripheralCont->GetChildren();
  if(childs.empty()) {
    return false;
  }

  m_interruptList.clear();

  for(const auto child : childs) {
    auto peri = dynamic_cast<SvdPeripheral*>(child);
    if(!peri || !peri->IsValid()) {
      continue;
    }

    const auto& interrupts = peri->GetInterrupt();
    if(interrupts.empty()) {
      continue;     // no interrupts in this peripheral
    }

    for(const auto interrupt : interrupts) {
      if(!interrupt || !interrupt->IsValid()) {
        continue;
      }

      const auto dim = interrupt->GetDimension();
      if(dim) {
        const auto& irqChilds = dim->GetChildren();
        for(const auto irqChild : irqChilds) {
          auto irq = dynamic_cast<SvdInterrupt*>(irqChild);
          if(!irq || !irq->IsValid()) {
            continue;
          }

          const auto& descr = irq->GetDescription();
          if(descr.empty()) {
            const auto& name = irq->GetName();
            irq->SetDescription(name);
          }

          AddInterrupt(irq);
        }
      }
      else {
        const auto& descr = interrupt->GetDescription();
        if(descr.empty()) {
          const auto& name = interrupt->GetName();
          interrupt->SetDescription(name);
        }

        AddInterrupt(interrupt);
      }
    }
  }

  return true;
}

bool SvdDevice::CreatePeripheralTypes()
{
  const auto peripheralCont = GetPeripheralContainer();
  if(!peripheralCont) {
    return false;
  }

  const auto& childs = peripheralCont->GetChildren();
  if(childs.empty()) {
    return false;
  }

  for(const auto child : childs) {
    SvdPeripheral *peri = dynamic_cast<SvdPeripheral*>(child);
    if(!peri || !peri->IsValid()) {
      continue;
    }

    if(!peri->IsModified()) {
        continue;
    }

    m_peripheralList.push_back(peri);
  }

  return true;
}

bool SvdDevice::CreateClusters()
{
  const auto peripheralCont = GetPeripheralContainer();
  if(!peripheralCont) {
    return false;
  }

  const auto& childs = peripheralCont->GetChildren();
  if(childs.empty()) {
    return false;
  }

  for(const auto child : childs) {
    SvdPeripheral *peri = dynamic_cast<SvdPeripheral*>(child);
    if(!peri || !peri->IsValid()) {
      continue;
    }

    GatherClusters(peri);
  }

  return true;
}

bool SvdDevice::GatherClusters(SvdItem *item)
{
  const auto& childs = item->GetChildren();

  for(const auto child : childs) {
    GatherClusters(child);
    const auto cluster = dynamic_cast<SvdCluster*>(child);
    if(cluster) {
      if(!cluster->IsModified()) {
        continue;
      }

#if 0     // enable if clusters with "%s" only should be generated in single instances
      const auto& n = cluster->GetName();
      if(n == "%s") {
        const auto dim = cluster->GetDimension();
        if(dim) {
          const auto exprType = dim->GetExpression()->GetType();
          if(exprType == SvdTypes::Expression::ARRAY) {
            LogMsg("M000", cluster->GetLineNumber());   // nameless cluster cannot be an array
            cluster->Invalidate();
          }
          else if(exprType == SvdTypes::Expression::EXTEND) {
            GatherClusters(dim);
          }
          else {
            LogMsg("M000", cluster->GetLineNumber());   // cannot generate nameless cluster
            cluster->Invalidate();
          }
        }
        continue;
      }
#endif

      const auto name = cluster->GetHeaderTypeNameHierarchical();//GetHeaderTypeName();

      m_clusterList.push_back(cluster);    // build reverse sorted list to keep dependencies cluster2cluster
    }
  }

  return true;
}

bool SvdDevice::AddToMap(SvdItem *item, const string& name, const string& tagUsed, map<string, SvdItem*>& map)
{
  const auto& mapItem = map[name];
  if(mapItem) {
    auto orig = (SvdItem*)item->GetDerivedFrom();
    if(!orig) {
      orig = item->GetCopiedFrom();
    }

    if(orig && orig == mapItem) {
      //int ctrap = 0;
    } else if(mapItem == item) {
      //int ctrap = 0;
    }
    else {
      const auto lineNo = item->GetLineNumber();
      const string& svdLevelStr = GetSvdLevelStr(item->GetSvdLevel());
      LogMsg("M372", LEVEL(svdLevelStr), TAG(tagUsed), NAME(name), LINE2(mapItem->GetLineNumber()), lineNo);      // Error
      item->Invalidate();
      return false;
    }
  }
  else {
    map[name] = item;
  }

  return true;
}

bool SvdDevice::AddToMap(SvdCluster *clust)
{
  // Cluster HeaderStructName must be always unique
  const auto& headerStructName = clust->GetHeaderStructName();
  if(!headerStructName.empty()) {
    AddToMap(clust, headerStructName, "headerStructName", m_perisHeaderStructMap);
  }

  const auto name = clust->GetHierarchicalName();
  if(!name.empty()) {
    AddToMap(clust, name, "name", m_perisMap);

    if(headerStructName.empty()) {
      AddToMap(clust, name, "name", m_perisHeaderStructMap);
    }
  }

  const auto enumCont = clust->GetEnumContainer();
  if(enumCont) {
    AddToMap(enumCont);
  }

  return true;
}

bool SvdDevice::AddToMap(SvdPeripheral *peri)
{
  // Peripheral Name must always be unique
  const auto name = peri->GetNameCalculated();
  if(!name.empty()) {
    AddToMap(peri, name, "name", m_perisMap);
  }

  // Peripheral HeaderStructName must be always unique
  const auto& headerStructName = peri->GetHeaderStructName();
  if(!headerStructName.empty()) {
    AddToMap(peri, headerStructName, "headerStructName", m_perisHeaderStructMap);

    if(headerStructName.empty()) {
      AddToMap(peri, name, "name", m_perisHeaderStructMap);
    }
  }

  const auto enumCont = peri->GetEnumContainer();
  if(enumCont) {
    AddToMap(enumCont);
  }

  return true;
}

bool SvdDevice::AddToMap(SvdItem *item)
{
  auto peri = dynamic_cast<SvdPeripheral*>(item);
  if(peri) {
    AddToMap(peri);
  }

  auto clust = dynamic_cast<SvdCluster*>(item);
  if(clust) {
    AddToMap(clust);
  }

  return true;
}

bool SvdDevice::AddToMap(SvdEnumContainer* enumCont)
{
  const auto& headerEnumName = enumCont->GetHeaderEnumName();
  if(!headerEnumName.empty()) {
    AddToMap(enumCont, headerEnumName, "headerEnumName", m_perisHeaderEnumMap);
  }

  return true;
}

bool SvdDevice::CheckEnumContainerNames(SvdRegister* reg)
{
  const auto fieldCont = reg->GetFieldContainer();
  if(!fieldCont || !fieldCont->IsValid()) {
    return true;
  }

  const auto& childs = fieldCont->GetChildren();
  for(const auto child : childs) {
    const auto field = dynamic_cast<const SvdField*>(child);
    if(!field || !field->IsValid()) {
      continue;
    }

    const auto& enumConts = field->GetEnumContainer();
    if(enumConts.empty()) {
      continue;
    }

    for(const auto item : enumConts) {
      const auto enumCont = dynamic_cast<SvdEnumContainer*>(item);
      if(!enumCont || !enumCont->IsValid()) {
        continue;
      }

      AddToMap(enumCont);
    }
  }

  return true;
}

bool SvdDevice::AddClusterNames(const list<SvdItem*>& childs)
{
  for(const auto child : childs) {
    const auto clust = dynamic_cast<SvdCluster*>(child);
    if(clust && clust->IsValid()) {
      const auto& clustChilds = clust->GetChildren();
      if(!clustChilds.empty()) {
        AddClusterNames(clustChilds);
      }

      AddToMap(clust);
    }

    // Check Enum <headerEnumName> against global namespace
    const auto reg = dynamic_cast<SvdRegister*>(child);
    if(reg && reg->IsValid()) {
      CheckEnumContainerNames(reg);
    }
  }

  return true;
}

bool SvdDevice::AddToMap(SvdPeripheral* peri, map<uint32_t, list<SvdPeripheral*> > &map, bool bSilent /*= 0*/)
{
  const auto name = peri->GetNameCalculated();
  if(name.empty()) {
    return true;
  }

  const auto lineNo = peri->GetLineNumber();
  const string& altPeriName = peri->GetAlternate();

  const auto addr = (uint32_t) peri->GetAbsoluteAddress();
  if(addr % 4) {
    LogMsg("M350", NAME(name), ADDR(addr), lineNo);
  }

  bool ok = true;
  const auto& mapItem = map[addr];
  if(!mapItem.empty()) {
    ok = false;
    if(!altPeriName.empty()) {
      for(const auto p : mapItem) {
        if(!p) {
          continue;
        }

        const string nam = p->GetNameCalculated();
        if(!altPeriName.compare(nam)) {
          ok = true;
          break;
        }
      }
    }

    if(!bSilent && !ok) {                           // report first register on this address
      const auto p = *mapItem.begin();
      const auto nam = p->GetNameCalculated();

      if(!altPeriName.empty()) {
        LogMsg("M348", LEVEL("Peripheral"), NAME(altPeriName), ADDR(addr), lineNo);
      }

      LogMsg("M343", LEVEL("Peripheral"), NAME(name), ADDR(addr), NAME2(nam), LINE2(p->GetLineNumber()), lineNo);
    }
  }

  map[addr].push_back(peri);

  return ok;
}

bool SvdDevice::CheckPeripherals(const list<SvdItem*>& childs)
{
  for(const auto child : childs) {
    const auto peri = dynamic_cast<SvdPeripheral*>(child);
    if(peri && peri->IsValid()) {
      if(peri->GetHasAnnonUnions()) {
        SetHasAnnonUnions();
      }

      const auto dim = peri->GetDimension();
      if(dim) {
        const auto expr = dim->GetExpression();
        if(expr) {
          const auto exprType = expr->GetType();
          if(exprType == SvdTypes::Expression::EXTEND) {
            const auto name = peri->GetNameCalculated();
            const auto lineNo = peri->GetLineNumber();
            LogMsg("M214", NAME(name), lineNo);
            peri->Invalidate();
          }
          else {
            AddToMap(peri);
            AddToMap(peri, m_perisBaseAddrMap);
          }
        }
      }
      else {
        AddToMap(peri);
        AddToMap(peri, m_perisBaseAddrMap);
      }

      // ClusterNames must be unique to peripherals
      const auto regs = peri->GetRegisterContainer();
      if(regs) {
        const auto& children = regs->GetChildren();
        AddClusterNames(children);
      }
    }
  }

  return true;
}

bool SvdDevice::CheckInterrupts(const map<uint32_t, SvdInterrupt*>& interrupts)
{
  if(interrupts.empty()) {
    LogMsg("M356");
    return true;
  }

  map<string, SvdInterrupt*> nameSortedCoreIrq;

  const auto cpu = GetCpu();
  if(!cpu) {
    return true;
  }

  const auto& coreIntrerrupts = cpu->GetInterruptList();
  for(const auto [key, irq] : coreIntrerrupts) {
    if(!irq) {
      continue;
    }

    const auto& name = irq->GetName();
    nameSortedCoreIrq[name] = irq;
  }

  bool found = false;
  for(const auto [key, irq] : interrupts) {
    if(!irq) {
      continue;
    }

    const auto& name = irq->GetName();
    const auto num  = irq->GetValue();

    if(num < 16) {
      found = true;
    }

    auto irqCore = nameSortedCoreIrq.find(name);
    if(irqCore != nameSortedCoreIrq.end()) {
      const auto lineNo = irq->GetLineNumber();
      LogMsg("M354", NUM(num), NAME(name), lineNo);
      irq->Invalidate();
    }
  }

  if(!found) {
    LogMsg("M355");
  }

  return true;
}

bool SvdDevice::CheckAddressBlockOverlap(SvdPeripheral* peri, SvdAddressBlock* addrBlock, const map<string, SvdItem*>& perisMap)
{
  if(!peri || !addrBlock || !addrBlock->IsValid()) {
    return true;
  }

  const auto name = peri->GetNameCalculated();
  if(name.empty()) {
    return true;
  }

  const auto lineNo        = peri->GetLineNumber();
  uint32_t  periStart      = (uint32_t)peri->GetAbsoluteAddress();
  uint32_t  addrBlockStart = periStart      + addrBlock->GetOffset();
  uint32_t  addrBlockEnd   = addrBlockStart + addrBlock->GetSize() -1;

  // alternate name is already checked, so no error message is necessary here
  const auto& alternate = peri->GetAlternate();
  if(!alternate.empty()) {
    return true;
  }

  for(const auto& [key, item] : perisMap) {
    const auto periTest = dynamic_cast<SvdPeripheral*>(item);
    if(!periTest || !periTest->IsValid()) {
      continue;
    }
    if(periTest == peri) {
      continue;
    }

    const auto periStartTest = (uint32_t)periTest->GetAbsoluteAddress();
    const auto nameTest      = periTest->GetNameCalculated();
    const auto& altNameTest  = periTest->GetAlternate();

    if(name == altNameTest || nameTest == alternate) {      // alternate definitions
      continue;
    }

    const auto& addrBlocksTest = periTest->GetAddressBlock();
    for(const auto addrBlockTest : addrBlocksTest) {
      if(!addrBlockTest || !addrBlockTest->IsValid()) {
        continue;
      }

      uint32_t addrBlockStartTest = periStartTest      + (uint32_t)addrBlockTest->GetOffset();
      uint32_t addrBlockEndTest   = addrBlockStartTest + addrBlockTest->GetSize() -1;

      if((addrBlockStart >= addrBlockStartTest && addrBlockStart <= addrBlockEndTest) ||
         (addrBlockEnd   >= addrBlockStartTest && addrBlockEnd   <= addrBlockEndTest) )
      {
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
        LogMsg("M352", NAME(name), ADDR(periStart), TXT(t), NAME2(nameTest), ADDR2(periStartTest), TXT2(tTest), LINE2(ln), lineNo);
      }
    }
  }

  return true;
}

bool SvdDevice::CheckPeripheralOverlap(const map<string, SvdItem*>& perisMap)
{
  for(const auto& [key, item] : perisMap) {
    const auto peri = dynamic_cast<SvdPeripheral*>(item);
    if(!peri || !peri->IsValid()) {
      continue;
    }

    const auto& addrBlocks = peri->GetAddressBlock();
    for(const auto addrBlock : addrBlocks) {
      if(!addrBlock || !addrBlock->IsValid()) {
        continue;
      }

      CheckAddressBlockOverlap(peri, addrBlock, perisMap);
    }
  }

  return true;
}

bool SvdDevice::CheckForItemsRegister(SvdRegister* reg)
{

  if(!reg) {
    return true;
  }

  uint32_t itemCnt = 0;
  const auto dim = reg->GetDimension();
  if(dim) {
    const auto& dimChilds = dim->GetChildren();
    for(const auto dimChild : dimChilds) {
      const auto dimReg = dynamic_cast<SvdRegister*>(dimChild);
      if(!dimReg || !dimReg->IsValid()) {
        continue;
      }

      itemCnt += CheckForItemsRegister(dimReg);
    }
    return true;
  }

  uint32_t fieldItems = 0;
  const auto fieldCont = reg->GetFieldContainer();
  if(fieldCont) {
    const auto& fieldChilds = fieldCont->GetChildren();
    for(const auto fieldChild : fieldChilds) {
      SvdField* field = dynamic_cast<SvdField*>(fieldChild);
      if(!field || !field->IsValid()) {
        continue;
      }

      fieldItems++;
    }
  }

  if(!fieldItems || !reg->GetChildCount()) {
    reg->SetNoValidFields();
  }

  return itemCnt? true : false;
}

bool SvdDevice::CheckForItemsCluster(const list<SvdItem*> &childs)
{
  uint32_t itemCnt = 0;

  for(const auto child : childs) {
    const auto clust = dynamic_cast<SvdCluster*>(child);
    if(clust && clust->IsValid()) {
      const auto& subChilds = clust->GetChildren();
      if(CheckForItemsCluster(subChilds)) {
        itemCnt++;
      }
    }
    const auto reg = dynamic_cast<SvdRegister*>(child);
    if(reg && reg->IsValid()) {
      itemCnt++;
      CheckForItemsRegister(reg);
    }
  }

  return itemCnt? true : false;
}

bool SvdDevice::CheckForItemsPeri(const list<SvdItem*> &childs)
{
  for(const auto child : childs) {
    const auto peri = dynamic_cast<SvdPeripheral*>(child);
    if(!peri || !peri->IsValid()) {
      continue;
    }

    uint32_t itemCnt = 0;

    const auto regCont = peri->GetRegisterContainer();
    if(regCont) {
      const auto& regChilds = regCont->GetChildren();
      for(const auto regChild : regChilds) {
        const auto clust = dynamic_cast<SvdCluster*>(regChild);
        if(clust && clust->IsValid()) {
          const auto& subChilds = clust->GetChildren();
          const auto dim = clust->GetDimension();
          if(dim) {
            //if(dim->GetExpression()->GetType() == SvdTypes::Expression::EXTEND) {
              const auto& dimChilds = dim->GetChildren();
              for(const auto dimChild : dimChilds) {
                SvdCluster* dimClust = dynamic_cast<SvdCluster*>(dimChild);
                if(!dimClust || !dimClust->IsValid()) {
                  continue;
                }

                const auto& dimClustChilds = dimClust->GetChildren();
                CheckForItemsCluster(dimClustChilds);
              }
            //}
          }

          if(CheckForItemsCluster(subChilds)) {
            itemCnt++;
          }
          else {
            const auto name = clust->GetNameCalculated();
            const auto lineNo = clust->GetLineNumber();
            const auto& svdLevelStr = GetSvdLevelStr(clust->GetSvdLevel());
            LogMsg("M234", LEVEL(svdLevelStr), NAME(name), lineNo);
            clust->Invalidate();
          }
        }

        const auto reg = dynamic_cast<SvdRegister*>(regChild);
        if(reg && reg->IsValid()) {
          itemCnt++;
          CheckForItemsRegister(reg);
        }
      }
    }

    if(!itemCnt) {
      const string name = peri->GetNameCalculated();
      const auto lineNo = peri->GetLineNumber();
      const auto& svdLevelStr = GetSvdLevelStr(peri->GetSvdLevel());
      LogMsg("M234", LEVEL(svdLevelStr), NAME(name), lineNo);
      peri->Invalidate();
    }
  }

  return true;
}

bool SvdDevice::CheckItem()
{
  if(!IsValid()) {
    return true;
  }

  const auto model = dynamic_cast<SvdModel*>(GetParent());
  if(!model) {
    return false;
  }

  const auto name = GetNameCalculated();
  const auto lineNo = GetLineNumber();

  const auto& schemaVersion = GetSchemaVersion();
  if(schemaVersion.empty()) {
    LogMsg("M306");
  }

  auto inputFileName = model->GetInputFileName();
  string::size_type pos = inputFileName.find_last_of(".");
  string fileExt = inputFileName.substr(pos, inputFileName.length());
  SvdUtils::ToLower(fileExt);

  if(!fileExt.length() || pos == string::npos) {
    LogMsg("M222", NAME(inputFileName), lineNo);
  }
  else if(fileExt != ".svd") {
    LogMsg("M221", NAME(inputFileName), lineNo);
  }

  if(pos != string::npos) {
    inputFileName.erase(pos, inputFileName.length());
  }

  pos = inputFileName.find_last_of('\\');
  if(pos == string::npos) {
    pos = inputFileName.find_last_of('/');
    if(pos == string::npos) {
      pos = 0;
    }
    else {
      pos++;
    }
  }
  else {
    pos++;
  }

  inputFileName.erase(0, pos);

  if(name.compare(inputFileName) != 0) {
    LogMsg("M223", VAL("INFILE", inputFileName), NAME(name), lineNo);
  }

  m_perisMap.clear();
  m_perisHeaderStructMap.clear();

  const auto peris = GetPeripheralContainer();
  if(peris) {
   const auto& childs = peris->GetChildren();
    CheckPeripherals(childs);
  }

  // m_perisMap must be set up here (via AddToMap and CheckPeripherals
  CheckPeripheralOverlap(m_perisMap);

  // must be the last test, invalidates peripherals which have only invalidated items
  if(peris) {
    const auto& childs = peris->GetChildren();
    CheckForItemsPeri(childs);
  }

  const auto& interrupts = GetInterruptList();
  CheckInterrupts(interrupts);

  const auto cpu = GetCpu();
  if(!cpu) {
    const string& schemaVer = GetSchemaVersion();
    if(schemaVer == "1.0") {
      LogMsg("M210");
    }
    else {
      LogMsg("M209");
    }
  }

  return SvdItem::CheckItem();
}
