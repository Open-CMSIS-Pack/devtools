/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteKernel.cpp
* @brief CMSIS RTE Data model
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/
#include "RteKernel.h"

#include "RteItem.h"
#include "RteCprjModel.h"
#include "RteCprjProject.h"
#include "CprjFile.h"

#include "RteUtils.h"
#include "RteFsUtils.h"
#include "XmlFormatter.h"

using namespace std;

static string schemaFile = "CPRJ.xsd";
static string schemaVer = "0.0.9";

RteKernel RteKernel::NULL_RTE_KERNEL;

RteKernel::RteKernel(RteCallback* rteCallback, RteGlobalModel* globalModel):
m_globalModel(globalModel),
m_bOwnModel(false),
m_rteCallback(rteCallback)
{
  if (!m_globalModel) {
    m_globalModel = new RteGlobalModel();
    m_bOwnModel = true;
  }
  m_globalModel->SetCallback(m_rteCallback);
}


RteKernel::~RteKernel()
{
  if (m_bOwnModel) {
    delete m_globalModel;
  }
}

bool RteKernel::SetCmsisPackRoot(const string& cmsisPackRoot)
{
  if (m_cmsisPackRoot == cmsisPackRoot)
    return false;
  m_cmsisPackRoot = cmsisPackRoot;
  return true;
}

RteCallback* RteKernel::GetRteCallback() const
{
  return m_rteCallback ? m_rteCallback : RteCallback::GetGlobal();
}

RteProject* RteKernel::GetProject(int nPjNum) const
{
  return GetGlobalModel()->GetProject(nPjNum);
}

RteTarget* RteKernel::GetTarget(const string& targetName, int nPjNum) const
{
  RteProject* rteProject = GetProject(nPjNum);
  if (rteProject) {
   if(targetName.empty())
     return rteProject->GetActiveTarget();
    return rteProject->GetTarget(targetName);
  }
  return nullptr;
}

RteModel* RteKernel::GetTargetModel(const string& targetName, int nPjNum) const
{
  RteTarget* rteTarget = GetTarget(targetName, nPjNum);
  if (rteTarget)
    return rteTarget->GetModel();
  return nullptr;
}

RteProject* RteKernel::GetActiveProject() const
{
  return GetGlobalModel()->GetActiveProject();
}

RteTarget* RteKernel::GetActiveTarget() const
{
  RteProject* rteProject = GetActiveProject();
  if (rteProject) {
    return rteProject->GetActiveTarget();
  }
  return nullptr;
}

RteModel* RteKernel::GetActiveTargetModel() const
{
  RteTarget* rteTarget = GetActiveTarget();
  if (rteTarget) {
    return rteTarget->GetModel();
  }
  return nullptr;
}


RteDeviceItem* RteKernel::GetActiveDevice() const
{
  RteTarget* rteTarget = GetActiveTarget();
  if (rteTarget) {
    return rteTarget->GetDevice();
  }
  return nullptr;
}

RteCprjProject* RteKernel::GetActiveCprjProject() const
{
  return dynamic_cast<RteCprjProject*>(GetActiveProject());
}

CprjFile* RteKernel::GetActiveCprjFile() const
{
  RteCprjProject* project = GetActiveCprjProject();
  if(project) {
    return project->GetCprjFile();
  }
  return nullptr;
}

bool RteKernel::SaveActiveCprjFile(const string& file/* = RteUtils::EMPTY_STRING*/) const {
  CprjFile* cprjFile = GetActiveCprjFile();
  if (cprjFile) {
    XMLTreeElement *root = cprjFile->CreateXmlTreeElement(NULL);
    XmlFormatter xmlFormatter;
    string xmlContent = xmlFormatter.FormatElement(root, schemaFile, schemaVer);
    string fileName = file.empty() ? cprjFile->GetPackageFileName() : file;
    if (!fileName.empty()) {
      RteFsUtils::CopyBufferToFile(fileName, xmlContent, false);
      return true;
    }
  }
  return false;
}

RteCprjProject* RteKernel::LoadCprj(const string& cprjFileName, const string& toolchain)
{
  RteCprjModel* cprjModel = ParseCprj(cprjFileName);
  if (!cprjModel)  {
    return nullptr;
  }

  RteCprjProject* cprjProject = new RteCprjProject(cprjModel);
  cprjProject->SetCallback(GetRteCallback());
  m_globalModel->AddProject(0, cprjProject);
  // ensure project is set active
  m_globalModel->SetActiveProjectId(cprjProject->GetProjectId());

  if (!cprjProject->SetToolchain(toolchain)) {
    cprjProject->Invalidate();
    return cprjProject;
  }
  // load required packs
  if(!LoadRequiredPdscFiles(cprjModel->GetCprjFile())){
    cprjProject->Invalidate();
    return cprjProject;
  }
  cprjProject->Initialize();
  cprjProject->GenerateRteHeaders();
  return cprjProject;
}


RteCprjModel* RteKernel::ParseCprj(const string& cprjFile)
{
  string msg = "Loading '" + cprjFile + "'";
  GetRteCallback()->OutputInfoMessage(msg);
  unique_ptr<XMLTree> xmlTree = CreateUniqueXmlTree();
  if (!xmlTree->AddFileName(cprjFile, true)) {
    GetRteCallback()->Err("R811", "Error parsing cprj file", cprjFile);
    GetRteCallback()->OutputMessages(xmlTree->GetErrorStrings());
    return nullptr;
  }

  RteCprjModel* cprjModel = new RteCprjModel();
  if (!cprjModel->Construct(xmlTree.get()) || !cprjModel->Validate())  {
    GetRteCallback()->Err("R812", "Error reading project file", cprjFile);
    RtePrintErrorVistior visitor(m_rteCallback);
    cprjModel->AcceptVisitor(&visitor);
    delete cprjModel;
    return nullptr;
  }
  return cprjModel;
}

RtePackage* RteKernel::LoadPack(const string& pdscFile)
{
  if(pdscFile.empty()) {
    return nullptr;
  }

  unique_ptr<XMLTree> xmlTree = CreateUniqueXmlTree();
  if (!xmlTree->AddFileName(pdscFile, true) || xmlTree->GetChildren().empty()) {
    GetRteCallback()->Err("R802", "Error parsing XML file", pdscFile);
    GetRteCallback()->OutputMessages(xmlTree->GetErrorStrings());
    return nullptr;
  }
  XMLTreeElement *doc = *(xmlTree->GetChildren().begin());
  return GetGlobalModel()->ConstructPack(doc);
}


bool RteKernel::LoadRequiredPdscFiles(CprjFile* cprjFile)
{
  if(GetCmsisPackRoot().empty()) {
    GetRteCallback()->Err("R801", "CMSIS_PACK_ROOT directory is not set");
    return false;
  }

  const list<RteItem*>& packRequirements =  cprjFile->GetPackRequirements();
  if (packRequirements.empty()) {
    GetRteCallback()->Err("R820", "Malformed or incomplete file", cprjFile->GetPackageFileName());
    return false; // it is an error: malformed cprj file
  }

  list<RtePackage*> packs;
  set<string> processedFiles;
  for (RteItem *packRequirement : packRequirements) {
    string packId;
    // Get local repo version
    string pdscFile = GetLocalPdscFile(*packRequirement, GetCmsisPackRoot(), packId);
    if(pdscFile.empty()) {
      pdscFile = GetInstalledPdscFile(*packRequirement, GetCmsisPackRoot(), packId);
    }
    if(pdscFile.empty()) {
      string msg= "Required pack not installed: ";

      msg += packRequirement->GetPackageID(true);

      const string& name = packRequirement->GetAttribute("name");
      const string& vendor = packRequirement->GetAttribute("vendor");
      const string& version = packRequirement->GetAttribute("version");
      msg += vendor + "." + name;
      if (!version.empty())
        msg += "." + version;
      GetRteCallback()->Err("R821", msg, cprjFile->GetPackageFileName());
      // TODO: install missing pack (not needed for web version)
      return false;
    }
    if(processedFiles.find(pdscFile) != processedFiles.end()) {
      continue; // already processed
    }
    processedFiles.insert(pdscFile);
    if(GetGlobalModel()->GetPackage(packId) ){
       continue; // already installed
    }
    RtePackage* pack = LoadPack(pdscFile);
    if (!pack) {
      return false; // error already reported
    }
    packs.push_back(pack);
  }
  GetGlobalModel()->InsertPacks(packs);
  return true;
}


string RteKernel::GetInstalledPdscFile(const RteAttributes& attributes, const string& rtePath, string& packId)
{
  const string& name = attributes.GetAttribute("name");
  const string& vendor = attributes.GetAttribute("vendor");
  const string& versionRange = attributes.GetAttribute("version");

  string path(rtePath);
  path += '/' + vendor + '/' + name;


  string installedVersion = RteFsUtils::GetInstalledPackVersion(path, versionRange);
  if (!installedVersion.empty()) {
    packId = vendor + '.' + name + '.' + installedVersion;
    return path + '/' + installedVersion + '/' + vendor + '.' + name + ".pdsc";
  }

  return RteUtils::EMPTY_STRING;
}

string RteKernel::GetLocalPdscFile(const RteAttributes& attributes, const string& rtePath, string& packId)
{
  const string& name = attributes.GetAttribute("name");
  const string& vendor = attributes.GetAttribute("vendor");
  const string& versionRange = attributes.GetAttribute("version");

  string url, version;
  if (GetUrlFromIndex(rtePath, name, vendor, versionRange, url, version)) {
    packId = vendor + '.' + name + '.' + version;
    static const string&& prefix = "file://localhost/";
    if (url.find(prefix) == 0) {
      url.erase(0, prefix.length());
    }
    return url + vendor + '.' + name + ".pdsc";
  }

  return RteUtils::EMPTY_STRING;
}

bool RteKernel::GetUrlFromIndex(const string& rtePath, const string& name, const string& vendor, const string& versionRange, string& indexedUrl, string& indexedVersion)
{
  unique_ptr<XMLTree> xmlTree = CreateUniqueXmlTree();
  list<XMLTreeElement*> indexList;
  if (!GetLocalPacks(rtePath, xmlTree, indexList)) {
    return false;
  }
  map<string, string> pdscMap;
  // Populate map with items matching name, vendor and version range
  for (const auto& item : indexList) {
    if ((name == item->GetAttribute("name")) && (vendor == item->GetAttribute("vendor"))) {
      const string& version = item->GetAttribute("version");
      if (versionRange.empty() || VersionCmp::RangeCompare(version, versionRange) == 0) {
        pdscMap[version] = item->GetAttribute("url");
      }
    }
  }

  // Return last element from the map
  if (!pdscMap.empty()) {
    indexedVersion = pdscMap.rbegin()->first;
    indexedUrl = pdscMap.rbegin()->second;
    return true;
  }

  return false;
}

bool RteKernel::GetLocalPacks(const string& rtePath, unique_ptr<XMLTree>& xmlTree, list<XMLTreeElement*>& packs)
{
  // Parse local repository index file
  const string& indexPath = string(rtePath) + "/.Local/local_repository.pidx";

  if (!RteFsUtils::Exists(indexPath)) {
    return true;
  }

  if (!xmlTree->AddFileName(indexPath, true) || xmlTree->GetChildren().empty()) {
    return false;
  }

  // Parse index file
  XMLTreeElement* indexChild, * pIndexChild;
  if (!xmlTree->ParseAll() || ((indexChild = xmlTree->GetFirstChild("index")) == nullptr)
    || ((pIndexChild = indexChild->GetFirstChild("pindex")) == nullptr)) {
    return false;
  }

  packs = pIndexChild->GetChildren();
  return true;
}

bool RteKernel::GetLocalPacksUrls(const string& rtePath, list<string>& urls)
{
  unique_ptr<XMLTree> xmlTree = CreateUniqueXmlTree();
  list<XMLTreeElement*> indexList;
  if (!GetLocalPacks(rtePath, xmlTree, indexList)) {
    return false;
  }
  for (const auto& item : indexList) {
    static const string&& prefix = "file://localhost/";
    string url = item->GetAttribute("url");
    if (url.find(prefix) == 0) {
      url.erase(0, prefix.length());
    }
    urls.push_back(url);
  }
  return true;
}

unique_ptr<XMLTree> RteKernel::CreateUniqueXmlTree() const
{
  unique_ptr<XMLTree> xmlTree(CreateXmlTree());
  if (xmlTree.get() != nullptr) {
    xmlTree->Init();
  }
  return xmlTree;
}

// End of RteKernel.cpp
