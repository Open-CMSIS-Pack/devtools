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
#include "RteCprjProject.h"
#include "CprjFile.h"

#include "RteUtils.h"
#include "RteFsUtils.h"
#include "XmlFormatter.h"

using namespace std;

static string schemaFile = "CPRJ.xsd";
static string schemaVer = "0.0.9";

static constexpr const char* R801 = "CMSIS_PACK_ROOT directory is not set";
static constexpr const char* R802 = "Error parsing XML file";
static constexpr const char* R811 = "Error parsing cprj file";
static constexpr const char* R812 = "Error reading project file";
static constexpr const char* R820 = "Malformed or incomplete file";
static constexpr const char* R821 = "Required pack not installed: ";
static constexpr const char* R822 = "Pack 'path' was not found";
static constexpr const char* R823 = "No PDSC file was found";
static constexpr const char* R824 = "Multiple PDSC files were found";

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
    string fileName = file.empty() ? cprjFile->GetRootFileName() : file;
    if (!fileName.empty()) {
      RteFsUtils::CopyBufferToFile(fileName, xmlContent, false);
      return true;
    }
  }
  return false;
}

RteCprjProject* RteKernel::LoadCprj(const string& cprjFileName, const string& toolchain, bool initialize, bool updateRteFiles)
{
  CprjFile* cprj = ParseCprj(cprjFileName);
  if (!cprj)  {
    return nullptr;
  }

  RteCprjProject* cprjProject = new RteCprjProject(cprj);
 // cprj->Reparent(cprjProject);
  cprjProject->SetCallback(GetRteCallback());
  cprjProject->SetAttribute("update-rte-files", updateRteFiles ? "1" : "0");
  m_globalModel->AddProject(0, cprjProject);
  // ensure project is set active
  m_globalModel->SetActiveProjectId(cprjProject->GetProjectId());

  if (initialize) {
    if (!InitializeCprj(cprjProject, toolchain)) {
      cprjProject->Invalidate();
    }
  }
  return cprjProject;
}


CprjFile* RteKernel::ParseCprj(const string& cprjFile)
{
  string msg = "Loading '" + cprjFile + "'";
  GetRteCallback()->OutputInfoMessage(msg);
  unique_ptr<XMLTree> xmlTree = CreateUniqueXmlTree();
  if (!xmlTree->AddFileName(cprjFile, true)) {
    GetRteCallback()->Err("R811", R811, cprjFile);
    GetRteCallback()->OutputMessages(xmlTree->GetErrorStrings());
    return nullptr;
  }

  XMLTreeElement* docElement = xmlTree->GetFirstChild();

  CprjFile* cprj = new CprjFile(nullptr);
  cprj->SetRootFileName(docElement->GetRootFileName());
  if (!cprj->Construct(docElement) || !cprj->Validate()) {
    GetRteCallback()->Err("R812", R812, cprjFile);
    RtePrintErrorVistior visitor(m_rteCallback);
    cprj->AcceptVisitor(&visitor);
    delete cprj;
    return nullptr;
  }
  return cprj;
}

bool RteKernel::InitializeCprj(RteCprjProject* cprjProject, const string& toolchain, const string& toolChainVersion)
{
  if (!cprjProject) {
    return false;
  }
  if (!cprjProject->SetToolchain(toolchain)) {
    return false;
  }
  // load required packs
  if (!LoadRequiredPdscFiles(cprjProject->GetCprjFile())) {
    return false;
  }
  cprjProject->Initialize();
  cprjProject->GenerateRteHeaders();
  return true;
}


RtePackage* RteKernel::LoadPack(const string& pdscFile)
{
  if(pdscFile.empty()) {
    return nullptr;
  }

  unique_ptr<XMLTree> xmlTree = CreateUniqueXmlTree();
  if (!xmlTree->AddFileName(pdscFile, true) || xmlTree->GetChildren().empty()) {
    GetRteCallback()->Err("R802", R802, pdscFile);
    GetRteCallback()->OutputMessages(xmlTree->GetErrorStrings());
    return nullptr;
  }
  XMLTreeElement *doc = *(xmlTree->GetChildren().begin());
  return GetGlobalModel()->ConstructPack(doc);
}


bool RteKernel::LoadRequiredPdscFiles(CprjFile* cprjFile)
{
  if(GetCmsisPackRoot().empty()) {
    GetRteCallback()->Err("R801", R801);
    return false;
  }

  const list<RteItem*>& packRequirements =  cprjFile->GetPackRequirements();
  if (packRequirements.empty()) {
    GetRteCallback()->Err("R820", R820, cprjFile->GetPackageFileName());
    return false; // it is an error: malformed cprj file
  }

  list<RtePackage*> packs;
  set<string> processedFiles;
  for (RteItem *packRequirement : packRequirements) {
    string packId, pdscFile;
    // Get pdsc from pack 'path' attribute
    if (packRequirement->HasAttribute("path")) {
      const string& absCprjPath = RteFsUtils::AbsolutePath(cprjFile->GetPackageFileName()).generic_string();
      pdscFile = GetPdscFileFromPath(*packRequirement, RteFsUtils::ParentPath(absCprjPath), packId);
    }
    // Get local repo version
    if (pdscFile.empty()) {
      pdscFile = GetLocalPdscFile(*packRequirement, GetCmsisPackRoot(), packId);
    }
    if(pdscFile.empty()) {
      pdscFile = GetInstalledPdscFile(*packRequirement, GetCmsisPackRoot(), packId);
    }
    if(pdscFile.empty()) {
      string msg= R821;

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


string RteKernel::GetInstalledPdscFile(const XmlItem& attributes, const string& rtePath, string& packId)
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

string RteKernel::GetLocalPdscFile(const XmlItem& attributes, const string& rtePath, string& packId)
{
  const string& name = attributes.GetAttribute("name");
  const string& vendor = attributes.GetAttribute("vendor");
  const string& versionRange = attributes.GetAttribute("version");

  string url, version;
  if (GetUrlFromIndex(rtePath, name, vendor, versionRange, url, version)) {
    packId = vendor + '.' + name + '.' + version;
    return url;
  }

  return RteUtils::EMPTY_STRING;
}

string RteKernel::GetPdscFileFromPath(const XmlItem& attributes, const string& cprjPath, string& packId)
{
  const string& name = attributes.GetAttribute("name");
  const string& vendor = attributes.GetAttribute("vendor");
  const string& version = attributes.GetAttribute("version");

  string packPath = attributes.GetAttribute("path");
  RteFsUtils::NormalizePath(packPath, cprjPath + '/');
  if (!RteFsUtils::Exists(packPath)) {
    GetRteCallback()->Err("R822", R822, packPath);
  } else {
    const auto& pdscFilesList = RteFsUtils::FindFiles(packPath, ".pdsc");
    if (pdscFilesList.empty()) {
      GetRteCallback()->Err("R823", R823, packPath);
    } else if (pdscFilesList.size() > 1) {
      GetRteCallback()->Err("R824", R824, packPath);
    } else {
      packId = vendor + '.' + name + (version.empty() ? "" : '.' + version);
      return pdscFilesList.front().generic_string();
    }
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
      // Load the local pack to get its version. The 'version' attribute in the local repository index is ignored.
      list<string> localPdscFiles;
      RteFsUtils::GetPackageDescriptionFiles(localPdscFiles, RteFsUtils::GetAbsPathFromLocalUrl(item->GetAttribute("url")), 1);
      for (const auto& localPdscFile : localPdscFiles) {
        RtePackage* pack = LoadPack(localPdscFile);
        if (pack) {
          const string& version = pack->GetVersionString();
          if (versionRange.empty() || VersionCmp::RangeCompare(version, versionRange) == 0) {
            pdscMap[version] = localPdscFile;
          }
        }
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
    const string& url = RteFsUtils::GetAbsPathFromLocalUrl(item->GetAttribute("url"));
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
