/*
 * Copyright (c) 2020-2024 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "CbuildModel.h"

#include "CbuildCallback.h"
#include "CbuildLayer.h"
#include "CbuildKernel.h"
#include "CbuildUtils.h"

#include "ErrLog.h"

#include "RteCprjProject.h"
#include "CprjFile.h"
#include "RteCprjTarget.h"
#include "RteFsUtils.h"
#include "RteKernel.h"
#include "RteGenerator.h"
#include "RtePackage.h"
#include "RteProject.h"
#include "RteUtils.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <regex>

#define EOL "\n"        // End of line

using namespace std;


CbuildModel::CbuildModel() {
}

CbuildModel::~CbuildModel() {
}

bool CbuildModel::Create(const CbuildRteArgs& args) {
  // load cprj file
  CbuildKernel::Get()->SetCmsisPackRoot(args.rtePath);
  m_cprjProject = CbuildKernel::Get()->LoadCprj(args.file, args.toolchain, true, args.updateRteFiles);
  if (!m_cprjProject)
    return false;

  m_updateRteFiles = args.updateRteFiles;

  // init paths
  Init(args.file, args.rtePath);
  m_cprjProject->SetProjectPath(m_prjFolder);

  // get cprj pack structure
  m_cprj = m_cprjProject->GetCprjFile();

  // check cprj schema version
  const string& schemaVersion = m_cprj->GetAttribute("schemaVersion");
  if (VersionCmp::Compare(schemaVersion, SCHEMA_VERSION) < 0) {
    LogMsg("M220", VAL("VER", schemaVersion), VAL("REQ", SCHEMA_VERSION));
    return false;
  }

  // check pack requirements (packlist command)
  if (args.checkPack) {
    vector<CbuildPackItem> packList;
    if (!CbuildProject::CheckPackRequirements(m_cprj, args.rtePath, packList))
      return false;

    if (!packList.empty()) {
      string intdir = CbuildUtils::StrPathConv(args.intDir);
      if (!intdir.empty()) {
        // command line intdir option
        if (fs::path(intdir).is_relative()) {
          intdir = RteFsUtils::GetCurrentFolder() + intdir;
        }
      } else {
        EvalTargetOutput();
        if (!GetIntDir().empty()) {
          // cprj "intdir" field
          intdir = CbuildUtils::StrPathConv(GetIntDir());
          if (fs::path(intdir).is_relative()) {
            intdir = m_prjFolder + intdir;
          }
        } else {
          intdir = RteFsUtils::GetCurrentFolder();
        }
      }
      error_code ec;
      if (!fs::exists(intdir, ec)) {
        fs::create_directories(intdir, ec);
      }
      // generate cpinstall file
      string filename = intdir + (intdir.back() == '/' ? "" : "/") + m_prjName + ".cpinstall";
      ofstream missingPacks(filename);
      for (const auto& pack : packList) {
        const string& packID = pack.vendor + "::" + pack.name + (pack.version.empty() ? "" : "@" + pack.version);
        missingPacks << packID << std::endl;
        LogMsg("M654", VAL("PACK", packID));
      }
      missingPacks.close();
      // generate cpinstall.json file
      filename += ".json";
      missingPacks.open(filename);
      missingPacks << CbuildUtils::GenerateJsonPackList(packList);
      missingPacks.close();
    }
    return true;

  } else {

    // Check if error happened when loading CPRJ
    if (!CbuildKernel::Get()->GetCallback()->GetErrorMessages().empty())
      return false;
  }

  // find toolchain configuration file
  m_compiler = m_cprjProject->GetToolchain();
  m_compilerVersion = m_cprjProject->GetToolchainVersion();
  if (!EvaluateToolchainConfig(m_compiler, m_compilerVersion, args.envVars, args.compilerRoot))
    return false;

  // evaluate device name
  if (!EvalDeviceName())
    return false;

  // create target (resolve)
  if (!CbuildProject(m_cprjProject).CreateTarget(m_targetName.c_str(), m_cprj, args.rtePath, m_compiler))
    return false;

  // get target
  m_cprjTarget = m_cprjProject->GetTarget(m_targetName);
  if (!m_cprjTarget)
    return false;

  // generate audit data
  if (!GenerateAuditData())
    return false;

  // generate cprj with fixed versions
  if (!args.update.empty()) {
    if (!GenerateFixedCprj(args.update))
      return false;
  }

  // evaluate result
  if (!EvaluateResult())
    return false;

  return true;
}

void CbuildModel::Init(const string &file, const string &rtePath) {
  /*
  Init:
  Extract and normalize paths
  */
  m_rtePath = string(rtePath);
  RteFsUtils::NormalizePath(m_rtePath);
  m_rtePath += "/";

  // extract project folder from file
  fs::path prjPath = RteFsUtils::AbsolutePath(file);
  m_prjFolder = prjPath.remove_filename().generic_string();
  RteFsUtils::NormalizePath(m_prjFolder);
  m_prjFolder += "/";
  m_cprjFile = file;

  fs::path name(file);
  m_prjName = name.filename().replace_extension("").generic_string();
  m_targetName = m_cprjProject->GetActiveTargetName();
}

bool CbuildModel::GenerateFixedCprj(const string& update) {
  /*
  GenerateFixedCprj:
  Create project description file with fixed/updated versions
  */
  const RtePackageMap& packs = m_cprjTarget->GetFilteredModel()->GetPackages();
  if (packs.empty()) {
    return false;
  }

  // Get CPRJ elements
  CbuildLayer cprj;
  if (!cprj.InitXml(m_cprjFile)) {
    return false;
  }
  cprj.InitHeaderInfo(m_cprjFile);
  xml_elements* elements = cprj.GetElements();

  // Update Created section
  if (!elements) {
    return false;
  }
  map<string, string> createdAttributes = elements->created->GetAttributes();
  createdAttributes["timestamp"] = cprj.GetTimestamp();
  createdAttributes["tool"] = cprj.GetTool();
  elements->created->SetAttributes(createdAttributes);

  // Compare pack attributes
  for (auto cprjPack : elements->packages->GetChildren()) {
    map<string, string> cprjPackAttributes = cprjPack->GetAttributes();
    for (auto pack : packs) {
      map<string, string> packAttributes = pack.second->GetAttributes();
      if ((cprjPackAttributes["name"] == packAttributes["name"]) &&
          (cprjPackAttributes["vendor"] == packAttributes["vendor"]) &&
          (VersionCmp::RangeCompare(packAttributes["version"], cprjPack->GetAttribute("version")) == 0)) {
        // Set fixed CPRJ pack version
        const string& version = pack.second->GetVersionString();
        cprjPackAttributes["version"] = version + ':' + version;
        cprjPack->SetAttributes(cprjPackAttributes);
      }
    }
  }

  // Get list of CPRJ components
  auto& cprjComponents = elements->components->GetChildren();

  // Iterate over used componentes
  const RteComponentMap components = m_cprjTarget->GetFilteredComponents();
  if (!components.empty()) {
    const map<string, RteFileInstance*> configFiles = m_cprjProject->GetFileInstances();

    for (auto mapElement : components) {
      RteComponent* component = mapElement.second;

      if ((m_cprjTarget->IsComponentUsed(component)) && (!component->IsGenerated())) {

        // Iterate over CPRJ components
        map<string, string> componentAttributes = component->GetAttributes();
        for (auto cprjComponent : cprjComponents) {

          // Compare component attributes: Cclass and Cgroup are required, Csub and Cvendor are optional fields
          map<string, string> cprjComponentAttributes = cprjComponent->GetAttributes();
          const bool csub = cprjComponentAttributes.find("Csub") != cprjComponentAttributes.end();
          const bool cvendor = cprjComponentAttributes.find("Cvendor") != cprjComponentAttributes.end();
          if ((componentAttributes["Cclass" ] == cprjComponentAttributes["Cclass" ]) &&
              (componentAttributes["Cgroup" ] == cprjComponentAttributes["Cgroup" ]) &&
              (!csub    || (componentAttributes["Csub"   ] == cprjComponentAttributes["Csub"   ])) &&
              (!cvendor || (componentAttributes["Cvendor"] == cprjComponentAttributes["Cvendor"]))) {

            // Set fixed CPRJ Component Version
            cprjComponentAttributes["Cversion"] = componentAttributes["Cversion"];
            cprjComponent->SetAttributes(cprjComponentAttributes);

            for (auto configFile : configFiles) {
              if (configFile.second->GetComponent(m_targetName)->Compare(component)) {
                map<string, string> fileAttributes = configFile.second->GetFile(m_targetName)->GetAttributes();
                fileAttributes["name"] = RteUtils::BackSlashesToSlashes(fileAttributes["name"]);

                // Iterate over component files
                bool found = false;
                for (auto file : cprjComponent->GetChildren()) {
                  map<string, string> cprjFileAttributes = file->GetAttributes();
                  if (fileAttributes["name"] == cprjFileAttributes["name"]) {
                    found = true;
                  }
                }
                if (!found) {
                  // Create missing CPRJ config file entry
                  map<string, string> cprjFileAttributes;
                  cprjFileAttributes["category"] = fileAttributes["category"];
                  cprjFileAttributes["attr"] = fileAttributes["attr"];
                  cprjFileAttributes["name"] = fileAttributes["name"];
                  cprjFileAttributes["version"] = fileAttributes["version"];
                  XMLTreeElement* fileElement = cprjComponent->CreateElement("file");
                  fileElement->SetAttributes(cprjFileAttributes);
                }
              }
            }
          }
        }
      }
    }
  }

  // Save CPRJ with fixed versions
  const string& filename = fs::path(update).is_relative() ? m_prjFolder + update : update;
  if (cprj.WriteXmlFile(filename, cprj.GetTree())) {
    LogMsg("M657", VAL("NAME", update));
    return true;
  }
  return false;
}

bool CbuildModel::GenerateAuditData() {
  /*
  GenerateAuditData:
  Create list of used packs, components, APIs and config files
  */
  const RtePackageMap& packs = m_cprjTarget->GetFilteredModel()->GetPackages();
  if (packs.empty()) {
    return false;
  }

  const RteComponentMap components = m_cprjTarget->GetFilteredComponents();
  const map<string, RteApi*> apis = m_cprjTarget->GetFilteredApis();
  if (!components.empty()) {
    const map<string, RteFileInstance*> configFiles = m_cprjProject->GetFileInstances();
    ostringstream auditData;
    for (auto pack : packs) {
      auditData << EOL << EOL << "# Package: " << pack.second->GetPackageID();
      auditData << EOL << "  Location: " << pack.second->GetAbsolutePackagePath();

      for (auto component : components) {
        if ((component.second->GetPackage()->Compare(pack.second)) && (m_cprjTarget->IsComponentUsed(component.second))) {
          auditData << EOL << EOL << "  * Component: " << component.first;

          for (auto configFile : configFiles) {
            const RteComponent *configFileComponent = configFile.second->GetComponent(m_targetName);
            if (configFileComponent && configFileComponent->Compare(component.second)) {
              string instanceName = configFile.second->GetInstanceName();
              RteFsUtils::NormalizePath(instanceName, m_prjFolder);
              auditData << EOL << "    - ConfigFile: " << instanceName << ":" << configFile.second->GetVersionString();
              if (configFile.second->HasNewVersion(m_targetName) > 0) auditData << " [" << configFile.second->GetFile(m_targetName)->GetVersionString() << "]";
            }
          }
        }
      }
      for (auto api : apis) {
        if ((api.second->GetPackage()->Compare(pack.second)) && (m_cprjTarget->IsApiSelected(api.second))) {
          auditData << EOL << EOL << "  * API: " << api.first.substr(2) << ":" << api.second->GetVersionString();
        }
      }
    }
    m_auditData = auditData.str();
  }
  return true;
}

bool CbuildModel::EvaluateResult() {
  /*
  EvaluateResult:
  Extract result info
  */
  if (m_updateRteFiles) {
    if (!GenerateRteHeaders()) {
      return false;
    }
  } else {
    const string rteComponents = m_cprjProject->GetRteHeader("RTE_Components.h", m_cprjTarget->GetName(), m_cprjProject->GetProjectPath());
    if (!RteFsUtils::Exists(rteComponents)) {
      LogMsg("M634", PATH(rteComponents));
    }
  }
  if (!EvalTargetOutput())
    return false;
  if (!EvalConfigFiles())
    return false;
  if (!EvalFlags())
    return false;
  if (!EvalOptions())
    return false;
  if (!EvalPreIncludeFiles())
    return false;

  const RtePackageMap& packs = m_cprjTarget->GetFilteredModel()->GetPackages();
  for (auto pack : packs) {
    m_packs.insert(pack.second->GetPackageID());
  }
  for (auto define : m_cprjTarget->GetDefines()) {
    if (!define.empty()) m_targetDefines.push_back(define);
  }
  for (auto inc : m_cprjTarget->GetIncludePaths()) {
    RteFsUtils::NormalizePath(inc, m_prjFolder);
    if (!RteFsUtils::Exists(inc)) {
      LogMsg("M204", PATH(inc));
      return false;
    }
    if (!inc.empty()) m_targetIncludePaths.push_back(inc);
  }
  for (auto lib : m_cprjTarget->GetLibraries()) {
    RteFsUtils::NormalizePath(lib, m_prjFolder);
    if (!RteFsUtils::Exists(lib)) {
      LogMsg("M204", PATH(lib));
      return false;
    }
    if (!lib.empty()) m_libraries.push_back(lib);
  }
  for (auto obj : m_cprjTarget->GetObjects()) {
    RteFsUtils::NormalizePath(obj, m_prjFolder);
    if (!RteFsUtils::Exists(obj)) {
      LogMsg("M204", PATH(obj));
      return false;
    }
    if (!obj.empty()) m_objects.push_back(obj);
  }

  if (!EvalIncludesDefines()) {
    return false;
  }
  if (!EvalSourceFiles()) {
    return false;
  }
  if (!EvalAccessSequence()) {
    return false;
  }

  return true;
}

bool CbuildModel::GenerateRteHeaders() {
  /*
  GenerateRteHeaders
  Generate Rte Headers Files
  */
  return m_cprjTarget ? m_cprjTarget->GenerateRteHeaders() : false;
}

bool CbuildModel::EvaluateToolchainConfig(const string& name, const string& versionRange, const vector<string>& envVars, const string& compilerRoot) {
  /*
  EvaluateToolchain:
  Search for compatible toolchain configuration file
  */

  // Search registered toolchains and config files in the compiler root
  if (GetCompatibleToolchain(name, versionRange, compilerRoot, envVars)) {
    return true;
  }

  // Error: Toolchain config not found
  LogMsg("M608", VAL("NAME", name), VAL("VER", versionRange));
  return false;
}

bool CbuildModel::GetCompatibleToolchain(const string& name, const string& versionRange, const string& dir, const vector<string>& envVars) {
  // extract toolchain info from environment variables
  map<string, map<string, string>> toolchains;
  for (const auto& envVar : envVars) {
    smatch sm;
    try {
      regex_match(envVar, sm, regex("(\\w+)_TOOLCHAIN_(\\d+)_(\\d+)_(\\d+)=(.*)"));
    } catch (exception&) {};
    if (sm.size() == 6) {
      toolchains[sm[1]][string(sm[2])+'.'+string(sm[3])+'.'+string(sm[4])] = sm[5];
    }
  }

  // get toolchain configuration files
  if (!RteFsUtils::Exists(dir)) {
    return false;
  }
  set<fs::directory_entry> toolchainConfigFiles;
  error_code ec;
  for (auto const& dir_entry : fs::recursive_directory_iterator(dir, ec)) {
    string extn = dir_entry.path().extension().string();
    if (dir_entry.path().extension().string() != CMEXT) {
      continue;
    }
    toolchainConfigFiles.insert(dir_entry);
  }

  // find compatible registered version
  string selectedVersion;
  for (const auto& [toolchainName, versions] : toolchains) {
    if (toolchainName.compare(name) == 0) {
      for (const auto& [version, root] : versions) {
        if ((VersionCmp::RangeCompare(version, versionRange) == 0) &&
          (VersionCmp::Compare(selectedVersion, version) <= 0)) {
          if (RteFsUtils::Exists(root)) {
            // check whether a config file is available for the registered version
            if (GetToolchainConfig(toolchainConfigFiles, toolchainName, RteUtils::GetPrefix(versionRange) + ':' + version)) {
              selectedVersion = version;
            }
          }
        }
      }
    }
  }

  if (selectedVersion.empty()) {
    // no compatible registered toolchain was found
    LogMsg("M616", VAL("NAME", name));
    return false;
  } else {
    // registered toolchain was found
    m_toolchainRegisteredVersion = selectedVersion;
    m_toolchainRegisteredRoot = toolchains[name][selectedVersion];
    RteFsUtils::NormalizePath(m_toolchainRegisteredRoot);
    return true;
  }
  return false;
}

bool CbuildModel::GetToolchainConfig(const set<fs::directory_entry>& files, const string& name, const string& version) {
  // find compatible toolchain configuration file
  string selectedVersion, selectedConfig;
  for (const auto& p : files) {
    smatch sm;
    const string& stem = p.path().stem().generic_string();
    try {
      regex_match(stem, sm, regex("(\\w+)\\.(\\d+\\.\\d+\\.\\d+)"));
    } catch (exception&) {};
    if (sm.size() == 3) {
      const string& configName = sm[1];
      const string& configVersion = sm[2];
      if ((configName.compare(name) == 0) &&
        (VersionCmp::RangeCompare(configVersion, version) <= 0) &&
        (VersionCmp::Compare(selectedVersion, configVersion) <= 0))
      {
        selectedVersion = configVersion;
        selectedConfig = p.path().generic_string();
      }
    }
  }
  if (!selectedVersion.empty()) {
    RteFsUtils::NormalizePath(selectedConfig);
    m_toolchainConfig = selectedConfig;
    m_toolchainConfigVersion = selectedVersion;
    return true;
  }
  return false;
}

bool CbuildModel::EvalPreIncludeFiles() {
  /*
  EvalPreIncludeFiles:
  Evaluate PreInclude Files
  */
  const map<RteComponent*, set<string> >& preincludeFiles = m_cprjTarget->GetPreIncludeFiles();

  for (const auto& [component, files] : preincludeFiles) {
    for (string file : files) {
      const string& preIncludeLocal = component ? component->ConstructComponentPreIncludeFileName() : "";
      const string& baseFolder = (file == "Pre_Include_Global.h" || file == preIncludeLocal) ?
        m_prjFolder + m_cprjProject->GetRteFolder() + "/_" + WildCards::ToX(m_cprjTarget->GetName()) + "/" :
        m_prjFolder;
      RteFsUtils::NormalizePath(file, baseFolder);
      if (!RteFsUtils::Exists(file)) {
        LogMsg("M204", PATH(file));
        return false;
      }
      if (component) {
        const string& componentName = GetExtendedRteGroupName(component, m_cprjProject->GetRteFolder());
        m_preIncludeFilesLocal[componentName].push_back(file);
      } else {
        m_preIncludeFilesGlobal.push_back(file);
      }
    }
  }
  return true;
}

bool CbuildModel::EvalConfigFiles() {
  /*
  EvalConfigFiles:
  Evaluate Configuration Files
  */

  const map<string, RteComponentInstance*>& components = m_cprjProject->GetComponentInstances();

  if (!components.empty()) for (auto itc : components) {
    RteComponentInstance* ci = itc.second;
    if (!ci || !ci->IsUsedByTarget(m_targetName))
      continue;
    if (ci->IsApi())
      continue;

    map<string, RteFileInstance*> compConfigFiles;
    m_cprjProject->GetFileInstancesForComponent(ci, m_targetName, compConfigFiles);
    const string& layer = ci->GetAttribute("layer");
    if (!ci->IsGenerated()) m_layerPackages[layer].insert(ci->GetPackage()->GetID());
    const RteComponentInstance* api = ci->GetApiInstance();
    if (api) m_layerPackages[layer].insert(api->GetPackage()->GetID());

    for (auto fi : compConfigFiles) {
      const string& prjFile = RteUtils::BackSlashesToSlashes(fi.second->GetInstanceName().c_str());
      const string& absPrjFile = m_cprjProject->GetProjectPath() + prjFile;
      const string& pkgFile = RteUtils::BackSlashesToSlashes(fi.second->GetFile(m_targetName)->GetOriginalAbsolutePath());
      error_code ec;
      if (m_updateRteFiles && !fs::exists(absPrjFile, ec)) {
        // Copy config file from pack if it's missing
        LogMsg("M653", VAL("NAME", prjFile));
        string dir = fs::path(absPrjFile).remove_filename().generic_string();
        fs::create_directories(dir, ec);
        if (ec) {
          LogMsg("M211", PATH(dir));
          return false;
        }
        fs::copy_file(pkgFile, absPrjFile, fs::copy_options::overwrite_existing, ec);
        if (ec) {
          LogMsg("M208", VAL("ORIG", pkgFile), VAL("DEST", absPrjFile));
          return false;
        }
      }
      if (fi.second->HasNewVersion(m_targetName) > 0) {
        m_configFiles.insert(pair<string, string>(absPrjFile, pkgFile));
      }
      m_layerFiles[layer].insert(prjFile);
    }
  }
  return true;
}

bool CbuildModel::EvalSourceFiles() {
  /*
  EvalSourceFiles:
  Evaluate Source Files
  */

  // rte c/cpp and asm source files
  if (!EvalRteSourceFiles(m_cSourceFiles, m_cxxSourceFiles, m_asmSourceFiles, m_linkerScript)) {
    return false;
  }

  // non-rte source files
  if (!EvalNonRteSourceFiles()) {
    return false;
  }

  // source files from gpdsc
  if (!EvalGeneratedSourceFiles()) {
    return false;
  }

  return true;
}

bool CbuildModel::EvalNonRteSourceFiles() {
  /*
  EvalNonRteSourceFiles:
  Evaluate User Source Files
  */

  // non-rte source files
  RteItem* files = m_cprj->GetItemByTag("files");
  if (files) {
    if (!EvalItem(files))
      return false;
  }
  return true;
}

bool CbuildModel::EvalItem(RteItem* item, const string& groupName, const string& groupLayer) {
  /*
  EvalItem:
  Evaluate User Source Files
  */
  const string& tag = item->GetTag();
  if (tag == "file") {
    // file
    string filepath;
    if (!EvalFile(item, groupName, m_prjFolder, filepath))
      return false;
    const string& fileLayer = item->GetAttribute("layer");
    m_layerFiles[fileLayer.empty() ? groupLayer : fileLayer].insert(item->GetName());
    return true;
  }

  string subGroupName;
  if (tag == "group") {
    // group
    subGroupName = groupName + "/" + item->GetName();
  } else if (tag == "files") {
    // files (root group)
    subGroupName = "Files";
  } else {
    return true;
  }

  const string& subGroupLayer = item->GetAttribute("layer");
  for (auto subItem : item->GetChildren()) {
    if (!EvalItem(subItem, subGroupName, (subGroupLayer.empty() ? groupLayer : subGroupLayer)))
      return false;
  }
  return true;
}

bool CbuildModel::EvalGeneratedSourceFiles() {
  /*
  EvalGeneratedSourceFiles:
  Evaluate Generated Source Files
  */

  // add generator's project files if any
  for (auto gi : m_cprjProject ->GetGpdscInfos()) {
    const RtePackage* gpdscPack = gi.second->GetGpdscPack();
    if (!gpdscPack) {
      continue;
    }
    const RteGenerator* gen = gi.second->GetGenerator();
    if(gen) {
      // gpdsc
      const string& gpdscName = gpdscPack->GetPackageFileName();
      const string& gpdscPath = gpdscPack->GetAbsolutePackagePath();
      const RteItem* components = gpdscPack->GetComponents();
      const RteItem* c = components ? components->GetFirstChild("component") : nullptr;
      const RteComponentInstance* ci = c ? m_cprjProject->GetComponentInstance(c->GetID()) : nullptr;
      const string& layer = ci ? ci->GetAttribute("layer") : RteUtils::EMPTY_STRING;

      m_layerFiles[layer].insert(RteFsUtils::RelativePath(gpdscName, m_prjFolder));

      // gpdsc <components> section
      if (components) {
        for (const RteItem* item : components->GetChildren()) {
          Collection<RteItem*> files;
          if (item->GetTag() == "component") {
            files = item->GetGrandChildren("files");
          } else if (item->GetTag() == "bundle") {
            for (const RteItem* bundledComponent : item->GetChildren()) {
              const Collection<RteItem*>& bundledComponentFiles = bundledComponent->GetGrandChildren("files");
              files.insert(files.end(), bundledComponentFiles.begin(), bundledComponentFiles.end());
            }
          }
          for (const RteItem* file : files) {
            if (file->GetTag() != "file") {
              continue;
            }
            string filepath = file->GetName();
            RteFsUtils::NormalizePath(filepath, gpdscPath);
            if (!RteFsUtils::Exists(filepath)) {
              LogMsg("M204", PATH(filepath));
              return false;
            }
            m_layerFiles[layer].insert(RteFsUtils::RelativePath(filepath, m_prjFolder));
          }
        }
      }

      // gpdsc <project_files> section
      const RteFileContainer* genFiles = gen->GetProjectFiles();
      if (!genFiles) {
        continue;
      }
      string grpName = genFiles->GetHierarchicalGroupName();
      if (grpName.empty()) {
        grpName = "Common Sources";
      }
      for(auto file : genFiles->GetChildren()){
        if (file->GetTag() != "file") {
          continue;
        }
        string filepath;
        if (!EvalFile(file, CbuildUtils::ReplaceColon(grpName), gpdscPath, filepath))
          return false;
        m_layerFiles[layer].insert(RteFsUtils::RelativePath(filepath, m_prjFolder));
      }
    }
  }
  return true;
}

bool CbuildModel::EvalFile(RteItem* file, const string& group, const string& base, string& filepath) {
  /*
  EvalFile:
  Evaluate File
  */

  RteFile* f = dynamic_cast<RteFile*>(file);
  if (!f)
    return false;
  filepath = f->GetName();
  const RteFile::Category& cat = CbuildUtils::GetFileType(f->GetCategory(), filepath);
  if (cat == RteFile::Category::HEADER) {
    const string & path = f->GetAttribute("path");
    filepath = (path.empty() ? fs::path(filepath).remove_filename().generic_string() : path);
  }
  RteFsUtils::NormalizePath(filepath, base);
  if (!RteFsUtils::Exists(filepath)) {
    LogMsg("M204", PATH(filepath));
    return false;
  }
  switch (cat) {
  case RteFile::Category::SOURCE_C:
    m_cSourceFiles[group].push_back(filepath);
    break;
  case RteFile::Category::SOURCE_CPP:
    m_cxxSourceFiles[group].push_back(filepath);
    break;
  case RteFile::Category::SOURCE_ASM:
    m_asmSourceFiles[group].push_back(filepath);
    break;
  case RteFile::Category::HEADER:         // header
    m_targetIncludePaths.push_back(filepath);
    break;
  case RteFile::Category::LIBRARY:        // library
    m_libraries.push_back(filepath);
    break;
  case RteFile::Category::OBJECT:         // object
    m_objects.push_back(filepath);
    break;
  case RteFile::Category::LINKER_SCRIPT:
    if (m_linkerScript.empty()) {
      m_linkerScript = filepath;
    }
    break;
  default:
    break;
  }
  return true;
}

bool CbuildModel::EvalDeviceName() {
  /*
  EvalDeviceName:
  Evaluate Device Name
  */

  const RteItem *target = m_cprj->GetItemByTag("target");
  if (!target) {
    // Missing <target> element
    LogMsg("M609", VAL("NAME", "target"));
    return false;
  }
  m_deviceName = target->GetAttribute("Dname");
  if (m_deviceName.empty()) {
    // Missing <target Dname> element
    LogMsg("M609", VAL("NAME", "target Dname"));
    return false;
  }
  return true;
}

vector<string> CbuildModel::SplitArgs(const string& args, const string& delim, bool relativePath) {
  /*
  SplitArgs:
  Split arguments from the string 'args' into a set of strings
  If 'relativePath' is true relative paths are normalized
  */

  vector<string> s;
  size_t end = 0, start = 0, len = args.length();
  while (end < len) {
    if ((end = args.find(delim, start)) == string::npos) end = len;
    string flag = args.substr(start, end - start);
    if (relativePath)
      flag = CbuildUtils::StrPathAbsolute(flag, m_prjFolder);
    s.push_back(flag);
    start = end + 1;
  }
  return s;
}

vector<string> CbuildModel::MergeArgs(const vector<string>& add, const vector<string>& remove, const vector<string>& reference, bool front) {
  /*
  MergeArgs:
   - Merge 'add' arguments from 'reference'
   - Remove 'remove' items from 'reference'
  */
  if (add.empty() && remove.empty()) {
    return {};
  }

  vector<string> list = reference;
  if (front) {
    list.insert(list.begin(), add.begin(), add.end());
  } else {
    list.insert(list.end(), add.begin(), add.end());
  }
  for (auto rem_item : remove) {
    auto first_match = std::find_if(list.cbegin(), list.cend(), [&rem_item](string item) {
      // remove if matches e.g. DEF or DEF=1
      return (item == rem_item || item.substr(0, item.find("=")) == rem_item);
      });
    if (first_match != list.cend()) {
      list.erase(first_match);
    }
  }
  return list;
}

const string CbuildModel::GetParentName(const RteItem* item) {
  /*
  GetParentName
  Get (nested) parent group name
  */

  RteItem* parent = item->GetParent();
  string parentName;
  while (parent) {
    const string& tag = parent->GetTag();
    string name;
    if (tag == "group") {
      name = parent->GetName();
    } else if (tag == "files") {
      name = "Files";
    } else {
      break;
    }
    parentName = name + (parentName.empty() ? "" : "/" + parentName);
    parent = parent->GetParent();
  }
  return parentName;
}

const vector<string>& CbuildModel::GetParentTranslationControls(const RteItem* item, map<string, vector<string>>& transCtrlMap, const vector<string>& targetTransCtrls) {
  /*
  GetParentTranslationControls
  Get next non-empty parent group or target translation control
  */

  string parentName = GetParentName(item);
  while (!parentName.empty()) {
    if (transCtrlMap.find(parentName) != transCtrlMap.end()) {
      const vector<string>& properties = transCtrlMap.at(parentName);
      if (!properties.empty()) {
        return properties;
      }
    }
    size_t delim = parentName.find_last_of('/');
    if (delim == string::npos)
      break;
    parentName = parentName.substr(0, delim);
  }
  return targetTransCtrls;
}

bool CbuildModel::SetItemFlags(const RteItem* item, const string& name) {
  /*
  SetItemFlags:
  - Set 'add' and 'remove' attributes for C/C++/AS flags
  */

  const RteItem* cflags = CbuildUtils::GetItemByTagAndAttribute(item->GetChildren(), "cflags", "compiler", m_compiler);
  const RteItem* cxxflags = CbuildUtils::GetItemByTagAndAttribute(item->GetChildren(), "cxxflags", "compiler", m_compiler);
  const RteItem* asflags = CbuildUtils::GetItemByTagAndAttribute(item->GetChildren(), "asflags", "compiler", m_compiler);

  if (cflags != NULL) {
    const vector<string>& parentFlags = GetParentTranslationControls(item, m_CFlags, m_targetCFlags);
    const vector<string>& flagsList = MergeArgs(SplitArgs(cflags->GetAttribute("add")), SplitArgs(cflags->GetAttribute("remove")), parentFlags);
    m_CFlags.insert(pair<string, vector<string>>(name, flagsList));
  }
  if (cxxflags != NULL) {
    const vector<string>& parentFlags = GetParentTranslationControls(item, m_CxxFlags, m_targetCxxFlags);
    const vector<string>& flagsList = MergeArgs(SplitArgs(cxxflags->GetAttribute("add")), SplitArgs(cxxflags->GetAttribute("remove")), parentFlags);
    m_CxxFlags.insert(pair<string, vector<string>>(name, flagsList));
  }
  if (asflags != NULL) {
    bool inheritanceBreak = false;
    const string& use = asflags->GetAttribute("use");
    if (!use.empty()) {
      const string& parentName = GetParentName(item);
      const bool assembler = (use == "armasm" || use == "gas");
      const bool parentAsm = m_Asm.find(parentName) != m_Asm.end() ? m_Asm.at(parentName) :
                          m_Asm.find("") != m_Asm.end() ? m_Asm.at("") : false;
      inheritanceBreak = (assembler != parentAsm);
      if (inheritanceBreak) {
        m_Asm.insert(pair<string, bool>(name, assembler));
      }
    }
    vector<string> flagsList;
    if (inheritanceBreak) {
      flagsList = SplitArgs(asflags->GetAttribute("add"));
    } else {
      const vector<string>& parentFlags = GetParentTranslationControls(item, m_AsFlags, m_targetAsFlags);
      flagsList = MergeArgs(SplitArgs(asflags->GetAttribute("add")), SplitArgs(asflags->GetAttribute("remove")), parentFlags);
    }
    m_AsFlags.insert(pair<string, vector<string>>(name, flagsList));
  }
  return true;
}

bool CbuildModel::SetItemOptions(const RteItem* item, const string& name) {
  const RteItem *options = item->GetItemByTag("options");
  if (!options) {
    return true; // options are optional
  }

  string optimize = options->GetAttribute("optimize");
  m_optimize.insert(pair<string, string>(name, optimize));

  string debug = options->GetAttribute("debug");
  m_debug.insert(pair<string, string>(name, debug));

  string warnings = options->GetAttribute("warnings");
  m_warnings.insert(pair<string, string>(name, warnings));

  string languageC = options->GetAttribute("languageC");
  m_languageC.insert(pair<string, string>(name, languageC));

  string languageCpp = options->GetAttribute("languageCpp");
  m_languageCpp.insert(pair<string, string>(name, languageCpp));
  return true;
}

bool CbuildModel::SetItemIncludesDefines(const RteItem* item, const string& name) {
  /*
  SetItemIncludesDefines:
  - Set 'define' and 'undefine' attributes
  - Set 'include' and 'exclude' attributes
  */
  if (!item) {
    return false;
  }

  const RteItem* defines   = item->GetItemByTag("defines");
  const RteItem* undefines = item->GetItemByTag("undefines");
  const RteItem* includes  = item->GetItemByTag("includes");
  const RteItem* excludes  = item->GetItemByTag("excludes");

  // Set Defines
  if (defines != nullptr || undefines != nullptr) {
    const auto& definesList = (defines ? SplitArgs(defines->GetText(), ";", false) : vector<string>{});
    const auto& undefinesList = (undefines ? SplitArgs(undefines->GetText(), ";", false) : vector<string>{});
    const auto& parentDefines = GetParentTranslationControls(item, m_defines, m_targetDefines);
    const auto& resDefinesList = MergeArgs(definesList, undefinesList, parentDefines);
    m_defines.insert(pair<string, vector<string>>(name, resDefinesList));
  }
  // Set Includes
  if (excludes != nullptr || includes != nullptr) {
    auto excludesList = (excludes ? SplitArgs(excludes->GetText(), ";", false) : vector<string>{});
    for (auto& exclude : excludesList) {
      if (CbuildUtils::NormalizePath(exclude, m_prjFolder)) {
        continue;
      }
      regex regEx{ "^\\$.*\\$$" };
      if (regex_search(exclude, regEx)) {
        continue;
      }
      LogMsg("M204", PATH(exclude));
      return false;
    }
    const auto& parentIncludes = GetParentTranslationControls(item, m_includePaths, m_targetIncludePaths);
    auto includesList = (includes ? SplitArgs(includes->GetText(), ";", false) : vector<string>{});
    for (auto& include : includesList) {
      if (CbuildUtils::NormalizePath(include, m_prjFolder)) {
        continue;
      }
      regex regEx{ "^\\$.*\\$$" };
      if (regex_search(include, regEx)) {
        continue;
      }
      LogMsg("M204", PATH(include));
      return false;
    }
    const auto& resIncludesList = MergeArgs(includesList, excludesList, parentIncludes, true);
    m_includePaths.insert(pair<string, vector<string>>(name, resIncludesList));
  }
  return true;
}

bool CbuildModel::EvalIncludesDefines() {
  /*
  EvalIncludesDefines:
  Evaluate User Includes and Defines
  */

  // Target includes and defines
  const RteItem* target = m_cprj->GetItemByTag("target");
  if (target) {
    const RteItem* includes = target->GetItemByTag("includes");
    const RteItem* defines = target->GetItemByTag("defines");
    if (includes) {
      const vector<string>& includesList = SplitArgs(includes->GetText(), ";", false);
      vector<string> normalizedIncludesList;
      for (auto include : includesList) {
        if (CbuildUtils::NormalizePath(include, m_prjFolder)) {
          normalizedIncludesList.push_back(include);
          continue;
        }
        regex regEx{ "^\\$.*\\$$" };
        if (regex_search(include, regEx)) {
          normalizedIncludesList.push_back(include);
          continue;
        }
        LogMsg("M204", PATH(include));
        return false;
      }
      m_targetIncludePaths.insert(m_targetIncludePaths.begin(), normalizedIncludesList.begin(), normalizedIncludesList.end());
    }
    if (defines) {
      const vector<string>& definesList = SplitArgs(defines->GetText(), ";", false);
      for (auto define : definesList) {
        if (!define.empty()) {
          m_targetDefines.push_back(define);
        }
      }
    }
  }

  // component defines/includes
  const RteItem* components = m_cprj->GetItemByTag("components");
  if (components) {
    for (auto ci : components->GetChildren()) {
      const string& componentName = GetExtendedRteGroupName(ci, m_cprjProject->GetRteFolder());
      if (!SetItemIncludesDefines(ci, componentName)) {
        return false;
      }
    }
  }

  // files defines/includes
  const RteItem* files = m_cprj->GetItemByTag("files");
  if (files) {
    if (!EvalItemTranslationControls(files, DEFINES)) {
      return false;
    }
  }

  return true;
}

bool CbuildModel::EvalFlags() {
  /*
  EvalFlags:
  Evaluate Flags
  */

  // Target flags
  const RteItem* target = m_cprj->GetItemByTag("target");
  if (target) {
    const RteItem* cflags = CbuildUtils::GetItemByTagAndAttribute(target->GetChildren(), "cflags", "compiler", m_compiler);
    const RteItem* cxxflags = CbuildUtils::GetItemByTagAndAttribute(target->GetChildren(), "cxxflags", "compiler", m_compiler);
    const RteItem* asflags = CbuildUtils::GetItemByTagAndAttribute(target->GetChildren(), "asflags", "compiler", m_compiler);
    if (cflags != NULL) m_targetCFlags = SplitArgs(cflags->GetAttribute("add"));
    if (cxxflags != NULL) m_targetCxxFlags = SplitArgs(cxxflags->GetAttribute("add"));
    if (asflags != NULL) {
      m_targetAsFlags = SplitArgs(asflags->GetAttribute("add"));
      string use = asflags->GetAttribute("use");
      if (!use.empty()) m_Asm.insert(pair<string, bool>("", use == "armasm" || use == "gas"));
    }
    const RteItem* ldflags = CbuildUtils::GetItemByTagAndAttribute(target->GetChildren(), "ldflags", "compiler", m_compiler);
    const RteItem* ldcflags = CbuildUtils::GetItemByTagAndAttribute(target->GetChildren(), "ldcflags", "compiler", m_compiler);
    const RteItem* ldcxxflags = CbuildUtils::GetItemByTagAndAttribute(target->GetChildren(), "ldcxxflags", "compiler", m_compiler);
    if (ldflags != NULL) {
      m_targetLdFlags = SplitArgs(ldflags->GetAttribute("add"));
      m_linkerScript = ldflags->GetAttribute("file");
      if (!m_linkerScript.empty()) {
        RteFsUtils::NormalizePath(m_linkerScript, m_prjFolder);
        if (!RteFsUtils::Exists(m_linkerScript)) {
          LogMsg("M204", PATH(m_linkerScript));
          return false;
        }
        const RteItem* layers = m_cprj->GetItemByTag("layers");
        if (layers) for (auto layer : layers->GetChildren()) {
          if (layer->GetAttributeAsBool("hasTarget")) {
            m_layerFiles[layer->GetAttribute("name")].insert(RteFsUtils::RelativePath(m_linkerScript, m_prjFolder));
          }
        }
      }
      m_linkerRegionsFile = ldflags->GetAttribute("regions");
      if (!m_linkerRegionsFile.empty()) {
        RteFsUtils::NormalizePath(m_linkerRegionsFile, m_prjFolder);
        if (!RteFsUtils::Exists(m_linkerRegionsFile)) {
          LogMsg("M204", PATH(m_linkerRegionsFile));
          return false;
        }
      }
      const RteItem* defines = ldflags->GetItemByTag("defines");
      if (defines) {
        const vector<string>& definesList = SplitArgs(defines->GetText(), ";", false);
        for (auto define : definesList) {
          if (!define.empty()) {
            m_linkerPreProcessorDefines.push_back(define);
          }
        }
      }
    }
    if (ldcflags != NULL)
    {
      m_targetLdCFlags = SplitArgs(ldcflags->GetAttribute("add"));
    }
    if (ldcxxflags != NULL)
    {
      m_targetLdCxxFlags = SplitArgs(ldcxxflags->GetAttribute("add"));
    }
    const RteItem* ldlibs = CbuildUtils::GetItemByTagAndAttribute(target->GetChildren(), "ldlibs", "compiler", m_compiler);
    if (ldlibs != NULL)
    {
      m_targetLdLibs = SplitArgs(ldlibs->GetAttribute("add"));
    }
  }

  // RTE group flags
  const RteItem* components = m_cprj->GetItemByTag("components");
  if (components) {
    for (auto ci : components->GetChildren()) {
      const string& componentName = GetExtendedRteGroupName(ci, m_cprjProject->GetRteFolder());
      SetItemFlags(ci, componentName);
    }
  }

  // User groups/files flags
  const RteItem* files = m_cprj->GetItemByTag("files");
  if (files) {
    if (!EvalItemTranslationControls(files, FLAGS)) {
      return false;
    }
  }

  return true;
}

bool CbuildModel::EvalOptions() {
  /*
  EvalOptions:
  Evaluate compile options (optimize, debug, warnnings ...)
  */

  // Target options
  const RteItem* target = m_cprj->GetItemByTag("target");
  if (target) {
    m_targetOptimize = target->GetChildAttribute("options", "optimize");
    m_targetDebug = target->GetChildAttribute("options", "debug");
    m_targetWarnings = target->GetChildAttribute("options", "warnings");
    m_targetLanguageC = target->GetChildAttribute("options", "languageC");
    m_targetLanguageCpp = target->GetChildAttribute("options", "languageCpp");
  }

  // RTE group options
  const RteItem* components = m_cprj->GetItemByTag("components");
  if (components) {
    for (auto ci : components->GetChildren()) {
      const string& componentName = GetExtendedRteGroupName(ci, m_cprjProject->GetRteFolder());
      SetItemOptions(ci, componentName);
    }
  }

  // User groups/files options
  const RteItem* files = m_cprj->GetItemByTag("files");
  if (files) {
    if (!EvalItemTranslationControls(files, OPTIONS)) {
      return false;
    }
  }

  return true;
}

bool CbuildModel::EvalItemTranslationControls(const RteItem* item, TranslationControlsKind kind, const string& groupName) {
  /*
  EvalItemTranslationControls:
  Evaluate User Groups/Files Flags/defines/includes
  */

  std::function<bool(const RteItem *item, const string &name)> setItem;

  switch (kind){
    case FLAGS :
      setItem = std::bind(&CbuildModel::SetItemFlags, this, std::placeholders::_1, std::placeholders::_2);
      break;
    case DEFINES:
      setItem = std::bind(&CbuildModel::SetItemIncludesDefines, this, std::placeholders::_1, std::placeholders::_2);
      break;
    case OPTIONS:
      setItem = std::bind(&CbuildModel::SetItemOptions, this, std::placeholders::_1, std::placeholders::_2);
      break;
    default:
      return false;
  }

  const string& tag = item->GetTag();
  if (tag == "file") {
    string fileName = item->GetName();
    RteFsUtils::NormalizePath(fileName, m_prjFolder);
    if (!setItem(item, fileName)) {
      return false;
    }
    return true;
  }

  string subGroupName;
  if (tag == "group") {
    subGroupName = groupName + "/" + item->GetName();
  } else if (tag == "files") {
    subGroupName = "Files";
  } else {
    return true;
  }

  setItem(item, subGroupName);
  for (auto subItem : item->GetChildren()) {
    if (!EvalItemTranslationControls(subItem, kind, subGroupName)) {
      return false;
    }
  }
  return true;
}

bool CbuildModel::EvalTargetOutput() {
  /*
  EvalTargetOutput:
  Evaluate Target Output
  */

  // Target output
  const RteItem* output = m_cprj->GetItemByTag("target")->GetItemByTag("output");
  if (!output) {
    // Missing <target output> element
    LogMsg("M609", VAL("NAME", "target output"));
    return false;
  }
  m_outputName = output->GetAttribute("name");
  if (m_outputName.empty()) {
    // Missing <target output name> element
    LogMsg("M609", VAL("NAME", "target output name"));
    return false;
  }
  m_outDir = RteUtils::BackSlashesToSlashes(output->GetAttribute("outdir"));
  m_intDir = RteUtils::BackSlashesToSlashes(output->GetAttribute("intdir"));
  m_outputType = output->GetAttribute("type");

  const string typeMap[] = { "elf", "hex", "bin", "lib", "cmse-lib" };
  for (const auto& type : typeMap) {
    const auto& file = output->GetAttribute(type);
    if (!file.empty()) {
      m_outputFiles[type] = file;
    }
  }

  return true;
}

bool CbuildModel::EvalRteSourceFiles(map<string, list<string>> &cSourceFiles, map<string, list<string>> &cxxSourceFiles, map<string, list<string>> &asmSourceFiles, string &linkerScript) {
  /*
  EvalRteSourceFiles:
  Evaluate RTE Source Files
  */

  const map<string, map<string, RteFileInfo>>& grps = m_cprjTarget->GetProjectGroups();
  for (auto grp : grps) {
    for (auto file : grp.second) {
      const RteFile::Category& cat = file.second.m_cat;
      string filepath = file.first;
      RteFsUtils::NormalizePath(filepath, m_cprjProject->GetProjectPath());
      if (!RteFsUtils::Exists(filepath)) {
        LogMsg("M204", PATH(filepath));
        return false;
      }
      // Use extended RTE group name instead of string obtained by RteItem::GetProjectGroupName() implementation("::" + Cclass)
      const string& componentName = GetExtendedRteGroupName(m_cprjTarget->GetComponentInstanceForFile(file.first.c_str()), m_cprjProject->GetRteFolder());
      switch (CbuildUtils::GetFileType(cat, file.first)) {
        case RteFile::Category::SOURCE_C:
          cSourceFiles[componentName].push_back(filepath);
          break;
        case RteFile::Category::SOURCE_CPP:
          cxxSourceFiles[componentName].push_back(filepath);
          break;
        case RteFile::Category::SOURCE_ASM:
          asmSourceFiles[componentName].push_back(filepath);
          break;
        case RteFile::Category::LINKER_SCRIPT:
          if (linkerScript.empty()) {
            linkerScript = filepath;
          }
          break;
        default:
          break;
      }
    }
  }
  return true;
}

string CbuildModel::GetExtendedRteGroupName(RteItem* ci, const string& rteFolder) {
  /*
  GetExtendedRteGroupName:
    Returns Cclass + Cgroup + Csub + Cvariant names
  */

  const string& cClassName = ci->GetCclassName();
  const string& cGroupName = ci->GetCgroupName();
  const string& cSubName = ci->GetCsubName();
  const string& cVariantName = ci->GetCvariantName();
  string rteGroupName = rteFolder;
  if (!cClassName.empty())   rteGroupName += "/" + CbuildUtils::RemoveSlash(cClassName);
  if (!cGroupName.empty())   rteGroupName += "/" + CbuildUtils::RemoveSlash(cGroupName);
  if (!cSubName.empty())     rteGroupName += "/" + CbuildUtils::RemoveSlash(cSubName);
  if (!cVariantName.empty()) rteGroupName += "/" + CbuildUtils::RemoveSlash(cVariantName);
  return rteGroupName;
}

void CbuildModel::InsertVectorPointers(vector<string*>& dst, vector<string>& src) {
  for (auto& item : src) {
    dst.push_back(&item);
  }
}

bool CbuildModel::EvalAccessSequence() {
  vector<string*> fields;
  vector<std::map<std::string, std::vector<std::string>>*> fieldList = {
    &m_defines , &m_includePaths, &m_CFlags, &m_CxxFlags, &m_AsFlags
  };

  // collect pointers to defines, includes and toolchain flags
  for (auto& itemList : fieldList) {
    for (auto& [_, item] : *itemList) {
      InsertVectorPointers(fields, item);
    }
  }
  InsertVectorPointers(fields, m_targetDefines);
  InsertVectorPointers(fields, m_targetIncludePaths);
  InsertVectorPointers(fields, m_targetCFlags);
  InsertVectorPointers(fields, m_targetCxxFlags);
  InsertVectorPointers(fields, m_targetAsFlags);
  InsertVectorPointers(fields, m_targetLdFlags);
  InsertVectorPointers(fields, m_targetLdCFlags);
  InsertVectorPointers(fields, m_targetLdCxxFlags);

  // iterate over every field and evaluate access sequences
  for (auto& item : fields) {
    if (item) {
      size_t offset = 0;
      while (offset != string::npos) {
        string sequence;
        if (!RteUtils::GetAccessSequence(offset, *item, sequence, '$', '$')) {
          return false;
        }
        if (offset != string::npos) {
          string replacement;
          regex regEx;
          if (sequence == "Bpack") {
            regEx = regex("\\$Bpack\\$");
            auto boardName = m_cprjTarget->GetAttribute("Bname");
            if (boardName.empty()) {
              LogMsg("M632", VAL("ATTR", "Bname"), VAL("ACCSEQ", sequence));
              break;
            }
            auto selectedBoard = m_cprjTarget->GetFilteredModel()->FindBoard(boardName);
            if (!selectedBoard) {
              LogMsg("M615", VAL("PROP", "board name"), VAL("VAL", boardName));
              return false;
            }
            replacement = RteUtils::RemoveTrailingBackslash(selectedBoard->GetPackage()->GetAbsolutePackagePath());
          }
          else if (sequence == "Dpack")
          {
            regEx = regex("\\$Dpack\\$");
            auto deviceName = m_cprjTarget->GetAttribute("Dname");
            auto deviceVendor = m_cprjTarget->GetAttribute("Dvendor");
            const auto device = m_cprjTarget->GetModel()->GetDevice(deviceName, deviceVendor);
            if (device) {
              replacement = RteUtils::RemoveTrailingBackslash(device->GetPackage()->GetAbsolutePackagePath());
            }
          }
          else if (sequence == "PackRoot") {
            regEx = regex("\\$PackRoot\\$");
            replacement = RteUtils::RemoveTrailingBackslash(CbuildKernel::Get()->GetCmsisPackRoot());
          }
          else if (regex_match(sequence, regex("Pack\\(.*"))) {
            string packStr, vendor, name, version;
            size_t offsetContext = 0;
            regEx = regex("\\$Pack\\(.*");
            if (!RteUtils::GetAccessSequence(offsetContext, sequence, packStr, '(', ')')) {
              return false;
            }
            string packId = packStr;
            auto packages = m_cprjTarget->GetModel()->GetPackages();
            for (const auto& pack : packages) {
              if (pack.second->GetPackageID(false) == packId || pack.second->GetPackageID() == packId) {
                replacement = RteUtils::RemoveTrailingBackslash(pack.second->GetAbsolutePackagePath());
                break;
              }
            }
            if (replacement.empty()) {
              LogMsg("M632", VAL("ATTR", packStr), VAL("ACCSEQ", sequence));
            }
          }
          else {
            LogMsg("M633", VAL("ACCSEQ", sequence));
          }
          *item = regex_replace(*item, regEx, replacement);
        }
      }
    }
  }

  // remove duplicates
  for (auto& itemList : fieldList) {
    for (auto& [_, item] : *itemList) {
      CollectionUtils::RemoveVectorDuplicates<string>(item);
    }
  }
  CollectionUtils::RemoveVectorDuplicates<string>(m_targetDefines);
  CollectionUtils::RemoveVectorDuplicates<string>(m_targetIncludePaths);
  CollectionUtils::RemoveVectorDuplicates<string>(m_targetCFlags);
  CollectionUtils::RemoveVectorDuplicates<string>(m_targetCxxFlags);
  CollectionUtils::RemoveVectorDuplicates<string>(m_targetAsFlags);
  CollectionUtils::RemoveVectorDuplicates<string>(m_targetLdFlags);
  CollectionUtils::RemoveVectorDuplicates<string>(m_targetLdCFlags);
  CollectionUtils::RemoveVectorDuplicates<string>(m_targetLdCxxFlags);

  return true;
}

const RteTarget* CbuildModel::GetTarget() const
{
  if (m_cprjTarget) {
    return m_cprjTarget;
  } else if (m_cprjProject) {
    return m_cprjProject->GetTarget(m_targetName);
  }
  return nullptr;
}
