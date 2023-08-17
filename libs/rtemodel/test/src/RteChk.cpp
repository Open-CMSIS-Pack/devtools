/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
*/

// RteChk.cpp : RTE Model tester
//

#include "RteChk.h"
#include "XMLTreeSlim.h"
#include "RteValueAdjuster.h"
#include "RteItemBuilder.h"

#include "RteModel.h"
#include "RteFsUtils.h"

#include "ErrLog.h"

#include <time.h>
#include <map>
#include <algorithm>
using namespace std;

static clock_t clockInMsec() {
  static clock_t CLOCK_PER_MSEC = CLOCKS_PER_SEC / 1000;
  clock_t t = clock();
  if (CLOCK_PER_MSEC <= 1)
    return t;
  return (t / CLOCK_PER_MSEC);
}

class RteXmlParser : public XMLTreeSlim
{
public:
  RteXmlParser(RteItemBuilder* builder) :XMLTreeSlim(builder, true) { m_XmlValueAdjuster = new RteValueAdjuster(); }
  ~RteXmlParser() { delete m_XmlValueAdjuster; }

};

// Visitor design patter iteration through model
class RteChkErrorVistior : public RteVisitor
{
public:
  virtual VISIT_RESULT Visit(RteItem* rteItem)
  {
    if (rteItem->IsValid())
      return VISIT_RESULT::SKIP_CHILDREN;

    const list<string>& errors = rteItem->GetErrors();
    if (errors.empty())
      return VISIT_RESULT::CONTINUE_VISIT;
    list<string>::const_iterator it;
    for (it = errors.begin(); it != errors.end(); it++) {
      string sErr = (*it);
      cerr << sErr << endl;
    }
    return VISIT_RESULT::CONTINUE_VISIT;
  }
};


RteChk::RteChk(std::ostream& os) :
  m_flags(TIME|VALIDATE),
  m_npdsc(0),
  m_indent(0),
  m_os(os)
{
  m_rteModel = new RteModel();
  m_rteModel->SetUseDeviceTree(true);
  m_rteItemBuilder = new RteItemBuilder();
  m_xmlTree = new RteXmlParser(m_rteItemBuilder);
}

RteChk::~RteChk()
{
  delete m_xmlTree;
  delete m_rteModel;
  delete m_rteItemBuilder;
};

void RteChk::SetFlag(unsigned flag, char op)
{
  if (op == '+') {
    m_flags |= flag;
  } else if (op == '-') {
    m_flags &= ~flag;
  }
}

bool RteChk::IsFlagSet(unsigned flag) const
{
  return (m_flags & flag) != 0;
}

unsigned RteChk::CharToFlagValue(char ch)
{
  switch (ch) {
  case 'a':
    return ALL;
  case 'd':
    return DUMP;
  case 't':
    return TIME;
  case 'v':
    return VALIDATE;
  case 'c':
    return COMPONENTS;
  case 'D':
    return DFP;
  case 'B':
    return BSP;
  case 'p':
    return PACKS;
  default:
    break;
  };
  return NONE;
}

int RteChk::ProcessArguments(int argc, char* argv[])
{
  for (int i = 1; i < argc; i++) {
    char *ptr = argv[i];
    if (*ptr == '-' || *ptr == '+') {
      char op = *ptr;
      ptr++;
      SetFlag(CharToFlagValue(*ptr), op);
      continue;
    }
    AddFileDir(argv[i]);

    m_xmlTree->AddFileName(argv[i]);
  }

  if (m_files.empty() && m_dirs.empty()) {
    m_os << "Usage: " << endl;
    m_os << "RteChk [-t] [-d] FILE1.pdsc|DIR1 [DIR2 FILE2.pdsc ...]";
    return -1;
  }
  return 0;
}

void RteChk::AddFileDir(const string& path) {
  error_code ec;
  if (!fs::exists(path, ec))
    return;
  if (fs::is_directory(path, ec)) {
    m_dirs.insert(path);
  } else {
    m_files.insert(path);
  }
}

unsigned long RteChk::CollectFiles() {

  list<string> files;
  for (const string& dir : m_dirs) {
    RteFsUtils::GetPackageDescriptionFiles(files, dir, 3);
  }
  for (const string& file : m_files) {
    files.push_back(file);
  }

  if (!files.empty()) {
    m_xmlTree->SetFileNames(files, false);
  }
  return files.size();
}


int RteChk::RunCheckRte()
{
  m_os << "Collecting pdsc files ";
  clock_t t0 = clockInMsec();
  m_npdsc = CollectFiles();
  clock_t t1 = clockInMsec();
  if (IsFlagSet(TIME)) {
    m_os << "(" << t1 - t0 << " ms) ";
  }
  m_os << m_npdsc << " files found" << endl;
  if (!m_npdsc) {
    m_os << "Nothing to process" << endl;
    return 1;
  }

  m_xmlTree->Init();
  m_os << "Parsing XML " ;
  bool success = m_xmlTree->ParseAll();
  clock_t t2 = clockInMsec();
  if (IsFlagSet(TIME)) {
    m_os << "(" << t2 - t1 << " ms) ";
  }

  if (success)
    m_os << "passed";
  else {
    m_os << "failed";
  }
  m_os << std::endl;
  if (!success || IsFlagSet(VALIDATE)) {
    const list<string>& errors = m_xmlTree->GetErrorStrings();
    for (auto it = errors.begin(); it != errors.end(); it++) {
      m_os << *it << endl;
    }
  }

  if (!success)
    return 1;

  m_os << endl<< "Constructing Model ";

  m_rteModel->InsertPacks(m_rteItemBuilder->GetPacks());

  clock_t t3 = clockInMsec();
  if (IsFlagSet(TIME)) {
    m_os << "(" << t3 - t2 << " ms. Read+Construct: " << t3 - t1 << " ms) ";
  }

  if (success)
    m_os << "passed";
  else
    m_os << "failed";
  m_os << endl;

  m_os << endl << "Cleaning XML data";
  m_xmlTree->Clear();
  clock_t t4 = clockInMsec();
  if (IsFlagSet(TIME)) {
    m_os << " (" << t4 - t3 << " ms. Total: " << t4 - t1 << " ms)";
  }
  m_os << endl;

  m_rteModel->ClearErrors();
  if (IsFlagSet(VALIDATE)) {
    m_os << endl << "Validating Model ";
    success = m_rteModel->Validate();
    clock_t t5 = clockInMsec();
    if (IsFlagSet(TIME)) {
      m_os << "(" << t5 - t3 << " ms. Total: " << t5 - t1 << " ms) ";
    }
  }

  if (success)
    m_os << "passed";
  else
    m_os << "failed";
  m_os << std::endl;

  if (!success) {
    RteChkErrorVistior visitor;
    m_rteModel->AcceptVisitor(&visitor); // an example of "Visitor" design pattern tree iteration
  }
  if (IsFlagSet(DUMP)) {
    DumpModel(); // an example of classic iteration over model tree
  }

  // print summary information
  m_os << endl << "Summary:" << endl;
  m_os << "Packs: " << m_npdsc << endl;
  ListPacks();
  m_os << endl;
  m_os << "Components: " << m_rteModel->GetComponentCount() << endl;
  ListComponents();
  m_os << endl;
  m_os << "Devices: " << m_rteModel->GetDeviceCount() << endl;
  m_os << "Boards: " << m_rteModel->GetBoards().size() << endl;

  m_os << endl << "completed" << endl;
  ErrLog::Get()->ClearLogMessages();
  return success ? 0 : 1;
}



void RteChk::PrintText(const string& str)
{
  if (str.empty())
    return;
  for (int i = 0; i < m_indent; i++)
    m_os << " ";
  m_os << str << endl;
}


void RteChk::DumpDeviceElement(RteDeviceElement* e)
{
  if (!e)
    return;
  XmlItem rteAttributes;
  e->GetEffectiveAttributes(rteAttributes); // also inherited/overwritten
  XmlItem* pAttributes = &rteAttributes;
  const string& tag = e->GetTag();
  const string& name = e->GetName();

  string elementString = "<" + tag + ">";
  if (!e->HasAttribute("name") && tag != name) {
    elementString += "(" + name + ")";
  }
  elementString += " " + pAttributes->GetAttributesAsXmlString();
  PrintText(elementString);

  const string& descr = e->GetDescription();
  if (!descr.empty()) {
    string sDescr = "Description: ";
    sDescr += descr;
    // remove newlines
    sDescr.erase(std::remove(sDescr.begin(), sDescr.end(), '\r'), sDescr.end());
    sDescr.erase(std::remove(sDescr.begin(), sDescr.end(), '\n'), sDescr.end());

    PrintText(sDescr);
  }
}


void RteChk::DumpEffectiveContent(RteDeviceProperty* d)
{
  RteDevicePropertyGroup* pg = dynamic_cast<RteDevicePropertyGroup*>(d);
  if (!pg)
    return;

  const list<RteDeviceProperty*>& props = pg->GetEffectiveContent();
  for (auto itp = props.begin(); itp != props.end(); itp++) {
    RteDeviceProperty* p = *itp;
    DumpDeviceElement(p);
    DumpEffectiveContent(p);
  }
}


void RteChk::DumpEffectiveProperties(RteDeviceItem* d, const string& processorName, bool endLeaf)
{
  PrintText("Properties");
  m_indent++;
  RteDevicePropertyMap props;
  if (!endLeaf) {
    d->GetProperties(props);
  }
  const RteDevicePropertyMap& propMap = endLeaf ? d->GetEffectiveProperties(processorName) : props;

  for (auto it = propMap.begin(); it != propMap.end(); it++) {
    const list<RteDeviceProperty*>& props = it->second;
    for (auto itp = props.begin(); itp != props.end(); itp++) {
      RteDeviceProperty* p = *itp;
      DumpDeviceElement(p);
      if (p->GetTag() != "sequence")
        DumpEffectiveContent(p);
    }
  }
  m_indent--;
}


void RteChk::DumpDeviceItem(RteDeviceItem* d, const string& dName, bool endLeaf)
{
  m_indent++;
  DumpDeviceElement(d);
  string processorName = RteUtils::GetSuffix(dName);
  DumpEffectiveProperties(d, processorName, endLeaf);
  m_indent--;
}



void RteChk::DumpDeviceAggregate(RteDeviceItemAggregate* da)
{
  if (!da)
    return;

  if (da->GetType() > RteDeviceItem::VENDOR_LIST) {
    PrintText(da->GetName());
  }
  m_indent++;
  if (da->GetType() > RteDeviceItem::VENDOR) {
    // add device items;
    const RteDeviceItemMap& deviceItems = da->GetAllDeviceItems();
    for (auto itd = deviceItems.begin(); itd != deviceItems.end(); itd++) {
      DumpDeviceItem(itd->second, da->GetName(), da->GetChildCount() == 0);
    }
  }

  // add children
  const auto children = da->GetChildren();
  for (auto it = children.begin(); it != children.end(); it++) {
    DumpDeviceAggregate(it->second);
  }
  m_indent--;
}


void RteChk::DumpConditions(RtePackage* pack)
{
  RteItem* conditions = pack->GetConditions();
  if (!conditions)
    return;
  for (auto pCond : conditions->GetChildren() ) {
    m_os << " Condition " << pCond->GetName() << endl;
    m_os << " Desc: " << pCond->GetDescription() << endl;
    // condition expressions
    for (auto child : pCond->GetChildren()) {
      RteConditionExpression* expr = dynamic_cast<RteConditionExpression*>(child);
      if (!expr)
        continue;
      m_os << " " << expr->GetName() << " " << expr->GetAttributesAsXmlString() << endl;
    }
  }
}

void RteChk::DumpComponents(RtePackage* pack)
{
  // components
  RteItem* components = pack->GetComponents();
  if (components) {
    for (auto child : components->GetChildren()) {
      RteComponent *pComp = dynamic_cast<RteComponent*>(child);
      if (pComp == NULL) {
        continue;
      }
      PrintComponent(pComp, true);
    }
  }
}


void RteChk::DumpExamples(RtePackage* pack)
{
  // Examples
  RteItem* examples = pack->GetExamples();

  if (examples) {
    for (auto child : examples->GetChildren()) {
      RteExample *pExmp = dynamic_cast<RteExample*>(child);
      if (pExmp == NULL) {
        continue;
      }

      m_os << " " << pExmp->GetTag() << " ";
      {
        auto attributes = pExmp->GetAttributes();
        for (auto itattr = attributes.begin(); itattr != attributes.end(); itattr++) {
          m_os << itattr->first << ":" << itattr->second << " " ;
        }
        m_os << endl;
      }

      m_os << " Desc: " << pExmp->GetDescription() << endl;

      // board
      const RteItem* boardInfo = pExmp->GetBoardInfoItem();
      if (boardInfo) {
        m_os << " Board: " << boardInfo->GetAttributesString() << endl;
      }

      auto children = pExmp->GetChildren();
      for (auto itchild = children.begin(); itchild != children.end(); ++itchild) {
        RteItem *pi = *itchild;
        if (pi == NULL) {
          continue;
        }

        m_os << " " << pi->GetTag() << ": ";
        m_os << pi->GetAttributesAsXmlString() << endl;
      }

      m_os << " --- Categories --- " << endl;
      auto listCat = pExmp->GetCategories();
      for (auto itcat = listCat.begin(); itcat != listCat.end(); ++itcat) {
        m_os << " " << *itcat << endl;
      }

      m_os << " --- Keywords --- " << endl;
      auto listKw = pExmp->GetKeywords();
      for (auto itkey = listKw.begin(); itkey != listKw.end(); ++itkey) {
        m_os << " " << *itkey << endl;
      }
    }
  }
}

void RteChk::DumpModel()
{
  if (m_rteModel->IsEmpty())
    return;
  auto& packs = m_rteModel->GetChildren();
  int i = 0;
  for (auto it = packs.begin(); it != packs.end(); it++, i++) {
    RtePackage* pack = dynamic_cast<RtePackage*>(*it);
    if (pack == NULL) {
      continue;
    }
    m_os << endl;

    m_os << "Pack[" << i << "] : " << pack->GetVendorName()
      << "." << pack->GetName()
      << "." << pack->GetVersionString()
      << " (";
    switch (RteChk::GetPackageType(pack)) {
    case PACK_GENERIC:
      m_os << "generic";
      break;
    case PACK_DFP:
      m_os << "DFP";
      break;
    case PACK_BSP:
      m_os << "BSP";
      break;
    };
    m_os << " )" << endl;
    m_os << " Filename:" << RteUtils::ExtractFileName(pack->GetPackageFileName()) << endl;

    m_os << "--- Conditions --- " << endl;
    DumpConditions(pack);
    m_os << "--- Components --- " << endl;
    DumpComponents(pack);
    m_os << "--- Examples --- " << endl;
    DumpExamples  (pack);
  }
  // Devices
  m_os << "--- Device Tree --- " << endl;
  RteDeviceItemAggregate* da = m_rteModel->GetDeviceTree();
  DumpDeviceAggregate(da);

}

void RteChk::ListPacks()
{
  RtePackageMap packs;
  CollectPacks(packs, PACK_GENERIC);
  m_os << "Generic: " << packs.size() << endl;
  if (IsFlagSet(PACKS)) {
    PrintPacks(packs);
  }
  packs.clear();
  CollectPacks(packs, PACK_DFP);
  m_os << "DFP: " << packs.size() << endl;
  if (IsFlagSet(PACKS) && IsFlagSet(DFP)) {
    PrintPacks(packs);
  }
  packs.clear();
  CollectPacks(packs, PACK_BSP);
  m_os << "BSP: " << packs.size() << endl;
  if (IsFlagSet(PACKS) && IsFlagSet(BSP)) {
    PrintPacks(packs);
  }
}


void RteChk::CollectPacks(RtePackageMap& packs, PACK_TYPE packType)
{
  for (auto child : m_rteModel->GetChildren()) {
    RtePackage* pack = dynamic_cast<RtePackage*>(child);
    if (pack == NULL) {
      continue;
    }
    const string& id = pack->GetID();
    if (packType == GetPackageType(pack)) {
      packs[id] = pack;
    }
  }
}

void RteChk::PrintPacks(const RtePackageMap& packs)
{
  for (auto& it : packs) {
    m_os << it.first << endl;
  }
  m_os << endl;

}

PACK_TYPE RteChk::GetPackageType(RtePackage* pack)
{
  const string& id = pack->GetID();
  PACK_TYPE pType = PACK_GENERIC;
  if (id.find("ARM.CMSIS") == string::npos) {
    if (pack->GetDeviceFamiles()) {
      pType = PACK_DFP;
    } else if (pack->GetItem("boards") != nullptr) {
      pType = PACK_BSP;
    }
  }
  return pType;
}

void RteChk::ListComponents()
{
  RteComponentMap components;
  CollectComponents(components, PACK_GENERIC);
  m_os << "From generic packs: " << components.size() << endl;
  if (IsFlagSet(COMPONENTS)) {
    PrintComponents(components, false);
  }
  components.clear();
  CollectComponents(components, PACK_DFP);
  m_os << "From DFP: " << components.size() << endl;
  if (IsFlagSet(COMPONENTS) && IsFlagSet(DFP)) {
    PrintComponents(components, false);
  }
  components.clear();
  CollectComponents(components, PACK_BSP);
  m_os << "From BSP: " << components.size() << endl;
  if (IsFlagSet(COMPONENTS) && IsFlagSet(BSP)) {
    PrintComponents(components, false);
  }
}

void RteChk::CollectComponents(RteComponentMap& components, PACK_TYPE packType)
{
  for (auto& it : m_rteModel->GetComponentList()) {
    RteComponent* pComp = it.second;
    const string& id = it.first;
    RtePackage* pack = pComp->GetPackage();
    if (GetPackageType(pack) == packType) {
      components[id] = pComp;
    }
  }
}


void RteChk::PrintComponents(RteComponentMap& components, bool bFiles)
{
  for (auto& it : components) {
    PrintComponent(it.second, bFiles);
  }
}

void RteChk::PrintComponent(RteComponent* pComp, bool bFiles)
{
  m_os << pComp->GetFullDisplayName() << endl;
  m_os << " " << pComp->GetTag() << " " ;
  {
    m_os << "Cbundle=" << pComp->GetCbundleName() << ", ";
    m_os << "Cclass=" << pComp->GetCclassName() << ", ";
    m_os << "Cgroup=" << pComp->GetCgroupName() << ", ";
    m_os << "Csub=" << pComp->GetCsubName() << ", ";
    m_os << "Cvariant=" << pComp->GetCvariantName() << ", ";
    m_os << "Cvendor=" << pComp->GetVendorString() << ", ";
    m_os << "Cversion=" << pComp->GetVersionString() << ", ";
    m_os << "Capiversion=" << pComp->GetApiVersionString() << ", ";
    m_os << "Condition=" << pComp->GetConditionID();
    m_os << endl;
  }
  m_os << "     Desc: " << pComp->GetDescription() << endl;
  RteFileContainer* fileContainer = pComp->GetFileContainer();

  if (fileContainer && bFiles) {
    // files
    m_os << " --- Files --- " << endl;
    for (auto child : fileContainer->GetChildren()) {
      RteFile *pFile = dynamic_cast<RteFile*>(child);
      if (pFile == NULL) {
        continue;
      }

      m_os << " " << pFile->GetTag() << " ";
      m_os << pFile->GetName() << " - ";
      m_os << pFile->GetCategoryString() << " Copy:" << (pFile->IsConfig() ? "yes" : "no");
      m_os << " Condition=" << pFile->GetConditionID();
      m_os << endl;
    }
  }
  m_os << endl;
}

unsigned long RteChk::GetPackCount() const
{
  return m_npdsc;
}
int RteChk::GetComponentCount() const
{
  return m_rteModel ? m_rteModel->GetComponentCount() : -1;
}
int RteChk::GetDeviceCount() const
{
  return m_rteModel ? m_rteModel->GetDeviceCount() : -1;
}
int RteChk::GetBoardCount() const
{
  return m_rteModel ? (int)m_rteModel->GetBoards().size() : -1;
}


//------------------------------------------------------------------
int RteChk::CheckRte(int argc, char* argv[])
{
  RteChk rteChk;

  int res = rteChk.ProcessArguments(argc, argv);
  if (res != 0)
    return res;

  cout << ">>>> Start RTE check" << endl;
  res = rteChk.RunCheckRte();
  cout << "<<<< End RTE check : " << res << endl << endl;

  return res;
}
