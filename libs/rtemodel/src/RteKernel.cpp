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
#include "RteItemBuilder.h"

#include "RteUtils.h"
#include "RteFsUtils.h"
#include "RteConstants.h"

#include "XmlFormatter.h"
#include "YmlFormatter.h"

#include "CollectionUtils.h"

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

RteKernel::RteKernel(RteCallback* rteCallback, RteGlobalModel* globalModel) :
m_globalModel(globalModel),
m_bOwnModel(false),
m_rteCallback(rteCallback)
{
  if (!m_globalModel) {
    m_globalModel = new RteGlobalModel();
    m_bOwnModel = true;
  }
  if (m_rteCallback) {
    m_globalModel->SetCallback(m_rteCallback);
  }

  // default attributes
  m_toolInfo.AddAttribute("name", RteUtils::EMPTY_STRING);
  m_toolInfo.AddAttribute("version", RteUtils::EMPTY_STRING);
}


RteKernel::~RteKernel()
{
  if (m_bOwnModel) {
    delete m_globalModel;
  }
  ClearExternalGenerators();
}

bool RteKernel::Init()
{
  LoadExternalGenerators();
  return true;
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

void RteKernel::SetRteCallback(RteCallback* callback)
{
  m_rteCallback = callback;
  GetGlobalModel()->SetCallback(m_rteCallback);
}

RteGenerator* RteKernel::GetExternalGenerator(const std::string& id) const
{
  return get_or_null(m_externalGenerators, id);
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
  auto rteItemBuilder = CreateUniqueRteItemBuilder();
  unique_ptr<XMLTree> xmlTree = CreateUniqueXmlTree(rteItemBuilder.get());
  if (!xmlTree->AddFileName(cprjFile, true)) {
    GetRteCallback()->Err("R811", R811, cprjFile);
    GetRteCallback()->OutputMessages(xmlTree->GetErrorStrings());
    return nullptr;
  }
  CprjFile* cprj = rteItemBuilder->GetCprjFile();

  if (!cprj || !cprj->Validate()) {
    GetRteCallback()->Err("R812", R812, cprjFile);
    if (cprj) {
      RtePrintErrorVistior visitor(m_rteCallback);
      cprj->AcceptVisitor(&visitor);
      delete cprj;
    }
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
  return true;
}

void RteKernel::LoadExternalGenerators()
{
  ClearExternalGenerators();
  string etcDir = GetCmsisToolboxDir() + "/etc";
  list<string> files;
  RteFsUtils::GetMatchingFiles(files, ".generator.yml", etcDir, 1, true);
  RteGlobalModel* globalModel = GetGlobalModel();
  auto rteItemBuilder = CreateUniqueRteItemBuilder(globalModel);
  unique_ptr<XMLTree> ymlTree = CreateUniqueXmlTree(rteItemBuilder.get(), ".yml");
  for(auto& f : files) {
    if(contains_key(m_externalGeneratorFiles, f)) {
      continue;
    }
    bool result = ymlTree->ParseFile(f);
    RteItem* rootItem = rteItemBuilder->GetRoot();
    if(result && rootItem) {
      m_externalGeneratorFiles[f] = rootItem;
      for(auto item : rootItem->GetChildren()) {
        RteGenerator* g = dynamic_cast<RteGenerator*>(item);
        if(g) {
          m_externalGenerators[g->GetID()] = g;
        }
      }
    }
    rteItemBuilder->Clear(false);
  }
}

void RteKernel::ClearExternalGenerators()
{
  m_externalGenerators.clear();
  for(auto [_, g] : m_externalGeneratorFiles) {
    delete g;
  }
  m_externalGeneratorFiles.clear();
}

RtePackage* RteKernel::LoadPack(const string& pdscFile, PackageState packState) const
{
  if(pdscFile.empty()) {
    return nullptr;
  }
  RtePackRegistry* packRegistry = GetPackRegistry();
  RtePackage* pack = packRegistry->GetPack(pdscFile);
  if(pack) {
    return pack;
  }
  const string ext = RteUtils::ExtractFileExtension(pdscFile, true);
  auto rteItemBuilder= CreateUniqueRteItemBuilder(GetGlobalModel(), packState);
  unique_ptr<XMLTree> xmlTree = CreateUniqueXmlTree(rteItemBuilder.get(), ext);
  bool success = xmlTree->AddFileName(pdscFile, true);
  pack = rteItemBuilder->GetPack();
  if (!success || !pack) {
    GetRteCallback()->Err("R802", R802, pdscFile);
    GetRteCallback()->OutputMessages(xmlTree->GetErrorStrings());
    return nullptr;
  }
  if(packState != PackageState::PS_GENERATED) {
    if(!packRegistry->AddPack(pack) && pack) {
      delete pack;
      pack = nullptr;
    }
  }
  return pack;
}

bool RteKernel::LoadPacks(const std::list<std::string>& pdscFiles, std::list<RtePackage*>& packs, RteModel* model, bool bReplace) const
{
  bool success = true;
  if(pdscFiles.empty()) {
    return success;
  }
  if(!model) {
    model = GetGlobalModel();
  }
  RtePackRegistry* packRegistry = GetPackRegistry();
  unique_ptr<XMLTree> xmlTree = CreateUniqueXmlTree();
  for(auto& pdscFile : pdscFiles) {
    auto rteItemBuilder = CreateUniqueRteItemBuilder(model, model->GetPackageState());
    xmlTree->SetXmlItemBuilder(rteItemBuilder.get());
    RtePackage* pack = packRegistry->GetPack(pdscFile);
    if(bReplace) {
      packRegistry->ErasePack(pdscFile);
    } else if(pack) {
      packs.push_back(pack);
      continue;
    }
    bool result = xmlTree->AddFileName(pdscFile, true);
    pack = rteItemBuilder->GetPack();
    if(!result || !pack) {
      GetRteCallback()->Err("R802", R802, pdscFile);
      GetRteCallback()->OutputMessages(xmlTree->GetErrorStrings());
      success = false;
    } else {
      if(packRegistry->AddPack(pack, bReplace)) {
        packs.push_back(pack);
      } else {
        delete pack;
      }
    }
    GetRteCallback()->PackProcessed(pdscFile, result);
  }
  return success;
}


bool RteKernel::LoadRequiredPdscFiles(CprjFile* cprjFile)
{
  if(GetCmsisPackRoot().empty()) {
    GetRteCallback()->Err("R801", R801);
    return false;
  }

  auto& packRequirements =  cprjFile->GetPackRequirements();
  if (packRequirements.empty()) {
    GetRteCallback()->Err("R820", R820, cprjFile->GetPackageFileName());
    return false; // it is an error: malformed cprj file
  }

  list<RtePackage*> packs;
  set<string> processedFiles;
  for (RteItem *packRequirement : packRequirements) {
    pair<string,string> pdscFile;
    // Get pdsc from pack 'path' attribute
    if (packRequirement->HasAttribute("path")) {
      const string& absCprjPath = RteFsUtils::AbsolutePath(cprjFile->GetPackageFileName()).generic_string();
      pdscFile = GetPdscFileFromPath(*packRequirement, RteFsUtils::ParentPath(absCprjPath));
    }
    // Get installed or local repo version
    if (pdscFile.second.empty()) {
      pdscFile = GetEffectivePdscFile(*packRequirement);
    }
    if(pdscFile.second.empty()) {
      string msg= R821;

      msg += packRequirement->GetPackageID(true);

      GetRteCallback()->Err("R821", msg, cprjFile->GetPackageFileName());
      // TODO: install missing pack (not needed for web version)
      return false;
    }
    if(processedFiles.find(pdscFile.second) != processedFiles.end()) {
      continue; // already processed
    }
    processedFiles.insert(pdscFile.second);
    if(GetGlobalModel()->GetPackage(pdscFile.first) ){
       continue; // already installed
    }
    RtePackage* pack = LoadPack(pdscFile.second);
    if (!pack) {
      return false; // error already reported
    }
    packs.push_back(pack);
  }
  GetGlobalModel()->InsertPacks(packs);
  return true;
}

bool RteKernel::GetEffectivePdscFilesAsMap(map<string, string, RtePackageComparator>& pdscMap, bool latest) const
{
  auto& cmsisPackRoot = GetCmsisPackRoot();
  if(cmsisPackRoot.empty()) {
    return false;
  }
  // Get all installed files
  RteKernel::GetInstalledPdscFiles(pdscMap);

  // Overwrite entries with local pdsc files if any
  XmlItem emptyAttributes;
  GetLocalPdscFiles(emptyAttributes, pdscMap);

  // purge entries if only latest are required
  if(latest) {
    string processedCommonId;
    for(auto it = pdscMap.begin(); it != pdscMap.end();) {
      auto itcur = it++;
      string commonId = RtePackage::CommonIdFromId(itcur->first);
      if(commonId == processedCommonId) {
        pdscMap.erase(itcur);
      } else {
        processedCommonId = commonId;
      }
    }
  }
  return true;
}


bool RteKernel::GetEffectivePdscFiles(std::list<std::string>& pdscFiles, bool latest) const
{
  map<string, string, RtePackageComparator> pdscMap;
  if(!GetEffectivePdscFilesAsMap(pdscMap, latest)) {
    return false;
  }
  for(auto& [id, f] : pdscMap) {
    pdscFiles.push_back(f);
  }
  return true;
}


bool RteKernel::LoadAndInsertPacks(std::list<RtePackage*>& packs, std::list<std::string>& pdscFiles) {
  RteGlobalModel* globalModel = GetGlobalModel();
  if (!globalModel) {
    return false;
  }
  std::list<RtePackage*> newPacks;
  pdscFiles.unique();
  for (const auto& pdscFile : pdscFiles) {
    RtePackage* pack = LoadPack(pdscFile);
    if (!pack) {
      return false;
    }
    if(!RtePackage::GetPackFromList(pack->GetID(), packs)){
      newPacks.push_back(pack);
    }
  }

  globalModel->InsertPacks(newPacks);

  // Track only packs that were actually inserted into the model
  packs.clear();
  for (const auto& [_, pack] : globalModel->GetPackages()) {
    packs.push_back(pack);
  }
  return true;
}


void RteKernel::GetInstalledPdscFiles(std::map<std::string, std::string, RtePackageComparator>& pdscMap) const
{
  list<string> allFiles;
  RteFsUtils::GetPackageDescriptionFiles(allFiles, GetCmsisPackRoot(), 3);
  for(auto& f : allFiles) {
    string id = RtePackage::PackIdFromPath(f);
    pdscMap[id] = f;
  }
}

pair<string, string> RteKernel::GetInstalledPdscFile(const XmlItem& attributes) const
{
  const string& name = attributes.GetAttribute("name");
  const string& vendor = attributes.GetAttribute("vendor");
  if(!name.empty() && !vendor.empty()) {
    string path = GetCmsisPackRoot() + '/' + vendor + '/' + name;
    const string& versionRange = attributes.GetAttribute("version");
    string installedVersion = RteFsUtils::GetInstalledPackVersion(path, versionRange);
    if(!installedVersion.empty()) {
      string packId = RtePackage::ComposePackageID(vendor, name, installedVersion);
      path += '/' + installedVersion + '/' + vendor + '.' + name + ".pdsc";
      return make_pair(packId, path);
    }
  }
  return make_pair(RteUtils::EMPTY_STRING, RteUtils::EMPTY_STRING);
}

pair<string, string> RteKernel::GetLocalPdscFile(const XmlItem& attributes) const
{
  map<string, string, RtePackageComparator> pdscMap;
  if(!attributes.IsEmpty() && GetLocalPdscFiles(attributes, pdscMap)) {
    return *pdscMap.begin();
  }
  return make_pair(RteUtils::EMPTY_STRING, RteUtils::EMPTY_STRING);
}

pair<string, string> RteKernel::GetEffectivePdscFile(const XmlItem& attributes) const
{
  auto localPdsc = GetLocalPdscFile(attributes);
  auto installedPdsc = GetInstalledPdscFile(attributes);

  string localVersion = RtePackage::VersionFromId(localPdsc.first);
  string installedVersion = RtePackage::VersionFromId(installedPdsc.first);
  if(!localVersion.empty() && VersionCmp::Compare(localVersion, installedVersion) >= 0) {
    return localPdsc;
  }
  return installedPdsc;
}


pair<string, string> RteKernel::GetPdscFileFromPath(const XmlItem& attributes, const string& prjPath) const
{
  const string& name = attributes.GetAttribute("name");
  const string& vendor = attributes.GetAttribute("vendor");
  const string& version = attributes.GetAttribute("version");

  string packPath = attributes.GetAttribute("path");
  RteFsUtils::NormalizePath(packPath, prjPath + '/');
  if (!RteFsUtils::Exists(packPath)) {
    GetRteCallback()->Err("R822", R822, packPath);
  } else {
    const auto& pdscFilesList = RteFsUtils::FindFiles(packPath, ".pdsc");
    if (pdscFilesList.empty()) {
      GetRteCallback()->Err("R823", R823, packPath);
    } else if (pdscFilesList.size() > 1) {
      GetRteCallback()->Err("R824", R824, packPath);
    } else {
      string packId = vendor + '.' + name + (version.empty() ? "" : '.' + version);
      string pdscFile = pdscFilesList.front().generic_string();
      return make_pair(packId, pdscFile);
    }
  }

  return make_pair(RteUtils::EMPTY_STRING, RteUtils::EMPTY_STRING);
}

bool RteKernel::GetLocalPdscFiles(const XmlItem& attr, std::map<std::string, std::string, RtePackageComparator>& pdscMap) const
{
  unique_ptr<XMLTreeElement> pIndex(ParseLocalRepositoryIdx());
  if(!pIndex) {
    return false;
  }
  const string& name = attr.GetAttribute("name");
  const string& vendor = attr.GetAttribute("vendor");
  const string& versionRange = attr.GetAttribute("version");
  bool found = false;
  // Populate map with items matching name, vendor and version range
  for (auto& item : pIndex->GetChildren()) {
    if ((name.empty() || name == item->GetAttribute("name"))
        && (vendor.empty() || vendor == item->GetAttribute("vendor")))
    {
      // Load the local pack to get its version. The 'version' attribute in the local repository index is ignored.
      string url = RteFsUtils::GetAbsPathFromLocalUrl(item->GetAttribute("url"));
      if(RteFsUtils::IsRelative(url)) {
        url = RteFsUtils::MakePathCanonical(item->GetRootFilePath() + url) + '/';
      }
      const string localPdscFile = url + item->GetAttribute("vendor") + '.' + item->GetAttribute("name") + ".pdsc";
      RtePackage* pack = LoadPack(localPdscFile);
      if(pack) {
        const string& version = pack->GetVersionString();
        if(versionRange.empty() || VersionCmp::RangeCompare(version, versionRange) == 0) {
          pdscMap[pack->GetID()] = localPdscFile;
          found = true;
        }
      }
    }
  }
  return found;
}


XMLTreeElement* RteKernel::ParseLocalRepositoryIdx() const
{
  // Parse local repository index file
  const string indexPath = GetCmsisPackRoot() + "/.Local/local_repository.pidx";

  if (!RteFsUtils::Exists(indexPath)) {
    return nullptr;
  }
  unique_ptr<XMLTree> xmlTree = CreateUniqueXmlTree(nullptr);
  if (!xmlTree->AddFileName(indexPath, true) || xmlTree->GetChildren().empty()) {
    return nullptr;
  }
  // Parse index file
  XMLTreeElement* indexChild = xmlTree->GetFirstChild("index");
  XMLTreeElement* pIndexChild = indexChild ? indexChild->GetFirstChild("pindex") : nullptr;
  if (!pIndexChild) {
    return nullptr;
  }
  // save filename to process relative paths
  pIndexChild->SetRootFileName(indexPath);
  pIndexChild->Reparent(nullptr); // detach from parent to avoid deletion
  return pIndexChild;
}


unique_ptr<XMLTree> RteKernel::CreateUniqueXmlTree(IXmlItemBuilder* itemBuilder, const std::string& ext) const
{
  bool bYaml = !ext.empty() && (ext == ".yml" || ext == ".yaml");
  XMLTree* pXmlTree = bYaml ? CreateYmlTree(itemBuilder) : CreateXmlTree(itemBuilder);
  unique_ptr<XMLTree> xmlTree(pXmlTree);
  if (xmlTree.get() != nullptr) {
    xmlTree->SetCallback(GetRteCallback());
    xmlTree->Init();
  }
  return xmlTree;
}

unique_ptr<RteItemBuilder> RteKernel::CreateUniqueRteItemBuilder(RteItem* rootParent, PackageState packState, const RteItem& options) const
{
  unique_ptr<RteItemBuilder> builder( new RteItemBuilder(rootParent, packState));

  return builder;
}

YmlTree* RteKernel::CreateYmlTree(IXmlItemBuilder* itemBuilder) const
{
  return new YmlTree(itemBuilder);
}

// End of RteKernel.cpp
