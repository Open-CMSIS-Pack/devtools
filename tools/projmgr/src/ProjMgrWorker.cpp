/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgrWorker.h"
#include "ProjMgrLogger.h"
#include "ProjMgrYamlEmitter.h"

#include "CrossPlatformUtils.h"
#include "RteFsUtils.h"

#include <algorithm>
#include <iostream>
#include <regex>

using namespace std;

static const regex accessSequencesRegEx = regex(string("^(") +
  ProjMgrUtils::AS_SOLUTION_DIR + "|" +
  ProjMgrUtils::AS_PROJECT_DIR  + "|" +
  ProjMgrUtils::AS_OUT_DIR      + "|" +
  ProjMgrUtils::AS_BIN          + "|" +
  ProjMgrUtils::AS_ELF          + "|" +
  ProjMgrUtils::AS_HEX          + "|" +
  ProjMgrUtils::AS_LIB          + "|" +
  ProjMgrUtils::AS_CMSE         + ")" +
  "\\((.*)\\)$"
);

static const map<const string, tuple<const string, const string, const string>> affixesMap = {
  { ""   , {ProjMgrUtils::DEFAULT_ELF_SUFFIX, ProjMgrUtils::DEFAULT_LIB_PREFIX, ProjMgrUtils::DEFAULT_LIB_SUFFIX }},
  { "AC6", {ProjMgrUtils::AC6_ELF_SUFFIX    , ProjMgrUtils::AC6_LIB_PREFIX    , ProjMgrUtils::AC6_LIB_SUFFIX     }},
  { "GCC", {ProjMgrUtils::GCC_ELF_SUFFIX    , ProjMgrUtils::GCC_LIB_PREFIX    , ProjMgrUtils::GCC_LIB_SUFFIX     }},
  { "IAR", {ProjMgrUtils::IAR_ELF_SUFFIX    , ProjMgrUtils::IAR_LIB_PREFIX    , ProjMgrUtils::IAR_LIB_SUFFIX     }},
};

ProjMgrWorker::ProjMgrWorker(void) {
  m_loadPacksPolicy = LoadPacksPolicy::DEFAULT;
  RteCondition::SetVerboseFlags(0);
}

ProjMgrWorker::~ProjMgrWorker(void) {
  ProjMgrKernel::Destroy();

  for (auto context : m_contexts) {
    for (auto componentItem : context.second.components) {
      delete componentItem.second.instance;
    }
  }
}

void ProjMgrWorker::SetParser(ProjMgrParser* parser) {
  m_parser = parser;
}

bool ProjMgrWorker::AddContexts(ProjMgrParser& parser, ContextDesc& descriptor, const string& cprojectFile) {
  error_code ec;
  ContextItem context;
  std::map<std::string, CprojectItem>& cprojects = parser.GetCprojects();
  if (cprojects.find(cprojectFile) == cprojects.end()) {
    ProjMgrLogger::Error(cprojectFile, "cproject not parsed, adding context failed");
    return false;
  }
  context.cproject = &cprojects.at(cprojectFile);
  context.cdefault = &parser.GetCdefault();
  context.csolution = &parser.GetCsolution();

  // No build/target-types
  if (context.csolution->buildTypes.empty() && context.csolution->targetTypes.empty()) {
    return AddContext(parser, descriptor, { "" }, cprojectFile, context);
  }

  // No build-types
  if (context.csolution->buildTypes.empty()) {
    for (const auto& targetTypeItem : context.csolution->targetTypes) {
      if (!AddContext(parser, descriptor, {"", targetTypeItem.first}, cprojectFile, context)) {
        return false;
      }
    }
    return true;
  }

  // Add contexts for project x build-type x target-type combinations
  for (const auto& buildTypeItem : context.csolution->buildTypes) {
    for (const auto& targetTypeItem : context.csolution->targetTypes) {
      if (!AddContext(parser, descriptor, {buildTypeItem.first, targetTypeItem.first}, cprojectFile, context)) {
        return false;
      }
    }
  }
  return true;
}

bool ProjMgrWorker::AddContext(ProjMgrParser& parser, ContextDesc& descriptor, const TypePair& type, const string& cprojectFile, ContextItem& parentContext) {
  if (CheckType(descriptor.type, {type})) {
    ContextItem context = parentContext;
    context.type.build = type.build;
    context.type.target = type.target;
    const string& buildType = (!type.build.empty() ? "." : "") + type.build;
    const string& targetType = (!type.target.empty() ? "+" : "") + type.target;
    context.name = context.cproject->name + buildType + targetType;
    context.precedences = false;

    // default directories
    context.directories.cprj = m_outputDir.empty() ? context.cproject->directory : m_outputDir;
    context.directories.intdir = "tmp/" + context.cproject->name + (type.target.empty() ? "" : "/" + type.target) + (type.build.empty() ? "" : "/" + type.build);
    context.directories.outdir = "out/" + context.cproject->name + (type.target.empty() ? "" : "/" + type.target) + (type.build.empty() ? "" : "/" + type.build);
    context.directories.rte = "RTE";

    // customized directories
    if (m_outputDir.empty() && !context.csolution->directories.cprj.empty()) {
      context.directories.cprj = context.csolution->directory + "/" + context.csolution->directories.cprj;
    }
    if (!context.csolution->directories.intdir.empty()) {
      context.directories.intdir = context.csolution->directories.intdir;
    }
    if (!context.csolution->directories.outdir.empty()) {
      context.directories.outdir = context.csolution->directories.outdir;
    }
    if (!context.cproject->rteBaseDir.empty()) {
      context.directories.rte = context.cproject->rteBaseDir;
    }

    error_code ec;
    context.directories.cprj = fs::weakly_canonical(RteFsUtils::AbsolutePath(context.directories.cprj), ec).generic_string();

    // context variables
    context.variables[ProjMgrUtils::AS_SOLUTION] = context.csolution->name;
    context.variables[ProjMgrUtils::AS_PROJECT] = context.cproject->name;
    context.variables[ProjMgrUtils::AS_BUILD_TYPE] = context.type.build;
    context.variables[ProjMgrUtils::AS_TARGET_TYPE] = context.type.target;

    // user defined variables
    auto userVariablesList = {
      context.csolution->target.build.variables,
      context.csolution->buildTypes[type.build].variables,
      context.csolution->targetTypes[type.target].build.variables,
    };
    for (const auto& var : userVariablesList) {
      for (const auto& [key, value] : var) {
        if ((context.variables.find(key) != context.variables.end()) && (context.variables.at(key) != value)) {
          ProjMgrLogger::Warn("variable '" + key + "' redefined from '" + context.variables.at(key) + "' to '" + value + "'");
        }
        context.variables[key] = value;
      }
    }

    // parse clayers
    for (const auto& clayer : context.cproject->clayers) {
      if (clayer.layer.empty()) {
        continue;
      }
      if (CheckContextFilters(clayer.typeFilter, context)) {
        string const& clayerRef = ExpandString(clayer.layer, context.variables);
        string const& clayerFile = fs::canonical(fs::path(cprojectFile).parent_path().append(clayerRef), ec).generic_string();
        if (clayerFile.empty()) {
          if (regex_match(clayer.layer, regex(".*\\$.*\\$.*"))) {
            ProjMgrLogger::Warn(clayer.layer, "variable was not defined for context '" + context.name +"'");
          } else {
            ProjMgrLogger::Error(clayer.layer, "clayer file was not found");
            return false;
          }
        } else {
          if (!parser.ParseClayer(clayerFile, m_checkSchema)) {
            return false;
          }
          context.clayers[clayerFile] = &parser.GetClayers().at(clayerFile);
        }
      }
    }
    ProjMgrUtils::PushBackUniquely(m_ymlOrderedContexts, context.name);
    m_contexts[context.name] = context;
  }
  return true;
}

void ProjMgrWorker::GetContexts(map<string, ContextItem>* &contexts) {
  m_contextsPtr = &m_contexts;
  contexts = m_contextsPtr;
}

void ProjMgrWorker::GetYmlOrderedContexts(vector<string> &contexts) {
  contexts = m_ymlOrderedContexts;
}

void ProjMgrWorker::SetOutputDir(const std::string& outputDir) {
  m_outputDir = outputDir;
}

void ProjMgrWorker::SetSelectedToolchain(const std::string& selectedToolchain) {
  m_selectedToolchain = selectedToolchain;
}

void ProjMgrWorker::SetCheckSchema(bool checkSchema) {
  m_checkSchema = checkSchema;
}

void ProjMgrWorker::SetVerbose(bool verbose) {
  m_verbose = verbose;
}

void ProjMgrWorker::SetDebug(bool debug) {
  m_debug = debug;
}

void ProjMgrWorker::SetLoadPacksPolicy(const LoadPacksPolicy& policy) {
  m_loadPacksPolicy = policy;
}

void ProjMgrWorker::SetEnvironmentVariables(const StrVec& envVars) {
  m_envVars = envVars;
}

bool ProjMgrWorker::GetRequiredPdscFiles(ContextItem& context, const std::string& packRoot, std::set<std::string>& errMsgs) {
  if (!ProcessPackages(context)) {
    return false;
  }
  for (auto packItem : context.packRequirements) {
    // parse required version range
    const auto& pack = packItem.pack;
    const auto& reqVersion = pack.version;
    string reqVersionRange;
    if (!reqVersion.empty()) {
      if (reqVersion.find(">=") != string::npos) {
        reqVersionRange = reqVersion.substr(2);
      } else {
        reqVersionRange = reqVersion + ":" + reqVersion;
      }
    }

    if (packItem.path.empty()) {
      bool bPackFilter = (pack.name.empty() || WildCards::IsWildcardPattern(pack.name));
      auto filteredPackItems = GetFilteredPacks(packItem, packRoot);
      for (const auto& filteredPackItem : filteredPackItems) {
        auto filteredPack = filteredPackItem.pack;
        string packId, pdscFile, localPackId;

        XmlItem attributes({
          {"name",    filteredPack.name},
          {"vendor",  filteredPack.vendor},
          {"version", reqVersionRange},
          });
        // get installed and local pdsc that satisfy the version range requirements
        pdscFile = m_kernel->GetInstalledPdscFile(attributes, packRoot, packId);
        const string& localPdscFile = m_kernel->GetLocalPdscFile(attributes, packRoot, localPackId);
        if (!localPdscFile.empty()) {
          const size_t packIdLen = (filteredPack.vendor + '.' + filteredPack.name + '.').length();
          if (pdscFile.empty() ? true : VersionCmp::Compare(localPackId.substr(packIdLen), packId.substr(packIdLen)) >= 0) {
            // local pdsc takes precedence
            pdscFile = localPdscFile;
          }
        }
        if (pdscFile.empty()) {
          if (!bPackFilter) {
            std::string packageName =
              (filteredPack.vendor.empty() ? "" : filteredPack.vendor + "::") +
              filteredPack.name +
              (reqVersion.empty() ? "" : "@" + reqVersion);
            errMsgs.insert("required pack: " + packageName + " not installed");
            context.missingPacks.push_back(filteredPack);
          }
          continue;
        }
        context.pdscFiles.insert({ pdscFile, {"", reqVersionRange }});
      }
      if (bPackFilter && context.pdscFiles.empty()) {
        std::string filterStr = pack.vendor +
          (pack.name.empty() ? "" : "::" + pack.name) +
          (reqVersion.empty() ? "" : "@" + reqVersion);
        errMsgs.insert("no match found for pack filter: " + filterStr);
      }
    } else {
      string packPath = packItem.path;
      RteFsUtils::NormalizePath(packPath, context.csolution->directory + "/");
      if (!RteFsUtils::Exists(packPath)) {
        errMsgs.insert("pack path: " + packItem.path + " does not exist");
        break;
      }
      string pdscFile = pack.vendor + '.' + pack.name + ".pdsc";
      RteFsUtils::NormalizePath(pdscFile, packPath + "/");
      if (!RteFsUtils::Exists(pdscFile)) {
        errMsgs.insert("pdsc file was not found in: " + packItem.path);
        break;
      } else {
        context.pdscFiles.insert({ pdscFile, {packPath, reqVersionRange}});
      }
    }
  }
  return (0 == errMsgs.size());
}

string ProjMgrWorker::GetPackRoot() {
  error_code ec;
  string packRoot;
  packRoot = CrossPlatformUtils::GetEnv("CMSIS_PACK_ROOT");
  if (packRoot.empty()) {
    packRoot = CrossPlatformUtils::GetDefaultCMSISPackRootDir();
  }
  packRoot = fs::weakly_canonical(fs::path(packRoot), ec).generic_string();
  return packRoot;
}

bool ProjMgrWorker::InitializeModel() {
  m_packRoot = GetPackRoot();
  m_kernel = ProjMgrKernel::Get();
  if (!m_kernel) {
    ProjMgrLogger::Error("initializing RTE Kernel failed");
    return false;
  }
  m_model = m_kernel->GetGlobalModel();
  if (!m_model) {
    ProjMgrLogger::Error("initializing RTE Model failed");
    return false;
  }
  m_kernel->SetCmsisPackRoot(m_packRoot);
  m_model->SetCallback(m_kernel->GetCallback());
  return true;
}

bool ProjMgrWorker::LoadAllRelevantPacks() {
  // Get required pdsc files
  std::list<std::string> pdscFiles;
  std::set<std::string> errMsgs;
  if (m_selectedContexts.empty()) {
    for (const auto& [context,_] : m_contexts) {
      m_selectedContexts.push_back(context);
    }
  }
  for (const auto& context : m_selectedContexts) {
    auto& contextItem = m_contexts.at(context);
    if (!GetRequiredPdscFiles(contextItem, m_packRoot, errMsgs)) {
      std::for_each(errMsgs.begin(), errMsgs.end(), [](const auto& errMsg) {ProjMgrLogger::Error(errMsg); });
      return false;
    }
    for (const auto& [pdscFile, _] : contextItem.pdscFiles) {
      ProjMgrUtils::PushBackUniquely(pdscFiles, pdscFile);
    }
  }
  // Check load packs policy
  if (pdscFiles.empty() && (m_loadPacksPolicy == LoadPacksPolicy::REQUIRED)) {
    ProjMgrLogger::Error("required packs must be specified");
    return false;
  }
  // Get installed packs
  if (pdscFiles.empty() || (m_loadPacksPolicy == LoadPacksPolicy::ALL) || (m_loadPacksPolicy == LoadPacksPolicy::LATEST)) {
    const bool latest = (m_loadPacksPolicy == LoadPacksPolicy::LATEST) || (m_loadPacksPolicy == LoadPacksPolicy::DEFAULT);
    if (!m_kernel->GetInstalledPacks(pdscFiles, latest)) {
      ProjMgrLogger::Error("parsing installed packs failed");
      return false;
    }
  }
  if (!m_kernel->LoadAndInsertPacks(m_loadedPacks, pdscFiles)) {
    ProjMgrLogger::Error("failed to load and insert packs");
    return CheckRteErrors();
  }
  if (!m_model->Validate()) {
    RtePrintErrorVistior visitor(m_kernel->GetCallback());
    m_model->AcceptVisitor(&visitor);
    return CheckRteErrors();
  }
  return true;
}

bool ProjMgrWorker::LoadPacks(ContextItem& context) {
  if (!InitializeModel()) {
    return false;
  }
  if (m_loadedPacks.empty() && !LoadAllRelevantPacks()) {
    return false;
  }
  if (!InitializeTarget(context)) {
    return false;
  }
  // Filter context specific packs
  if (!context.pdscFiles.empty() && (m_loadPacksPolicy != LoadPacksPolicy::ALL) && (m_loadPacksPolicy != LoadPacksPolicy::LATEST)) {
    set<string> selectedPacks;
    for (const auto& pack : m_loadedPacks) {
      if (context.pdscFiles.find(pack->GetPackageFileName()) != context.pdscFiles.end()) {
        selectedPacks.insert( pack->GetPackageID());
      }
    }
    RtePackageFilter filter;
    filter.SetSelectedPackages(selectedPacks);
    context.rteActiveTarget->SetPackageFilter(filter);
    context.rteActiveTarget->UpdateFilterModel();
  }
  return CheckRteErrors();
}

std::vector<PackageItem> ProjMgrWorker::GetFilteredPacks(const PackageItem& packItem, const string& rtePath) const
{
  std::vector<PackageItem> filteredPacks;
  auto& pack = packItem.pack;
  if (!pack.name.empty() && !WildCards::IsWildcardPattern(pack.name)) {
    filteredPacks.push_back({{ pack.name, pack.vendor, pack.version }});
  }
  else {
    error_code ec;
    string dirName, path;
    path = rtePath + '/' + pack.vendor;
    for (const auto& entry : fs::directory_iterator(path, ec)) {
      if (entry.is_directory()) {
        dirName = entry.path().filename().generic_string();
        if (pack.name.empty() || WildCards::Match(pack.name, dirName)) {
          filteredPacks.push_back({{ dirName, pack.vendor }});
        }
      }
    }
  }
  return filteredPacks;
}

bool ProjMgrWorker::CheckRteErrors(void) {
  const auto& callback = m_kernel->GetCallback();
  const list<string>& rteWarningMessages = callback->GetWarningMessages();
  if (!rteWarningMessages.empty()) {
    string warnMsg = "RTE Model reports:";
    for (const auto& rteWarningMessage : rteWarningMessages) {
      warnMsg += "\n" + rteWarningMessage;
    }
    ProjMgrLogger::Warn(warnMsg);
    callback->ClearWarningMessages();
  }
  const list<string>& rteErrorMessages = callback->GetErrorMessages();
  if (!rteErrorMessages.empty()) {
    string errorMsg = "RTE Model reports:";
    for (const auto& rteErrorMessage : rteErrorMessages) {
      errorMsg += "\n" + rteErrorMessage;
    }
    ProjMgrLogger::Error(errorMsg);
    return false;
  }
  return true;
}

bool ProjMgrWorker::InitializeTarget(ContextItem& context) {
  if (context.rteActiveTarget == nullptr) {
    // RteGlobalModel has the RteProject pointer ownership
    RteProject* rteProject = make_unique<RteProject>().release();
    m_model->AddProject(0, rteProject);
    m_model->SetActiveProjectId(rteProject->GetProjectId());
    context.rteActiveProject = m_model->GetActiveProject();
    const string& targetName = (context.type.build.empty() && context.type.target.empty()) ? "Target 1" :
      context.type.build.empty() ? context.type.target : context.type.build + (context.type.target.empty() ? "" : '+' + context.type.target);
    context.rteActiveProject->AddTarget(targetName, map<string, string>(), true, true);
    context.rteActiveProject->SetActiveTarget(targetName);
    context.rteActiveProject->SetName(context.name);
    context.rteActiveTarget = context.rteActiveProject->GetActiveTarget();
    context.rteFilteredModel = context.rteActiveTarget->GetFilteredModel();
  }
  return CheckRteErrors();
}

bool ProjMgrWorker::SetTargetAttributes(ContextItem& context, map<string, string>& attributes) {
  if (context.rteActiveTarget == nullptr) {
    InitializeTarget(context);
  }
  if (context.cproject) {
    if (!context.cproject->directory.empty()) {
      context.rteActiveProject->SetProjectPath(context.cproject->directory + "/");
    }
    if (!context.directories.rte.empty()) {
      error_code ec;
      const string& rteFolder = fs::relative(context.directories.cprj + "/"+ context.directories.rte, context.cproject->directory, ec).generic_string();
      context.rteActiveProject->SetRteFolder(rteFolder);
    }
  }
  context.rteActiveTarget->SetAttributes(attributes);
  context.rteActiveTarget->UpdateFilterModel();
  return CheckRteErrors();
}

void ProjMgrWorker::GetDeviceItem(const std::string& element, DeviceItem& device) const {
  string deviceInfoStr = element;
  if (!element.empty()) {
    device.vendor = RteUtils::RemoveSuffixByString(deviceInfoStr, "::");
    deviceInfoStr = RteUtils::RemovePrefixByString(deviceInfoStr, "::");
    device.name  = RteUtils::GetPrefix(deviceInfoStr);
    device.pname = RteUtils::GetSuffix(deviceInfoStr);
  }
}

void ProjMgrWorker::GetBoardItem(const std::string& element, BoardItem& board) const {
  string boardId = element;
  if (!boardId.empty()) {
    board.vendor = RteUtils::RemoveSuffixByString(boardId, "::");
    boardId = RteUtils::RemovePrefixByString(boardId, "::");
    board.name = RteUtils::GetPrefix(boardId);
    board.revision = RteUtils::GetSuffix(boardId);
  }
}

bool ProjMgrWorker::GetPrecedentValue(std::string& outValue, const std::string& element) const {
  if (!element.empty()) {
    if (!outValue.empty() && (outValue != element)) {
      ProjMgrLogger::Error("redefinition from '" + outValue + "' into '" + element + "' is not allowed");
      return false;
    }
    outValue = element;
  }
  return true;
}

void ProjMgrWorker::GetAllCombinations(const ConnectionsCollectionMap& src, const ConnectionsCollectionMap::iterator& it,
  std::vector<ConnectionsCollectionVec>& combinations, const ConnectionsCollectionVec& previous) {
  // combine items from a table of 'connections'
  // see an example in the test case ProjMgrWorkerUnitTests.GetAllCombinations
  const auto nextIt = next(it, 1);
  // iterate over the input columns
  for (const auto& item : it->second) {
    ConnectionsCollectionVec combination = previous;
    combination.push_back(item);
    if (nextIt != src.end()) {
      // run recursively over the next item
      GetAllCombinations(src, nextIt, combinations, combination);
    } else {
      // add a new combination containing an item from each column
      combinations.push_back(combination);
    }
  }
}

void ProjMgrWorker::GetAllSelectCombinations(const ConnectPtrVec& src, const ConnectPtrVec::iterator& it,
  // combine items from a vector of 'select' nodes
  // see an example in the test case ProjMgrWorkerUnitTests.GetAllSelectCombinations
  std::vector<ConnectPtrVec>& combinations) {
  // for every past combination add a new combination containing additionally the current item
  for (auto combination : vector<ConnectPtrVec>(combinations)) {
    combination.push_back(*it);
    combinations.push_back(combination);
  }
  // add a new combination with the current item
  combinations.push_back(ConnectPtrVec({*it}));
  const auto nextIt = next(it, 1);
  if (nextIt != src.end()) {
    // run recursively over the next item
    GetAllSelectCombinations(src, nextIt, combinations);
  }
}

bool ProjMgrWorker::CollectLayersFromPacks(ContextItem& context, StrVecMap& clayers) {
  for (const auto& clayerItem : context.rteActiveTarget->GetFilteredModel()->GetLayerDescriptors()) {
    const string& clayerFile = clayerItem->GetOriginalAbsolutePath(clayerItem->GetFileString());
    if (!RteFsUtils::Exists(clayerFile)) {
      return false;
    }
    ProjMgrUtils::PushBackUniquely(clayers[clayerItem->GetTypeString()], clayerFile);
  }
  return true;
}

bool ProjMgrWorker::CollectLayersFromSearchPath(const string& clayerSearchPath, StrVecMap& clayers) {
  if (!clayerSearchPath.empty()) {
    error_code ec;
    const auto& absSearchPath = RteFsUtils::MakePathCanonical(clayerSearchPath);
    if (!RteFsUtils::Exists(absSearchPath)) {
      ProjMgrLogger::Error(absSearchPath, "clayer search path does not exist");
      return false;
    }
    for (auto& item : fs::recursive_directory_iterator(absSearchPath, ec)) {
      if (fs::is_regular_file(item, ec) && (!ec)) {
        const string& clayerFile = item.path().generic_string();
        if (regex_match(clayerFile, regex(".*\\.clayer\\.(yml|yaml)"))) {
          if (!m_parser->ParseGenericClayer(clayerFile, m_checkSchema)) {
            return false;
          }
          ClayerItem* clayer = &m_parser->GetGenericClayers()[clayerFile];
          ProjMgrUtils::PushBackUniquely(clayers[clayer->type], clayerFile);
        }
      }
    }
  }
  return true;
}

bool ProjMgrWorker::DiscoverMatchingLayers(ContextItem& context, const string& clayerSearchPath) {
  // required layer types
  StrVec requiredLayerTypes;
  for (const auto& clayer : context.cproject->clayers) {
    if (clayer.type.empty() || !CheckContextFilters(clayer.typeFilter, context) ||
      (ExpandString(clayer.layer, context.variables) != clayer.layer)) {
      continue;
    }
    requiredLayerTypes.push_back(clayer.type);
  }

  // debug message
  string debugMsg;
  if (m_debug) {
    debugMsg = "check for context '" + context.name + "'\n";
  }

  ConnectionsCollectionVec allConnections;
  StrVecMap matchedTypeClayers;

  if (!requiredLayerTypes.empty()) {
    // collect generic clayers from loaded packs and from search path
    StrVecMap genericClayers;
    if (!CollectLayersFromPacks(context, genericClayers) ||
        !CollectLayersFromSearchPath(clayerSearchPath, genericClayers)) {
      return false;
    }
    // clayers matching required types
    for (const auto& requiredType : requiredLayerTypes) {
      if (genericClayers.find(requiredType) != genericClayers.end()) {
        for (const auto& clayer : genericClayers.at(requiredType)) {
          matchedTypeClayers[requiredType].push_back(clayer);
        }
      } else {
        if (m_debug) {
          debugMsg += "no clayer matches type '" + requiredType + "'\n";
        }
      }
    }
    // parse matched type layers and collect connections
    for (const auto& [type, clayers] : matchedTypeClayers) {
      for (const auto& clayer : clayers) {
        if (!m_parser->ParseGenericClayer(clayer, m_checkSchema)) {
          return false;
        }
        const ClayerItem& clayerItem = m_parser->GetGenericClayers()[clayer];
        if (type != clayerItem.type) {
          if (m_debug) {
            debugMsg += "clayer type '" + clayerItem.type + "' does not match type '" + type + "' in pack description\n";
          }
        }
        // skip non-matching 'for-board' and 'for-device' filters
        if (!CheckBoardDeviceInLayer(context, clayerItem)) {
          continue;
        }
        ConnectionsCollection collection = {clayerItem.path, type};
        for (const auto& connect : clayerItem.connections) {
          collection.connections.push_back(&connect);
        }
        allConnections.push_back(collection);
      }
    }
  }

  // collect connections from project and layers
  CollectConnections(context, allConnections);

  // classify connections according to layer types and set config-ids
  ConnectionsCollectionMap classifiedConnections = ClassifyConnections(allConnections);

  // cross classified connections to get all combinations to be validated
  vector<ConnectionsCollectionVec> combinations;
  if (!classifiedConnections.empty()) {
    GetAllCombinations(classifiedConnections, classifiedConnections.begin(), combinations);
  }

  // validate connections combinations
  for (const auto& combination : combinations) {
    // debug message
    if (m_debug) {
      debugMsg += "\ncheck combined connections:";
      for (const auto& item : combination) {
        const auto& type = m_parser->GetGenericClayers()[item.filename].type;
        debugMsg += "\n  " + item.filename + (type.empty() ? "" : " (layer type: " + type + ")");
        for (const auto& connect : item.connections) {
          debugMsg += "\n    " + (connect->set.empty() ? "" : "set: " + connect->set + " ") + "(" +
            connect->connect + (connect->info.empty() ? "" : " - " + connect->info) + ")";
        }
      }
      debugMsg += "\n";
    }
    // validate connections
    ConnectionsList connections;
    GetConsumesProvides(combination, connections);
    ConnectionsValidationResult result = ValidateConnections(connections);
    if (result.valid) {
      context.validConnections.push_back(combination);
      for (const auto& [type, _] : matchedTypeClayers) {
        for (const auto& item : combination) {
          if (item.type == type) {
            ProjMgrUtils::PushBackUniquely(context.compatibleLayers[type], item.filename);
          }
        }
      }
    }

    // debug message
    if (m_debug) {
      PrintConnectionsValidation(result, debugMsg);
      debugMsg += "connections are " + string(result.valid ? "valid" : "invalid") + "\n";
    }
  }

  // assess generic layers validation results
  if (!matchedTypeClayers.empty()) {
    if (!context.compatibleLayers.empty()) {
      for (const auto& [type, _] : matchedTypeClayers) {
        if (context.compatibleLayers[type].size() == 1) {
          // unique match
          const auto& clayer = context.compatibleLayers[type].front();
          if (m_debug) {
            debugMsg += "\nclayer of type '" + type + "' was uniquely found:\n  " + clayer + "\n";
          }
        } else if (context.compatibleLayers[type].size() > 1) {
          // multiple matches
          if (m_debug) {
            debugMsg += "\nmultiple clayers match type '" + type + "':";
            for (const auto& clayer : context.compatibleLayers[type]) {
              debugMsg += "\n  " + clayer;
            }
            debugMsg += "\n";
          }
        }
      }
    } else {
      // no valid combination
      if (m_debug) {
        debugMsg += "\nno valid combination of clayers was found\n";
      }
    }
  }

  if (m_debug) {
    ProjMgrLogger::Debug(debugMsg);
  }

  if (!matchedTypeClayers.empty() && context.compatibleLayers.empty()) {
    return false;
  }

  if (context.validConnections.size() > 0) {
    // remove redundant sets
    RemoveRedundantSubsets(context.validConnections);
  }

  if (m_verbose || m_debug) {
    // print all valid configuration options
    if (context.validConnections.size() > 0) {
      map<int, map<string, map<string, set<const ConnectItem*>>>> configurationOptions;
      int index = 0;
      for (const auto& combination : context.validConnections) {
        index++;
        for (const auto& item : combination) {
          for (const auto& connect : item.connections) {
            configurationOptions[index][item.type][item.filename].insert(connect);
          }
        }
      }
      for (const auto& [index, types] : configurationOptions) {
        string infoMsg = "valid configuration #" + to_string(index) + ": (context '" + context.name +"')";
        for (const auto& [type, filenames] : types) {
          for (const auto& [filename, options] : filenames) {
            infoMsg += "\n  " + filename + (type.empty() ? "" : " (layer type: " + type + ")");
            for (const auto& connect : options) {
              if (!connect->set.empty()) {
                infoMsg += "\n    set: " + connect->set + " (" + connect->connect + (connect->info.empty() ? "" : " - " + connect->info) + ")";
              }
            }
          }
        }
        ProjMgrLogger::Info(infoMsg + "\n");
      }
    }
  }
  return true;
}

void ProjMgrWorker::PrintConnectionsValidation(ConnectionsValidationResult result, string& msg) {
  if (!result.valid) {
    if (!result.conflicts.empty()) {
      msg += "connections provided with multiple different values:";
      for (const auto& id : result.conflicts) {
        msg += "\n  " + id;
      }
      msg += "\n";
    }
    if (!result.incompatibles.empty()) {
      msg += "required connections not provided:";
      for (const auto& [id, value] : result.incompatibles) {
        msg += "\n  " + id + (value.empty() ? "" : ": " + value);
      }
      msg += "\n";
    }
    if (!result.overflows.empty()) {
      msg += "sum of required values exceed provided:";
      for (const auto& [id, value] : result.overflows) {
        msg += "\n  " + id + (value.empty() ? "" : ": " + value);
      }
      msg += "\n";
    }
  }
}

void ProjMgrWorker::CollectConnections(ContextItem& context, ConnectionsCollectionVec& connections) {
  // collect connections from project and layers
  ConnectionsCollection projectCollection = { context.cproject->path, RteUtils::EMPTY_STRING };
  for (const auto& connect : context.cproject->connections) {
    projectCollection.connections.push_back(&connect);
  }
  connections.push_back(projectCollection);
  for (const auto& [_, clayerItem] : context.clayers) {
    ConnectionsCollection layerCollection = { clayerItem->path, clayerItem->type };
    for (const auto& connect : clayerItem->connections) {
      layerCollection.connections.push_back(&connect);
    }
    connections.push_back(layerCollection);
  }
}

ConnectionsCollectionMap ProjMgrWorker::ClassifyConnections(const ConnectionsCollectionVec& connections) {
  // classify connections according to layer types and set config-ids
  ConnectionsCollectionMap classifiedConnections;
  for (const auto& collectionEntry : connections) {
    // get type classification
    const string& classifiedType = collectionEntry.type.empty() ? to_string(hash<string>{}(collectionEntry.filename)) : collectionEntry.type;
    // group connections by config-id
    map<string, ConnectPtrVec> connectionsMap;
    for (const auto& connect : collectionEntry.connections) {
      const string& configId = connect->set.substr(0, connect->set.find('.'));
      connectionsMap[configId].push_back(connect);
    }
    // get common connections
    ConnectPtrVec commonConnections;
    bool hasMultipleSelect = false;
    for (const auto& [configId, connectionsEntry] : connectionsMap) {
      if (!configId.empty()) {
        // 'config-id' has multiple 'select' choices
        hasMultipleSelect = true;
      } else {
        // 'config-id' has only one 'select'
        commonConnections.insert(commonConnections.end(), connectionsEntry.begin(), connectionsEntry.end());
      }
    }
    // iterate over 'select' choices
    if (hasMultipleSelect) {
      for (const auto& [configId, selectConnections] : connectionsMap) {
        if (!configId.empty()) {
          // combine nodes with identical 'config-id'.'select'
          map<string, ConnectPtrVec> selectMap;
          for (const auto& connect : selectConnections) {
            selectMap[connect->set].push_back(connect);
          }
          for (auto& [_, multipleSelectConnections] : selectMap) {
            vector<ConnectPtrVec> selectCombinations;
            GetAllSelectCombinations(multipleSelectConnections, multipleSelectConnections.begin(), selectCombinations);
            for (const auto& selectCombination : selectCombinations) {
              // insert a classified connections entry
              ConnectionsCollection collection = { collectionEntry.filename, collectionEntry.type, commonConnections };
              collection.connections.insert(collection.connections.end(), selectCombination.begin(), selectCombination.end());
              classifiedConnections[classifiedType + configId].push_back(collection);
            }
          }
        }
      }
    } else {
      // insert a classified connections entry
      ConnectionsCollection collection = { collectionEntry.filename, collectionEntry.type, commonConnections };
      classifiedConnections[classifiedType].push_back(collection);
    }
  }
  return classifiedConnections;
}

void ProjMgrWorker::GetConsumesProvides(const ConnectionsCollectionVec& collection, ConnectionsList& connections) {
  // collect consumed and provided connections
  for (const auto& item : collection) {
    for (const auto& connect : item.connections) {
      for (const auto& consumed : connect->consumes) {
        connections.consumes.push_back(&consumed);
      }
      for (const auto& provided : connect->provides) {
        connections.provides.push_back(&provided);
      }
    }
  }
}

ConnectionsValidationResult ProjMgrWorker::ValidateConnections(ConnectionsList& connections) {
  // elaborate provided list
  StrMap providedValues;
  StrVec conflicts;
  for (const auto& provided : connections.provides) {
    const auto& key = provided->first;
    const auto& value = provided->second;
    if ((providedValues.find(key) != providedValues.end()) && (providedValues.at(key) != value)) {
      // interface is provided with multiple different values
      ProjMgrUtils::PushBackUniquely(conflicts, key);
      continue;
    }
    // new entry
    providedValues[key] = value;
  }

  // elaborate consumed list
  IntMap consumedAddedValues;
  for (auto it = connections.consumes.begin(); it != connections.consumes.end();) {
    const auto& id = (*it)->first;
    const auto& value = (*it)->second;
    if (value.find_first_of('+') == 0) {
      // move entry to the consumedAddedValues map
      consumedAddedValues[id] += ProjMgrUtils::StringToInt(value);
      it = connections.consumes.erase(it);
    } else {
      it++;
    }
  }

  // validate consumedAddedValues against provided values
  StrPairVec overflows;
  for (const auto& [consumedKey, consumedValue] : consumedAddedValues) {
    const int providedValue = providedValues.find(consumedKey) == providedValues.end() ? 0 :
      ProjMgrUtils::StringToInt(providedValues.at(consumedKey));
    if (consumedValue > providedValue) {
      overflows.push_back({ consumedKey, to_string(consumedValue) + " > " + to_string(providedValue) });
    }
  }
  // validate remaining consumedList against provided interface strings
  StrPairVec incompatibles;
  for (const auto& consumed : connections.consumes) {
    const auto& consumedKey = consumed->first;
    if ((providedValues.find(consumedKey) == providedValues.end()) ||
      (!consumed->second.empty() && (consumed->second != providedValues.at(consumedKey)))) {
      incompatibles.push_back({ consumedKey, consumed->second });
    }
  }

  // set results
  bool result = !conflicts.empty() || !overflows.empty() || !incompatibles.empty() ? false : true;
  return { result, conflicts, overflows, incompatibles, connections.provides };
}

bool ProjMgrWorker::ProcessDevice(ContextItem& context) {
  DeviceItem deviceItem;
  GetDeviceItem(context.device, deviceItem);
  if (context.board.empty() && deviceItem.name.empty()) {
    ProjMgrLogger::Error("missing device and/or board info");
    return false;
  }

  RteDeviceItem* matchedBoardDevice = nullptr;
  if(!context.board.empty()) {
    BoardItem boardItem;
    GetBoardItem(context.board, boardItem);
    // find board
    RteBoard* matchedBoard = nullptr;
    const RteBoardMap& availableBoards = context.rteFilteredModel->GetBoards();
    list<RteBoard*> partialMatchedBoards;
    for (const auto& [_, board] : availableBoards) {
      if (board->GetName() == boardItem.name) {
        if (boardItem.vendor.empty() || (boardItem.vendor == DeviceVendor::GetCanonicalVendorName(board->GetVendorName()))) {
          partialMatchedBoards.push_back(board);
        }
      }
    }
    if (partialMatchedBoards.empty()) {
      ProjMgrLogger::Error("board '" + context.board + "' was not found");
      return false;
    }

    if (boardItem.revision.empty() && (partialMatchedBoards.size() == 1)) {
      matchedBoard = partialMatchedBoards.front();
    } else {
      if (boardItem.revision.empty()) {
        // Multiple matches
        string msg = "multiple boards were found for identifier '" + context.board + "'";
        for (const auto& board : partialMatchedBoards) {
          msg += "\n" + board->GetDisplayName() + " in pack " + board->GetPackage()->GetPackageFileName();
        }
        ProjMgrLogger::Error(msg);
        return false;
      }
      for (auto& board : partialMatchedBoards) {
        if (boardItem.revision == board->GetRevision()) {
          matchedBoard = board;
          break;
        }
      }
    }
    if (!matchedBoard) {
      ProjMgrLogger::Error("board '" + context.board + "' was not found");
      return false;
    }

    const auto& boardPackage = matchedBoard->GetPackage();
    context.packages.insert({ ProjMgrUtils::GetPackageID(boardPackage), boardPackage });
    context.targetAttributes["Bname"]    = matchedBoard->GetName();
    context.targetAttributes["Bvendor"]  = matchedBoard->GetVendorName();
    context.targetAttributes["Brevision"] = matchedBoard->GetRevision();
    context.targetAttributes["Bversion"] = matchedBoard->GetRevision(); // deprecated

    // find device from the matched board
    list<RteItem*> mountedDevices;
    matchedBoard->GetMountedDevices(mountedDevices);
    if (mountedDevices.size() > 1) {
      ProjMgrLogger::Error("found multiple mounted devices");
      string msg = "one of the following devices must be specified:";
      for (const auto& device : mountedDevices) {
        msg += "\n" + device->GetDeviceName();
      }
      ProjMgrLogger::Error(msg);
      return false;
    }
    else if (mountedDevices.size() == 0) {
      ProjMgrLogger::Error("found no mounted device");
      return false;
    }

    auto mountedDevice = *(mountedDevices.begin());
    auto device = context.rteFilteredModel->GetDevice(mountedDevice->GetDeviceName(), mountedDevice->GetDeviceVendor());
    if (!device) {
      ProjMgrLogger::Error("board mounted device " + mountedDevice->GetFullDeviceName() + " not found");
      return false;
    }
    matchedBoardDevice = device;
  }

  RteDeviceItem* matchedDevice = nullptr;
  if (deviceItem.name.empty()) {
    matchedDevice = matchedBoardDevice;
    const string& variantName = matchedBoardDevice->GetDeviceVariantName();
    const string& selectableDevice = variantName.empty() ? matchedBoardDevice->GetDeviceName() : variantName;
    context.device = GetDeviceInfoString("", selectableDevice, deviceItem.pname);
  } else {
    list<RteDevice*> devices;
    context.rteFilteredModel->GetDevices(devices, "", "", RteDeviceItem::VARIANT);
    list<RteDeviceItem*> matchedDevices;
    for (const auto& device : devices) {
      if (device->GetFullDeviceName() == deviceItem.name) {
        if (deviceItem.vendor.empty() || (deviceItem.vendor == DeviceVendor::GetCanonicalVendorName(device->GetEffectiveAttribute("Dvendor")))) {
          matchedDevices.push_back(device);
        }
      }
    }
    for (const auto& item : matchedDevices) {
      if ((!matchedDevice) || (VersionCmp::Compare(matchedDevice->GetPackage()->GetVersionString(), item->GetPackage()->GetVersionString()) < 0)) {
        matchedDevice = item;
      }
    }
    if (!matchedDevice) {
      string msg = "specified device '" + deviceItem.name + "' was not found among the installed packs.";
      msg += "\nuse \'cpackget\' utility to install software packs.\n  cpackget add Vendor.PackName --pack-root ./Path/Packs";
      ProjMgrLogger::Error(msg);
      return false;
    }
  }

  // check device variants
  if (matchedDevice->GetDeviceItemCount() > 0) {
    ProjMgrLogger::Error("found multiple device variants");
    string msg = "one of the following device variants must be specified:";
    for (const auto& variant : matchedDevice->GetDeviceItems()) {
      msg += "\n" + variant->GetFullDeviceName();
    }
    ProjMgrLogger::Error(msg);
    return false;
  }

  if (matchedBoardDevice && (matchedBoardDevice != matchedDevice)) {
    const string DeviceInfoString = matchedDevice->GetFullDeviceName();
    const string BoardDeviceInfoString = matchedBoardDevice->GetFullDeviceName();
    if (DeviceInfoString.find(BoardDeviceInfoString) == string::npos) {
      ProjMgrLogger::Warn("specified device '" + DeviceInfoString + "' and board mounted device '" +
        BoardDeviceInfoString + "' are different");
    }
  }

  // check device processors
  const auto& processor = matchedDevice->GetProcessor(deviceItem.pname);
  if (!processor) {
    if (!deviceItem.pname.empty()) {
      ProjMgrLogger::Error("processor name '" + deviceItem.pname + "' was not found");
    }
    string msg = "one of the following processors must be specified:";
    const auto& processors = matchedDevice->GetProcessors();
    for (const auto& p : processors) {
      msg += "\n" + matchedDevice->GetDeviceName() + ":" + p.first;
    }
    ProjMgrLogger::Error(msg);
    return false;
  }

  const auto& processorAttributes = processor->GetAttributes();
  context.targetAttributes.insert(processorAttributes.begin(), processorAttributes.end());
  context.targetAttributes["Dvendor"] = matchedDevice->GetEffectiveAttribute("Dvendor");
  context.targetAttributes["Dname"] = matchedDevice->GetFullDeviceName();

  if (!context.controls.processed.processor.fpu.empty()) {
    if (context.controls.processed.processor.fpu == "on") {
      context.targetAttributes["Dfpu"] = "FPU";
    } else if (context.controls.processed.processor.fpu == "off") {
      context.targetAttributes["Dfpu"] = "NO_FPU";
    }
  }
  if (!context.controls.processed.processor.endian.empty()) {
    if (context.controls.processed.processor.endian == "big") {
      context.targetAttributes["Dendian"] = "Big-endian";
    } else if (context.controls.processed.processor.endian == "little") {
      context.targetAttributes["Dendian"] = "Little-endian";
    }
  }
  if (!context.controls.processed.processor.trustzone.empty()) {
    if (context.controls.processed.processor.trustzone == "secure") {
      context.targetAttributes["Dsecure"] = "Secure";
    } else if (context.controls.processed.processor.trustzone == "non-secure") {
      context.targetAttributes["Dsecure"] = "Non-secure";
    } else if (context.controls.processed.processor.trustzone == "off") {
      context.targetAttributes["Dsecure"] = "TZ-disabled";
    }
  }

  context.packages.insert({ ProjMgrUtils::GetPackageID(matchedDevice->GetPackage()), matchedDevice->GetPackage() });
  return true;
}

bool ProjMgrWorker::ProcessBoardPrecedence(StringCollection& item) {
  BoardItem board;
  string boardVendor, boardName, boardRevision;

  for (const auto& element : item.elements) {
    GetBoardItem(*element, board);
    if (!(GetPrecedentValue(boardVendor, board.vendor) &&
      GetPrecedentValue(boardName, board.name) &&
      GetPrecedentValue(boardRevision, board.revision))) {
      return false;
    }
  }
  *item.assign = GetBoardInfoString(boardVendor, boardName, boardRevision);
  return true;
}

bool ProjMgrWorker::ProcessDevicePrecedence(StringCollection& item) {
  DeviceItem device;
  string deviceVendor, deviceName, processorName;

  for (const auto& element : item.elements) {
    GetDeviceItem(*element, device);

    if (!(GetPrecedentValue(deviceVendor, device.vendor) &&
      GetPrecedentValue(deviceName, device.name) &&
      GetPrecedentValue(processorName, device.pname))) {
      return false;
    }
  }

  *item.assign = GetDeviceInfoString(deviceVendor, deviceName, processorName);
  return true;
}

bool ProjMgrWorker::ProcessPackages(ContextItem& context) {
  vector<PackItem> packRequirements;

  // Solution package requirements
  if (context.csolution) {
    packRequirements.insert(packRequirements.end(), context.csolution->packs.begin(), context.csolution->packs.end());
  }
  // Project package requirements
  if (context.cproject) {
    packRequirements.insert(packRequirements.end(), context.cproject->packs.begin(), context.cproject->packs.end());
  }
  // Layers package requirements
  for (const auto& [_, clayer] : context.clayers) {
    packRequirements.insert(packRequirements.end(), clayer->packs.begin(), clayer->packs.end());
  }

  // Filter context specific package requirements
  vector<PackItem> packages;
  for (const auto& packItem : packRequirements) {
    if (CheckContextFilters(packItem.type, context)) {
      packages.push_back(packItem);
    }
  }
  // Process packages
  for (const auto& packageEntry : packages) {
    PackageItem package;
    package.path = packageEntry.path;
    auto& pack = package.pack;
    string packInfoStr = packageEntry.pack;
    if (packInfoStr.find("::") != string::npos) {
      pack.vendor = RteUtils::RemoveSuffixByString(packInfoStr, "::");
      packInfoStr = RteUtils::RemovePrefixByString(packInfoStr, "::");
      pack.name   = RteUtils::GetPrefix(packInfoStr, '@');
    } else {
      pack.vendor = RteUtils::GetPrefix(packInfoStr, '@');
    }
    pack.version = RteUtils::GetSuffix(packInfoStr, '@');
    context.packRequirements.push_back(package);
  }
  return true;
}

bool ProjMgrWorker::ProcessToolchain(ContextItem& context) {

  if (context.compiler.empty()) {
    if (!context.cdefault || context.cdefault->compiler.empty()) {
      ProjMgrLogger::Error("compiler: value not set");
      return false;
    } else {
      context.compiler = context.cdefault->compiler;
    }
  }

  context.toolchain = GetToolchain(context.compiler);

  // get compatible registered toolchain
  if (!GetLatestToolchain(context.toolchain)) {
    // get compatible supported toolchain
    if (!GetToolchainConfig(context.toolchain.name, context.toolchain.range, context.toolchain.config, context.toolchain.version)) {
      ProjMgrLogger::Warn("cmake configuration file for toolchain '" + context.compiler + "' was not found");
      context.toolchain.version = RteUtils::GetPrefix(context.toolchain.range);
    }
  }
  if (context.toolchain.name == "AC6" || context.toolchain.name == "AC5") {
    context.targetAttributes["Tcompiler"] = "ARMCC";
    context.targetAttributes["Toptions"] = context.toolchain.name;
  } else {
    context.targetAttributes["Tcompiler"] = context.toolchain.name;
  }
  return true;
}

bool ProjMgrWorker::ProcessComponents(ContextItem& context) {
  if (!context.rteActiveTarget) {
    ProjMgrLogger::Error("missing RTE target");
    return false;
  }

  // Get installed components map
  const RteComponentMap& installedComponents = context.rteActiveTarget->GetFilteredComponents();
  RteComponentMap componentMap;
  for (auto& component : installedComponents) {
    const string& componentId = ProjMgrUtils::GetComponentID(component.second);
    componentMap[componentId] = component.second;
  }

  for (auto& [item, layer] : context.componentRequirements) {
    if (item.component.empty()) {
      continue;
    }
    RteComponent* matchedComponent = ProcessComponent(context, item, componentMap);
    if (!matchedComponent) {
      // No match
      ProjMgrLogger::Error("no component was found with identifier '" + item.component + "'");
      return false;
    }

    UpdateMisc(item.build.misc, context.toolchain.name);

    const auto& componentId = ProjMgrUtils::GetComponentID(matchedComponent);
    // Init matched component instance
    RteComponentInstance* matchedComponentInstance = new RteComponentInstance(matchedComponent);
    matchedComponentInstance->InitInstance(matchedComponent);
    if (!item.condition.empty()) {
      auto ti = matchedComponentInstance->EnsureTargetInfo(context.rteActiveTarget->GetName());
      ti->SetVersionMatchMode(VersionCmp::MatchMode::ENFORCED_VERSION);
      matchedComponentInstance->AddAttribute("versionMatchMode",
        VersionCmp::MatchModeToString(VersionCmp::MatchMode::ENFORCED_VERSION));
    }

    // Set layer's rtePath attribute
    if (!layer.empty()) {
      error_code ec;
      const string& rteDir = fs::relative(context.clayers[layer]->directory, context.cproject->directory, ec).append("RTE").generic_string();
      matchedComponentInstance->AddAttribute("rtedir", rteDir);
    }

    // Get generator
    RteGenerator* generator = matchedComponent->GetGenerator();
    const string generatorId = generator ? generator->GetID() : "";
    if (generator) {
      context.generators.insert({ generatorId, generator });
      string genDir;
      if (!GetGeneratorDir(generator, context, layer, genDir)) {
        return false;
      }
      matchedComponentInstance->AddAttribute("gendir", genDir);
      error_code ec;
      const string& gpdsc = fs::weakly_canonical(generator->GetExpandedGpdsc(context.rteActiveTarget, genDir), ec).generic_string();
      context.gpdscs.insert({ gpdsc, {componentId, generatorId, genDir} });
    }

    // Insert matched component into context list
    context.components.insert({ componentId, { matchedComponentInstance, &item, generatorId } });
    const auto& componentPackage = matchedComponent->GetPackage();
    context.packages.insert({ ProjMgrUtils::GetPackageID(componentPackage), componentPackage });
    if (matchedComponent->HasApi(context.rteActiveTarget)) {
      const auto& api = matchedComponent->GetApi(context.rteActiveTarget, false);
      if (api) {
        const auto& apiPackage = api->GetPackage();
        context.packages.insert({ ProjMgrUtils::GetPackageID(apiPackage), apiPackage });
      }
    }
  }

  // Add required components into RTE
  if (!AddRequiredComponents(context)) {
    return false;
  }

  return CheckRteErrors();
}

RteComponent* ProjMgrWorker::ProcessComponent(ContextItem& context, ComponentItem& item, RteComponentMap& componentMap)
{
  if (!item.condition.empty()) {
    RteComponentInstance ci(nullptr);
    ci.SetTag("component");

    ci.SetAttributes(ProjMgrUtils::ComponentAttributesFromId(item.component));
    ci.AddAttribute("condition", item.condition);
    auto ti = ci.EnsureTargetInfo(context.rteActiveTarget->GetName());
    ti->SetVersionMatchMode(VersionCmp::MatchMode::ENFORCED_VERSION);
    string packId = item.fromPack;
    RteUtils::ReplaceAll(packId,"::", ".");
    RteUtils::ReplaceAll(packId, "@", ".");
    RtePackageInstanceInfo packInfo(nullptr, packId);
    ci.SetPackageAttributes(packInfo);
    list<RteComponent*> components;
    RteComponent* enforced = context.rteActiveTarget->GetFilteredModel()->FindComponents(ci, components);
    if (enforced) {
      return enforced;
    }
  }

  // Filter components
  RteComponentMap filteredComponents;
  vector<string> filteredIds;
  string componentDescriptor = item.component;

  set<string> filterSet;
  if (componentDescriptor.find_first_of(ProjMgrUtils::COMPONENT_DELIMITERS) != string::npos) {
    // Consider a full or partial component identifier was given
    filterSet.insert(RteUtils::GetPrefix(componentDescriptor, *(ProjMgrUtils::PREFIX_CVERSION)));
  } else {
    // Consider free text was given
    filterSet = SplitArgs(componentDescriptor);
  }

  vector<string> componentIdVec;
  componentIdVec.reserve(componentMap.size());
  for (auto [componentId, _] :componentMap) {
    componentIdVec.push_back(componentId);
  }
  ApplyFilter(componentIdVec, filterSet, filteredIds);
  for (const auto& filteredId : filteredIds) {
    auto c = componentMap[filteredId];
    filteredComponents[filteredId] = c;
  }

  // Multiple matches, search best matched identifier
  if (filteredComponents.size() > 1) {
    RteComponentMap fullMatchedComponents;
    for (const auto& component : filteredComponents) {
      if (FullMatch(SplitArgs(component.first, ProjMgrUtils::COMPONENT_DELIMITERS), SplitArgs(componentDescriptor, ProjMgrUtils::COMPONENT_DELIMITERS))) {
        fullMatchedComponents.insert(component);
      }
    }
    if (fullMatchedComponents.size() > 0) {
      filteredComponents = fullMatchedComponents;
    }
  }

  // Multiple matches, check exact partial identifier
  string requiredComponentId = RteUtils::RemovePrefixByString(item.component, ProjMgrUtils::SUFFIX_CVENDOR);
  if (filteredComponents.size() > 1) {
    RteComponentMap matchedComponents;
    for (const auto& [id, component] : filteredComponents) {
      // Get component id without vendor and version
      const string& componentId = ProjMgrUtils::GetPartialComponentID(component);
      if (requiredComponentId.compare(componentId) == 0) {
        matchedComponents[id] = component;
      }
    }
    // Select unique exact match
    if (matchedComponents.size() == 1) {
      filteredComponents = matchedComponents;
    }
  }
  // Evaluate filtered components
  if (filteredComponents.empty()) {
    // No match
    return nullptr;
  }
  // One or multiple matches found
  // check for default variant if requested variant is empty
  for (const auto& [id, component] : filteredComponents) {
    if (component->IsDefaultVariant() && !component->GetCvariantName().empty()) {
      return component;
    }
  }

  set<string> availableComponentVersions;
  for_each(filteredComponents.begin(), filteredComponents.end(),
    [&](const pair<std::string, RteComponent*>& component) {
      availableComponentVersions.insert(RteUtils::GetSuffix(component.first, *ProjMgrUtils::PREFIX_CVERSION));
    });
  const string filterVersion = RteUtils::GetSuffix(item.component, *ProjMgrUtils::PREFIX_CVERSION, true);
  const string matchedVersion = VersionCmp::GetMatchingVersion(filterVersion, availableComponentVersions);
  if (matchedVersion.empty()) {
    return nullptr;
  }
  auto itr = std::find_if(filteredComponents.begin(), filteredComponents.end(),
    [&](const pair<std::string, RteComponent*>& item) {
      return (item.first.find(matchedVersion) != string::npos);
    });
  return itr->second;
}


bool ProjMgrWorker::AddRequiredComponents(ContextItem& context) {
  list<RteItem*> selItems;
  for (auto& [_, component] : context.components) {
    selItems.push_back(component.instance);
  }
  set<RteComponentInstance*> unresolvedComponents;
  context.rteActiveProject->AddCprjComponents(selItems, context.rteActiveTarget, unresolvedComponents);
  if (!unresolvedComponents.empty()) {
    string msg = "unresolved components:";
    for (const auto& component : unresolvedComponents) {
      msg += "\n" + ProjMgrUtils::GetComponentID(component);
    }
    ProjMgrLogger::Error(msg);
    return false;
  }
  context.rteActiveProject->CollectSettings();

  // Generate RTE headers
  context.rteActiveProject->GenerateRteHeaders();

  // Check regions header, generate it if needed
  if (!context.linker.regions.empty()) {
    CheckAndGenerateRegionsHeader(context);
  }

  return true;
}

void ProjMgrWorker::CheckAndGenerateRegionsHeader(ContextItem& context) {
  error_code ec;
  const string regionsHeader = fs::weakly_canonical(fs::path(context.directories.cprj).append(context.linker.regions), ec).generic_string();
  if (!RteFsUtils::Exists(regionsHeader)) {
    string generatedRegionsFile;
    if (GenerateRegionsHeader(context, generatedRegionsFile)) {
      ProjMgrLogger::Info(generatedRegionsFile, "regions header generated successfully");
    }
  }
  if (!RteFsUtils::Exists(regionsHeader)) {
    ProjMgrLogger::Warn(regionsHeader, "specified regions header was not found");
  }
}

bool ProjMgrWorker::GenerateRegionsHeader(ContextItem& context, string& generatedRegionsFile) {
  // get rte folder associated to 'Device' class
  string rteFolder;
  for (const auto& [_, component] : context.components) {
    if (component.instance->GetCclassName() == "Device") {
      rteFolder = component.instance->GetRteFolder().empty() ? "" :
        fs::path(context.cproject->directory).append(component.instance->GetRteFolder()).generic_string();
      break;
    }
  }
  // get context's rte folder
  if (rteFolder.empty()) {
    rteFolder = fs::path(context.directories.cprj).append(context.directories.rte).generic_string();
  }
  // generate regions header
  if (!context.rteActiveTarget->GenerateRegionsHeader(rteFolder + "/")) {
    ProjMgrLogger::Warn("regions header file generation failed");
    return false;
  }
  error_code ec;
  generatedRegionsFile = fs::weakly_canonical(fs::path(rteFolder).append(context.rteActiveTarget->GetRegionsHeader()), ec).generic_string();
  return true;
}

bool ProjMgrWorker::ProcessConfigFiles(ContextItem& context) {
  if (!context.rteActiveTarget) {
    ProjMgrLogger::Error("missing RTE target");
    return false;
  }
  const auto& components = context.rteActiveProject->GetComponentInstances();
  if (!components.empty()) for (const auto& itc : components) {
    RteComponentInstance* ci = itc.second;
    if (!ci || !ci->IsUsedByTarget(context.rteActiveTarget->GetName())) {
      continue;
    }
    map<string, RteFileInstance*> configFiles;
    context.rteActiveProject->GetFileInstances(ci, context.rteActiveTarget->GetName(), configFiles);
    if (!configFiles.empty()) {
      const string& componentID = ProjMgrUtils::GetComponentID(ci);
      for (auto fi : configFiles) {
        context.configFiles[componentID].insert(fi);
      }
    }
  }
  // Linker script
  if (context.linker.script.empty() && context.linker.regions.empty() && context.linker.defines.empty()) {
    const auto& groups = context.rteActiveTarget->GetProjectGroups();
    for (auto group : groups) {
      for (auto file : group.second) {
        if (file.second.m_cat == RteFile::Category::LINKER_SCRIPT) {
          error_code ec;
          context.linker.script = fs::relative(context.cproject->directory + "/" + file.first, context.directories.cprj, ec).generic_string();
          break;
        }
      }
    }
  }
  SetDefaultLinkerScript(context);

  return true;
}

void ProjMgrWorker::SetDefaultLinkerScript(ContextItem& context) {
  if (context.linker.script.empty()) {
    const string& compilerRoot = GetCompilerRoot();
    string linkerScript;
    error_code ec;
    for (auto const& entry : fs::recursive_directory_iterator(compilerRoot, ec)) {
      string stem = entry.path().stem().generic_string();
      if (RteUtils::EqualNoCase(context.toolchain.name + "_linker_script", stem)) {
        linkerScript = entry.path().generic_string();
        break;
      }
    }
    if (linkerScript.empty()) {
      ProjMgrLogger::Warn("linker script template for compiler '" + context.toolchain.name + "' was not found");
      return;
    }
    context.linker.script = fs::relative(linkerScript, context.directories.cprj).generic_string();
    if (context.linker.regions.empty()) {
      string generatedRegionsFile;
      if (GenerateRegionsHeader(context, generatedRegionsFile)) {
        context.linker.regions = fs::relative(generatedRegionsFile, context.directories.cprj).generic_string();
      }
    }
  }
}

bool ProjMgrWorker::ProcessComponentFiles(ContextItem& context) {
  if (!context.rteActiveTarget) {
    ProjMgrLogger::Error("missing RTE target");
    return false;
  }
  // files belonging to project groups, except config files
  const auto& groups = context.rteActiveTarget->GetProjectGroups();
  for (const auto& [_, group] : groups) {
    for (auto& [file, fileInfo] : group) {
      const auto& ci = context.rteActiveTarget->GetComponentInstanceForFile(file);
      const auto& component = ci->GetResolvedComponent(context.rteActiveTarget->GetName());
      if (!fileInfo.m_fi) {
        const auto& absPackPath = component->GetPackage()->GetAbsolutePackagePath();
        error_code ec;
        const auto& relFilename = fs::relative(file, absPackPath, ec).generic_string();
        const auto& componentFile = context.rteActiveTarget->GetFile(relFilename, component);
        if (componentFile) {
          const auto& attr = componentFile->GetAttribute("attr");
          const auto& category = componentFile->GetAttribute("category");
          const auto& version = attr == "config" ? componentFile->GetVersionString() : "";
          context.componentFiles[ProjMgrUtils::GetComponentID(component)].push_back({ file, attr, category, version });
        }
      }
    }
  }
  // iterate over components
  for (const auto& [componentId, component] : context.components) {
    const RteComponent* rteComponent = component.instance->GetParent()->GetComponent();
    const auto& files = rteComponent->GetFileContainer() ? rteComponent->GetFileContainer()->GetChildren() : list<RteItem*>();
    // pre-include files from packs
    for (const auto& componentFile : files) {
      const auto& category = componentFile->GetAttribute("category");
      const auto& attr = componentFile->GetAttribute("attr");
      if (((category == "preIncludeGlobal") || (category == "preIncludeLocal")) && attr.empty()) {
        const auto& preInclude = rteComponent->GetPackage()->GetAbsolutePackagePath() + componentFile->GetAttribute("name");
        if (IsPreIncludeByTarget(context.rteActiveTarget, preInclude)) {
          context.componentFiles[componentId].push_back({ preInclude, "", category, "" });
        }
      }
    }
    // config files
    map<const RteItem*, string> configFilePaths;
    for (const auto& configFileMap : context.configFiles) {
      if (configFileMap.first == componentId) {
        for (const auto& [_, configFile] : configFileMap.second) {
          // cache config file path first
          const auto& originalFile = configFile->GetFile(context.rteActiveTarget->GetName());
          const auto& filename = configFile->GetAbsolutePath();
          configFilePaths[originalFile] = filename;
          const auto& category = configFile->GetAttribute("category");
          switch (RteFile::CategoryFromString(category)) {
          case RteFile::Category::GEN_SOURCE:
          case RteFile::Category::GEN_HEADER:
          case RteFile::Category::GEN_PARAMS:
          case RteFile::Category::GEN_ASSET:
            continue; // ignore gen files
          default:
            break;
          };
          const auto& version = originalFile->GetVersionString();
          context.componentFiles[componentId].push_back({ filename, "config", category, version });
        }
      }
    }
    // input files for component generator. This list of files is directly fetched from the PDSC.
    if (rteComponent->GetGenerator()) {
      for (const RteItem* rteFile: files) {
        const auto& category = rteFile->GetAttribute("category");
        switch (RteFile::CategoryFromString(category)) {
        case RteFile::Category::GEN_SOURCE:
        case RteFile::Category::GEN_HEADER:
        case RteFile::Category::GEN_PARAMS:
        case RteFile::Category::GEN_ASSET:
          break; // add only gen files, ignore others
        default:
          continue;
        };
        const auto& version = rteFile->GetVersionString();
        const auto& attr = rteFile->GetAttribute("attr");
        const auto& filename = (attr == "config" && configFilePaths.find(rteFile) != configFilePaths.end()) ?
                                configFilePaths[rteFile] : rteFile->GetOriginalAbsolutePath();
        context.generatorInputFiles[componentId].push_back({ filename, attr, category, version });
      }
    }
  }
  // constructed local pre-include files
  const auto& preIncludeFiles = context.rteActiveTarget->GetPreIncludeFiles();
  for (const auto& [component, fileSet] : preIncludeFiles) {
    if (component) {
      const string& preIncludeLocal = component->ConstructComponentPreIncludeFileName();
      for (const auto& file : fileSet) {
        if (file == preIncludeLocal) {
          const string& filename = context.rteActiveProject->GetProjectPath() +
            context.rteActiveProject->GetRteHeader(file, context.rteActiveTarget->GetName(), "");
          const string& componentID = ProjMgrUtils::GetComponentID(component);
          context.componentFiles[componentID].push_back({ filename, "", "preIncludeLocal", ""});
          break;
        }
      }
    }
  }
  return true;
}

bool ProjMgrWorker::IsPreIncludeByTarget(const RteTarget* activeTarget, const string& preInclude) {
  const auto& preIncludeFiles = activeTarget->GetPreIncludeFiles();
  for (const auto& [_, fileSet] : preIncludeFiles) {
    for (auto file : fileSet) {
      error_code ec;
      if (fs::equivalent(file, preInclude, ec)) {
        return true;
      }
    }
  }
  return false;
}

bool ProjMgrWorker::ValidateContext(ContextItem& context) {
  context.validationResults.clear();
  map<const RteItem*, RteDependencyResult> results;
  context.rteActiveTarget->GetDepsResult(results, context.rteActiveTarget);

  for (const auto& [component, result] : results) {
    RteItem::ConditionResult validationResult = result.GetResult();
    const auto& componentID = ProjMgrUtils::GetComponentID(component);
    const auto& conditions = result.GetResults();
    const auto& aggregates = result.GetComponentAggregates();

    set<string> aggregatesSet;
    for (const auto& aggregate : aggregates) {
      aggregatesSet.insert(ProjMgrUtils::GetComponentAggregateID(aggregate));
    }
    set<string> expressionsSet;
    for (const auto& [condition, _] : conditions) {
      expressionsSet.insert(ProjMgrUtils::GetConditionID(condition));
    }
    context.validationResults.push_back({ validationResult, componentID, expressionsSet, aggregatesSet });
  }

  if (context.validationResults.empty()) {
    return true;
  }
  return false;
}

bool ProjMgrWorker::ProcessGpdsc(ContextItem& context) {
  // Read gpdsc
  const map<string, RteGpdscInfo*>& gpdscInfos = context.rteActiveProject->GetGpdscInfos();
  for (const auto& [file, info] : gpdscInfos) {
    error_code ec;
    const string gpdscFile = fs::weakly_canonical(file, ec).generic_string();
    bool validGpdsc;
    RtePackage* gpdscPack = ProjMgrUtils::ReadGpdscFile(gpdscFile, validGpdsc);
    if (!gpdscPack) {
      ProjMgrLogger::Error(gpdscFile, "context '" + context.name + "' generator '" + context.gpdscs.at(gpdscFile).generator +
        "' from component '" + context.gpdscs.at(gpdscFile).component + "': reading gpdsc failed");
      CheckRteErrors();
      return false;
    } else {
      if (!validGpdsc) {
        ProjMgrLogger::Warn(gpdscFile, "context '" + context.name + "' generator '" + context.gpdscs.at(gpdscFile).generator +
          "' from component '" + context.gpdscs.at(gpdscFile).component + "': gpdsc validation failed");
      }
      info->SetGpdscPack(gpdscPack);
    }
    // insert gpdsc components
    for (const auto& gpdscComponent : gpdscPack->GetComponents()->GetChildren()) {
      list<RteItem*> components;
      if (gpdscComponent->GetTag() == "component") {
        components.push_back(gpdscComponent);
      } else if (gpdscComponent->GetTag() == "bundle") {
        components = gpdscComponent->GetChildren();
      }
      for (const auto component : components) {
        const auto& componentId = ProjMgrUtils::GetComponentID(component);
        const auto& item = context.components[context.gpdscs.at(gpdscFile).component].item;
        RteComponentInstance* componentInstance = new RteComponentInstance(component);
        componentInstance->InitInstance(component);
        context.components.insert({ componentId, { componentInstance, item, component->GetGeneratorName()}});
      }
    }
  }
  if (!gpdscInfos.empty()) {
    // Update target with gpdsc model
    if (!SetTargetAttributes(context, context.targetAttributes)) {
      return false;
    }
    // Re-add required components into RTE
    if (!AddRequiredComponents(context)) {
      return false;
    }
  }
  return CheckRteErrors();
}

bool ProjMgrWorker::ProcessPrecedence(StringCollection& item) {
  for (const auto& element : item.elements) {
    if (!GetPrecedentValue(*item.assign, *element)) {
      return false;
    }
  }
  return true;
}

bool ProjMgrWorker::ProcessCompilerPrecedence(StringCollection& item, bool acceptRedefinition) {
  for (const auto& element : item.elements) {
    if (!element->empty()) {
      if (!ProjMgrUtils::AreCompilersCompatible(*item.assign, *element)) {
        if (acceptRedefinition) {
          ProjMgrLogger::Warn("redefinition from '" + *item.assign + "' into '" + *element + "'");
        } else {
          ProjMgrLogger::Error("redefinition from '" + *item.assign + "' into '" + *element + "' is not allowed");
          return false;
        }
      }
      ProjMgrUtils::CompilersIntersect(*item.assign, *element, *item.assign);
    }
  }
  return true;
}

void ProjMgrWorker::MergeDefines(StringVectorCollection& item) {
  for (const auto& element : item.pair) {
    AddStringItemsUniquely(*item.assign, *element.add);
    RemoveDefines(*item.assign, *element.remove);
  }
}

void ProjMgrWorker::MergeStringVector(StringVectorCollection& item) {
  for (const auto& element : item.pair) {
    AddStringItemsUniquely(*item.assign, *element.add);
    RemoveStringItems(*item.assign, *element.remove);
  }
}

bool ProjMgrWorker::ProcessPrecedences(ContextItem& context) {
  // Notes: defines, includes and misc are additive. All other keywords overwrite previous settings.
  // The target-type and build-type definitions are additive, but an attempt to
  // redefine an already existing type results in an error.
  // The settings of the target-type are processed first; then the settings of the
  // build-type that potentially overwrite the target-type settings.

  if (context.precedences) {
    return true;
  }
  context.precedences = true;

  // Get content of build and target types
  if (!GetTypeContent(context)) {
    return false;
  }

  StringCollection board = {
  &context.board,
  {
    &context.cproject->target.board,
    &context.csolution->target.board,
    &context.targetItem.board,
  },
  };
  for (const auto& clayer : context.clayers) {
    board.elements.push_back(&clayer.second->target.board);
  }
  if (!ProcessBoardPrecedence(board)) {
    return false;
  }

  StringCollection device = {
    &context.device,
    {
      &context.cproject->target.device,
      &context.csolution->target.device,
      &context.targetItem.device,
    },
  };
  for (const auto& clayer : context.clayers) {
    device.elements.push_back(&clayer.second->target.device);
  }
  if (!ProcessDevicePrecedence(device)) {
    return false;
  }

  StringCollection compiler = {
   &context.compiler,
   {
     &context.controls.cproject.compiler,
     &context.controls.csolution.compiler,
     &context.controls.target.compiler,
     &context.controls.build.compiler,
   },
  };
  for (const auto& [_, clayer] : context.clayers) {
    compiler.elements.push_back(&clayer->target.build.compiler);
  }
  if (!ProcessCompilerPrecedence(compiler)) {
    return false;
  }
  // accept compiler redefinition in the command line
  compiler = { &context.compiler, { &m_selectedToolchain } };
  if (!ProcessCompilerPrecedence(compiler, true)) {
    return false;
  }
  if (!ProcessToolchain(context)) {
    return false;
  }

  // set context variables (static access sequences)
  DeviceItem deviceItem;
  GetDeviceItem(context.device, deviceItem);
  context.variables[ProjMgrUtils::AS_DNAME] = deviceItem.name;
  context.variables[ProjMgrUtils::AS_PNAME] = deviceItem.pname;
  context.variables[ProjMgrUtils::AS_BNAME] = context.board;
  context.variables[ProjMgrUtils::AS_COMPILER] = context.toolchain.name;

  // Add cdefault misc into csolution
  if (context.cdefault) {
    context.controls.csolution.misc.insert(context.controls.csolution.misc.end(),
      context.cdefault->misc.begin(), context.cdefault->misc.end());
  }

  // Get build options content of project setup
  if (!GetProjectSetup(context)) {
    return false;
  }

  // Processor options
  if (!ProcessProcessorOptions(context)) {
    return false;
  }

  // Output filenames must be processed after board, device and compiler precedences
  // but before processing other access sequences
  if (!ProcessOutputFilenames(context)) {
    return false;
  }

  // Access sequences and relative path references must be processed
  // after board, device and compiler precedences (due to $Bname$, $Dname$ and $Compiler$)
  // after output filenames (due to $Output$)
  // but before processing misc, defines and includes precedences
  if (!ProcessSequencesRelatives(context)) {
    return false;
  }

  // Optimize
  StringCollection optimize = {
    &context.controls.processed.optimize,
    {
      &context.controls.cproject.optimize,
      &context.controls.csolution.optimize,
      &context.controls.target.optimize,
      &context.controls.build.optimize,
    },
  };
  for (auto& setup : context.controls.setups) {
    optimize.elements.push_back(&setup.optimize);
  }
  for (auto& [_, clayer] : context.controls.clayers) {
    optimize.elements.push_back(&clayer.optimize);
  }
  if (!ProcessPrecedence(optimize)) {
    return false;
  }

  // Debug
  StringCollection debug = {
    &context.controls.processed.debug,
    {
      &context.controls.cproject.debug,
      &context.controls.csolution.debug,
      &context.controls.target.debug,
      &context.controls.build.debug,
    },
  };
  for (auto& setup : context.controls.setups) {
    debug.elements.push_back(&setup.debug);
  }
  for (auto& [_, clayer] : context.controls.clayers) {
    debug.elements.push_back(&clayer.debug);
  }
  if (!ProcessPrecedence(debug)) {
    return false;
  }

  // Warnings
  StringCollection warnings = {
    &context.controls.processed.warnings,
    {
      &context.controls.cproject.warnings,
      &context.controls.csolution.warnings,
      &context.controls.target.warnings,
      &context.controls.build.warnings,
    },
  };
  for (auto& setup : context.controls.setups) {
    warnings.elements.push_back(&setup.warnings);
  }
  for (auto& [_, clayer] : context.controls.clayers) {
    warnings.elements.push_back(&clayer.warnings);
  }
  if (!ProcessPrecedence(warnings)) {
    return false;
  }

  // Language C
  StringCollection languageC = {
    &context.controls.processed.languageC,
    {
      &context.controls.cproject.languageC,
      &context.controls.csolution.languageC,
      &context.controls.target.languageC,
      &context.controls.build.languageC,
    },
  };
  for (auto& setup : context.controls.setups) {
    languageC.elements.push_back(&setup.languageC);
  }
  for (auto& [_, clayer] : context.controls.clayers) {
    languageC.elements.push_back(&clayer.languageC);
  }
  if (!ProcessPrecedence(languageC)) {
    return false;
  }

  // Language C++
  StringCollection languageCpp = {
    &context.controls.processed.languageCpp,
    {
      &context.controls.cproject.languageCpp,
      &context.controls.csolution.languageCpp,
      &context.controls.target.languageCpp,
      &context.controls.build.languageCpp,
    },
  };
  for (auto& setup : context.controls.setups) {
    languageCpp.elements.push_back(&setup.languageCpp);
  }
  for (auto& [_, clayer] : context.controls.clayers) {
    languageCpp.elements.push_back(&clayer.languageCpp);
  }
  if (!ProcessPrecedence(languageCpp)) {
    return false;
  }

  // Misc
  vector<vector<MiscItem>*> miscVec = {
    &context.controls.cproject.misc,
    &context.controls.csolution.misc,
    &context.controls.build.misc,
    &context.controls.target.misc,
  };
  for (auto& setup : context.controls.setups) {
    miscVec.push_back(&setup.misc);
  }
  for (auto& [_, clayer] : context.controls.clayers) {
    miscVec.push_back(&clayer.misc);
  }
  context.controls.processed.misc.push_back({});
  context.controls.processed.misc.front().forCompiler = context.compiler;
  AddMiscUniquely(context.controls.processed.misc.front(), miscVec);

  // Defines
  vector<string> projectDefines, projectUndefines;
  AddStringItemsUniquely(projectDefines, context.controls.cproject.defines);
  for (auto& [_, clayer] : context.controls.clayers) {
    AddStringItemsUniquely(projectDefines, clayer.defines);
  }
  for (auto& setup : context.controls.setups) {
    AddStringItemsUniquely(projectDefines, setup.defines);
  }
  AddStringItemsUniquely(projectUndefines, context.controls.cproject.undefines);
  for (auto& [_, clayer] : context.controls.clayers) {
    AddStringItemsUniquely(projectUndefines, clayer.undefines);
  }
  for (auto& setup : context.controls.setups) {
    AddStringItemsUniquely(projectUndefines, setup.undefines);
  }
  StringVectorCollection defines = {
    &context.controls.processed.defines,
    {
      {&projectDefines, &projectUndefines},
      {&context.controls.csolution.defines, &context.controls.csolution.undefines},
      {&context.controls.target.defines, &context.controls.target.undefines},
      {&context.controls.build.defines, &context.controls.build.undefines},
    }
  };
  MergeDefines(defines);

  // Includes
  vector<string> projectAddPaths, projectDelPaths;
  AddStringItemsUniquely(projectAddPaths, context.controls.cproject.addpaths);
  for (auto& [_, clayer] : context.controls.clayers) {
    AddStringItemsUniquely(projectAddPaths, clayer.addpaths);
  }
  for (auto& setup : context.controls.setups) {
    AddStringItemsUniquely(projectAddPaths, setup.addpaths);
  }
  AddStringItemsUniquely(projectDelPaths, context.controls.cproject.delpaths);
  for (auto& [_, clayer] : context.controls.clayers) {
    AddStringItemsUniquely(projectDelPaths, clayer.delpaths);
  }
  for (auto& setup : context.controls.setups) {
    AddStringItemsUniquely(projectDelPaths, setup.delpaths);
  }
  StringVectorCollection includes = {
    &context.controls.processed.addpaths,
    {
      {&projectAddPaths, &projectDelPaths},
      {&context.controls.csolution.addpaths, &context.controls.csolution.delpaths},
      {&context.controls.target.addpaths, &context.controls.target.delpaths},
      {&context.controls.build.addpaths, &context.controls.build.delpaths},
    }
  };
  MergeStringVector(includes);

  return true;
}

bool ProjMgrWorker::ProcessProcessorOptions(ContextItem& context) {
  StringCollection trustzone = {
    &context.controls.processed.processor.trustzone,
    {
      &context.controls.cproject.processor.trustzone,
      &context.controls.csolution.processor.trustzone,
      &context.controls.target.processor.trustzone,
      &context.controls.build.processor.trustzone,
    },
  };
  for (auto& setup : context.controls.setups) {
    trustzone.elements.push_back(&setup.processor.trustzone);
  }
  for (auto& [_, clayer] : context.controls.clayers) {
    trustzone.elements.push_back(&clayer.processor.trustzone);
  }
  if (!ProcessPrecedence(trustzone)) {
    return false;
  }

  StringCollection fpu = {
    &context.controls.processed.processor.fpu,
    {
      &context.controls.cproject.processor.fpu,
      &context.controls.csolution.processor.fpu,
      &context.controls.target.processor.fpu,
      &context.controls.build.processor.fpu,
    },
  };
  for (auto& setup : context.controls.setups) {
    fpu.elements.push_back(&setup.processor.fpu);
  }
  for (auto& [_, clayer] : context.controls.clayers) {
    fpu.elements.push_back(&clayer.processor.fpu);
  }
  if (!ProcessPrecedence(fpu)) {
    return false;
  }

  StringCollection endian = {
    &context.controls.processed.processor.endian,
    {
      &context.controls.cproject.processor.endian,
      &context.controls.csolution.processor.endian,
      &context.controls.target.processor.endian,
      &context.controls.build.processor.endian,
    },
  };
  for (auto& setup : context.controls.setups) {
    endian.elements.push_back(&setup.processor.endian);
  }
  for (auto& [_, clayer] : context.controls.clayers) {
    endian.elements.push_back(&clayer.processor.endian);
  }
  if (!ProcessPrecedence(endian)) {
    return false;
  }
  return true;
}

bool ProjMgrWorker::ProcessLinkerOptions(ContextItem& context) {
  // clear processing lists
  context.linker.scriptList.clear();
  context.linker.regionsList.clear();

  // linker options in cproject
  for (const LinkerItem& linker : context.cproject->linker) {
    if (!ProcessLinkerOptions(context, linker, context.cproject->directory)) {
      return false;
    }
  }
  // linker options in clayers
  for (const auto& [_, clayer] : context.clayers) {
    for (const LinkerItem& linker : clayer->linker) {
      if (!ProcessLinkerOptions(context, linker, clayer->directory)) {
        return false;
      }
    }
  }
  // linker options in setups
  for (const auto& setup : context.cproject->setups) {
    if (CheckContextFilters(setup.type, context) && CheckCompiler(setup.forCompiler, context.compiler)) {
      for (const LinkerItem& linker : setup.linker) {
        if (!ProcessLinkerOptions(context, linker, context.cproject->directory)) {
          return false;
        }
      }
    }
  }
  // check precedences
  StringCollection linkerScriptFile = {
    &context.linker.script,
  };
  StringCollection linkerRegionsFile = {
    &context.linker.regions,
  };
  for (auto& script : context.linker.scriptList) {
    linkerScriptFile.elements.push_back(&script);
  }
  for (auto& regions : context.linker.regionsList) {
    linkerRegionsFile.elements.push_back(&regions);
  }
  if (!ProcessPrecedence(linkerScriptFile)) {
    return false;
  }
  if (!ProcessPrecedence(linkerRegionsFile)) {
    return false;
  }
  return true;
}

bool ProjMgrWorker::ProcessLinkerOptions(ContextItem& context, const LinkerItem& linker, const string& ref) {
  if (CheckContextFilters(linker.typeFilter, context) &&
    CheckCompiler(linker.forCompiler, context.compiler)) {
    if (!linker.script.empty()) {
      context.linker.scriptList.push_back(linker.script);
      if (!ProcessSequenceRelative(context, context.linker.scriptList.back(), ref)) {
        return false;
      }
    }
    if (!linker.regions.empty()) {
      context.linker.regionsList.push_back(linker.regions);
      if (!ProcessSequenceRelative(context, context.linker.regionsList.back(), ref)) {
        return false;
      }
    }
    AddStringItemsUniquely(context.linker.defines, linker.defines);
  }
  return true;
}


bool ProjMgrWorker::ProcessSequencesRelatives(ContextItem& context) {

  // directories
  const string ref = m_outputDir.empty() ? context.csolution->directory : RteFsUtils::AbsolutePath(m_outputDir).generic_string();
  if (!ProcessSequenceRelative(context, context.directories.cprj) ||
      !ProcessSequenceRelative(context, context.directories.rte, context.cproject->directory) ||
      !ProcessSequenceRelative(context, context.directories.outdir, ref) ||
      !ProcessSequenceRelative(context, context.directories.intdir, ref)) {
    return false;
  }

  // project, solution, target-type and build-type translation controls
  if (!ProcessSequencesRelatives(context, context.controls.cproject, context.cproject->directory) ||
      !ProcessSequencesRelatives(context, context.controls.csolution, context.csolution->directory) ||
      !ProcessSequencesRelatives(context, context.controls.target, context.csolution->directory) ||
      !ProcessSequencesRelatives(context, context.controls.build, context.csolution->directory)) {
    return false;
  }

  // setups translation controls
  for (auto& setup : context.controls.setups) {
    if (!ProcessSequencesRelatives(context, setup, context.cproject->directory)) {
      return false;
    }
  }

  // components translation controls
  for (ComponentItem component : context.cproject->components) {
    if (!ProcessSequencesRelatives(context, component.build, context.cproject->directory)) {
      return false;
    }
    if (!AddComponent(component, "", context.componentRequirements, context.type, context)) {
      return false;
    }
  }

  // layers translation controls
  for (const auto& [name, clayer] : context.clayers) {
    if (!ProcessSequencesRelatives(context, context.controls.clayers[name], clayer->directory)) {
      return false;
    }
    for (ComponentItem component : clayer->components) {
      if (!ProcessSequencesRelatives(context, component.build, clayer->directory)) {
        return false;
      }
      if (!AddComponent(component, name, context.componentRequirements, context.type, context)) {
        return false;
      }
    }
  }
  return true;
}

void ProjMgrWorker::UpdatePartialReferencedContext(ContextItem& context, string& contextName) {
  if (contextName.find('+') == string::npos) {
    if (!context.type.target.empty()) {
      contextName.append('+' + context.type.target);
    }
  }
  if (contextName.find('.') == string::npos) {
    if (!context.type.build.empty()) {
      const size_t targetDelim = contextName.find('+');
      contextName.insert(targetDelim == string::npos ? contextName.length() : targetDelim, '.' + context.type.build);
    }
  }
  if (contextName.empty() || (contextName.front() == '.') || (contextName.front() == '+')) {
    contextName.insert(0, context.cproject->name);
  }
}

void ProjMgrWorker::ExpandAccessSequence(const ContextItem & context, const ContextItem & refContext, const string & sequence, string & item, bool withHeadingDot) {
  const string& refContextOutDir = refContext.directories.cprj + "/" + refContext.directories.outdir;
  const string& relOutDir = RteFsUtils::RelativePath(refContextOutDir, context.directories.cprj, withHeadingDot);
  string regExStr = "\\$";
  string replacement;
  if (sequence == ProjMgrUtils::AS_SOLUTION_DIR) {
    regExStr += ProjMgrUtils::AS_SOLUTION_DIR;
    replacement = RteFsUtils::RelativePath(refContext.csolution->directory, context.directories.cprj, withHeadingDot);
  } else if (sequence == ProjMgrUtils::AS_PROJECT_DIR) {
    regExStr += ProjMgrUtils::AS_PROJECT_DIR;
    replacement = RteFsUtils::RelativePath(refContext.cproject->directory, context.directories.cprj, withHeadingDot);
  } else if (sequence == ProjMgrUtils::AS_OUT_DIR) {
    regExStr += ProjMgrUtils::AS_OUT_DIR;
    replacement = relOutDir;
  } else if (sequence == ProjMgrUtils::AS_ELF) {
    regExStr += ProjMgrUtils::AS_ELF;
    replacement = refContext.outputTypes.elf.on ? relOutDir + "/" + refContext.outputTypes.elf.filename : "";
  } else if (sequence == ProjMgrUtils::AS_BIN) {
    regExStr += ProjMgrUtils::AS_BIN;
    replacement = refContext.outputTypes.bin.on ? relOutDir + "/" + refContext.outputTypes.bin.filename : "";
  } else if (sequence == ProjMgrUtils::AS_HEX) {
    regExStr += ProjMgrUtils::AS_HEX;
    replacement = refContext.outputTypes.hex.on ? relOutDir + "/" + refContext.outputTypes.hex.filename : "";
  } else if (sequence == ProjMgrUtils::AS_LIB) {
    regExStr += ProjMgrUtils::AS_LIB;
    replacement = refContext.outputTypes.lib.on ? relOutDir + "/" + refContext.outputTypes.lib.filename : "";
  } else if (sequence == ProjMgrUtils::AS_CMSE) {
    regExStr += ProjMgrUtils::AS_CMSE;
    replacement = refContext.outputTypes.cmse.on ? relOutDir + "/" + refContext.outputTypes.cmse.filename : "";
  }
  regex regEx = regex(regExStr + "\\(.*\\)\\$");
  item = regex_replace(item, regEx, replacement);
}

bool ProjMgrWorker::ProcessSequenceRelative(ContextItem& context, string& item, const string& ref, bool withHeadingDot) {
  size_t offset = 0;
  bool pathReplace = false;
  // expand variables (static access sequences)
  const string input = item = ExpandString(item, context.variables);
  // expand dynamic access sequences
  while (offset != string::npos) {
    string sequence;
    // get next access sequence
    if (!GetAccessSequence(offset, input, sequence, '$', '$')) {
      return false;
    }
    if (offset != string::npos) {
      smatch asMatches;
      if (regex_match(sequence, asMatches, accessSequencesRegEx) && asMatches.size() == 3) {
        // get capture groups, see regex accessSequencesRegEx
        const string sequenceName = asMatches[1];
        string contextName = asMatches[2];
        // access sequences with 'context' argument lead to path replacement
        pathReplace = true;
        // update referenced context name when it's partially specified
        UpdatePartialReferencedContext(context, contextName);
        // find referenced context
        if (m_contexts.find(contextName) != m_contexts.end()) {
          error_code ec;
          auto& refContext = m_contexts.at(contextName);
          // process referenced context precedences if needed
          if (!refContext.precedences) {
            if (!ProcessPrecedences(refContext)) {
              return false;
            }
          }
          // expand access sequence
          ExpandAccessSequence(context, refContext, sequenceName, item, withHeadingDot);
        } else {
          // full or partial context name cannot be resolved to a valid context
          ProjMgrLogger::Error("context '" + contextName + "' referenced by access sequence '" + sequenceName + "' does not exist");
          return false;
        }
      } else {
        // access sequence is unknown
        ProjMgrLogger::Warn("unknown access sequence: '" + sequence + "'");
        continue;
      }
    }
  }
  if (!pathReplace && !ref.empty()) {
    // adjust relative path according to the given reference
    error_code ec;
    if (!fs::equivalent(context.directories.cprj, ref, ec)) {
      const string& absPath = fs::weakly_canonical(fs::path(item).is_relative() ? ref + "/" + item : item, ec).generic_string();
      item = RteFsUtils::RelativePath(absPath, context.directories.cprj, withHeadingDot);
    }
  }
  return true;
}

bool ProjMgrWorker::ProcessGroups(ContextItem& context) {
  // Add cproject groups
  for (const auto& group : context.cproject->groups) {
    if (!AddGroup(group, context.groups, context, context.cproject->directory)) {
      return false;
    }
  }
  // Add clayers groups
  for (const auto& [_, clayer] : context.clayers) {
    for (const auto& group : clayer->groups) {
      if (!AddGroup(group, context.groups, context, clayer->directory)) {
        return false;
      }
    }
  }
  return true;
}

bool ProjMgrWorker::AddGroup(const GroupNode& src, vector<GroupNode>& dst, ContextItem& context, const string root) {
  if (CheckContextFilters(src.type, context) && CheckCompiler(src.forCompiler, context.compiler)) {
    std::vector<GroupNode> groups;
    for (const auto& group : src.groups) {
      if (!AddGroup(group, groups, context, root)) {
        return false;
      }
    }
    std::vector<FileNode> files;
    for (const auto& file : src.files) {
      if (!AddFile(file, files, context, root)) {
        return false;
      }
    }
    for (auto& dstNode : dst) {
      if (dstNode.group == src.group) {
        ProjMgrLogger::Error("conflict: group '" + dstNode.group + "' is declared multiple times");
        return false;
      }
    }

    // Replace sequences and/or adjust file relative paths
    BuildType srcNodeBuild = src.build;
    ProcessSequencesRelatives(context, srcNodeBuild, root);
    UpdateMisc(srcNodeBuild.misc, context.toolchain.name);

    dst.push_back({ src.group, src.forCompiler, files, groups, srcNodeBuild, src.type });
  }
  return true;
}

bool ProjMgrWorker::AddFile(const FileNode& src, vector<FileNode>& dst, ContextItem& context, const string root) {
  if (CheckContextFilters(src.type, context) && CheckCompiler(src.forCompiler, context.compiler)) {
    for (auto& dstNode : dst) {
      if (dstNode.file == src.file) {
        ProjMgrLogger::Error("conflict: file '" + dstNode.file + "' is declared multiple times");
        return false;
      }
    }

    // Get file node
    FileNode srcNode = src;

    // Replace sequences and/or adjust file relative paths
    ProcessSequenceRelative(context, srcNode.file, root);
    ProcessSequencesRelatives(context, srcNode.build, root);
    UpdateMisc(srcNode.build.misc, context.toolchain.name);

    // Set file category
    if (srcNode.category.empty()) {
      srcNode.category = ProjMgrUtils::GetCategory(srcNode.file);
    }

    dst.push_back(srcNode);

    // Set linker script
    if ((srcNode.category == "linkerScript") && (context.linker.script.empty() && context.linker.regions.empty() && context.linker.defines.empty())) {
      context.linker.script = srcNode.file;
    }

    // Store absolute file path
    error_code ec;
    const string& filePath = fs::weakly_canonical(root + "/" + srcNode.file, ec).generic_string();
    context.filePaths.insert({ srcNode.file, filePath });
  }
  return true;
}

bool ProjMgrWorker::AddComponent(const ComponentItem& src, const string& layer, vector<pair<ComponentItem, string>>& dst, TypePair type, ContextItem& context) {
  if (CheckContextFilters(src.type, context)) {
    for (auto& [dstNode, layer] : dst) {
      if (dstNode.component == src.component) {
        ProjMgrLogger::Error("conflict: component '" + dstNode.component + "' is declared multiple times");
        return false;
      }
    }
    dst.push_back({ src, layer });
  }
  return true;
}

bool ProjMgrWorker::CheckBoardDeviceInLayer(const ContextItem& context, const ClayerItem& clayer) {
  if (!clayer.forBoard.empty()) {
    BoardItem forBoard, board;
    GetBoardItem(clayer.forBoard, forBoard);
    GetBoardItem(context.board, board);
    if ((!forBoard.vendor.empty() && (forBoard.vendor != board.vendor)) ||
        (!forBoard.name.empty() && (forBoard.name != board.name)) ||
        (!forBoard.revision.empty() && (forBoard.revision != board.revision))) {
      return false;
    }
  }
  if (!clayer.forDevice.empty()) {
    DeviceItem forDevice, device;
    GetDeviceItem(clayer.forDevice, forDevice);
    GetDeviceItem(context.device, device);
    if ((!forDevice.vendor.empty() && (forDevice.vendor != device.vendor)) ||
        (!forDevice.name.empty() && (forDevice.name != device.name)) ||
        (!forDevice.pname.empty() && (forDevice.pname != device.pname))) {
      return false;
    }
  }
  return true;
}

bool ProjMgrWorker::CheckCompiler(const vector<string>& forCompiler, const string& selectedCompiler) {
  if (forCompiler.empty()) {
    return true;
  }
  for (const auto& compiler : forCompiler) {
    if (ProjMgrUtils::AreCompilersCompatible(compiler, selectedCompiler)) {
      return true;
    }
  }
  return false;
}

bool ProjMgrWorker::CheckType(const TypeFilter& typeFilter, const vector<TypePair>& typeVec) {
  const auto& exclude = typeFilter.exclude;
  const auto& include = typeFilter.include;

  if (include.empty()) {
    if (exclude.empty()) {
      return true;
    }
    else {
      // check not-for types
      for (const auto& excType : typeFilter.exclude) {
        for (const auto& type : typeVec) {
          if (((excType.build == type.build) && excType.target.empty()) ||
            ((excType.target == type.target) && excType.build.empty()) ||
            ((excType.build == type.build) && (excType.target == type.target))) {
            return false;
          }
        }
      }
      return true;
    }
  }
  else {
    // check for-types
    for (const auto& incType : typeFilter.include) {
      for (const auto& type : typeVec) {
        if (((incType.build == type.build) && incType.target.empty()) ||
          ((incType.target == type.target) && incType.build.empty()) ||
          ((incType.build == type.build) && (incType.target == type.target))) {
          return true;
        }
      }
    }
    return false;
  }
}

bool ProjMgrWorker::CheckContextFilters(const TypeFilter& typeFilter, const ContextItem& context) {
  vector<TypePair> typeVec = { context.type };
  if (context.csolution) {
    // get mapped contexts types
    vector<ContextName>& buildContextMap = context.csolution->buildTypes[context.type.build].contextMap;
    vector<ContextName>& targetContextMap = context.csolution->targetTypes[context.type.target].build.contextMap;
    for (const auto& contextMap : { buildContextMap, targetContextMap }) {
      for (const auto& mappedContext : contextMap) {
        if ((!mappedContext.project.empty()) && (mappedContext.project != context.cproject->name)) {
          continue;
        }
        typeVec.push_back({ mappedContext.build.empty() ? context.type.build : mappedContext.build,
            mappedContext.target.empty() ? context.type.target : mappedContext.target });
      }
    }
  }
  return CheckType(typeFilter, typeVec);
}

bool ProjMgrWorker::ProcessContext(ContextItem& context, bool loadGpdsc, bool resolveDependencies, bool updateRteFiles) {
  if (!LoadPacks(context)) {
    return false;
  }
  context.rteActiveProject->SetAttribute("update-rte-files", updateRteFiles ? "1" : "0");
  if (!ProcessPrecedences(context)) {
    return false;
  }
  if (!ProcessDevice(context)) {
    return false;
  }
  if (!SetTargetAttributes(context, context.targetAttributes)) {
    return false;
  }
  if (!ProcessLinkerOptions(context)) {
    return false;
  }
  if (!ProcessGroups(context)) {
    return false;
  }
  if (!ProcessComponents(context)) {
    return false;
  }
  if (loadGpdsc) {
    if (!ProcessGpdsc(context)) {
      return false;
    }
  }
  if (!ProcessConfigFiles(context)) {
    return false;
  }
  if (!ProcessComponentFiles(context)) {
    return false;
  }
  if (resolveDependencies) {
    // TODO: Add uniquely identified missing dependencies to RTE Model

    // Get dependency validation results
    if (!ValidateContext(context)) {
      string msg = "dependency validation for context '" + context.name + "' failed:";
      set<string> results;
      FormatValidationResults(results, context);
      for (const auto& result : results) {
        msg += "\n" + result;
      }
      if (context.cproject && !context.cproject->path.empty()) {
        ProjMgrLogger::Warn(context.cproject->path, msg);
      } else {
        ProjMgrLogger::Warn(msg);
      }
    }
  }
  return true;
}

void ProjMgrWorker::ApplyFilter(const vector<string>& origin, const set<string>& filter, vector<string>& result) {
  result.clear();
  for (const auto& item : origin) {
    bool match = true;
    for (const auto& word : filter) {
      if (word.empty()) {
        continue;
      }
      if (item.find(word) == string::npos) {
        match = false;
        break;
      }
    }
    if (match) {
      ProjMgrUtils::PushBackUniquely(result, item);
    }
  }
}

bool ProjMgrWorker::FullMatch(const set<string>& installed, const set<string>& required) {
  for (const auto& requiredWord : required) {
    if (requiredWord.empty()) {
      continue;
    }
    bool fullMatch = false;
    for (const auto& installedWord : installed) {
      if (installedWord.empty()) {
        continue;
      }
      if (requiredWord.compare(installedWord) == 0) {
        fullMatch = true;
        break;
      }
    }
    if (!fullMatch) {
      return false;
    }
  }
  return true;
}

set<string> ProjMgrWorker::SplitArgs(const string& args, const string& delimiter) {
  set<string> s;
  size_t end = 0, start = 0, len = args.length();
  while (end < len) {
    if ((end = args.find_first_of(delimiter, start)) == string::npos) end = len;
    s.insert(args.substr(start, end - start));
    start = end + 1;
  }
  return s;
}

bool ProjMgrWorker::ListPacks(vector<string>&packs, bool missingPacks, const string& filter) {
  set<string> packsSet;
  list<string> pdscFiles;
  std::set<std::string> errMsgs;
  bool reqOk = true;
  if (!InitializeModel()) {
    return false;
  }
  for (const auto& selectedContext : m_selectedContexts) {
    ContextItem& context = m_contexts[selectedContext];
    GetRequiredPdscFiles(context, m_packRoot, errMsgs);
    // Get missing packs identifiers
    for (const auto& pack : context.missingPacks) {
      packsSet.insert(ProjMgrUtils::GetPackageID(pack.vendor, pack.name, pack.version));
    }
    if (!missingPacks) {
      if (context.packRequirements.size() > 0) {
        // Get context required packs
        for (const auto& [pdscFile, _] : context.pdscFiles) {
          ProjMgrUtils::PushBackUniquely(pdscFiles, pdscFile);
        }
      }
    }
  }
  if (!missingPacks) {
    // Check load packs policy
    if (pdscFiles.empty() && (m_loadPacksPolicy == LoadPacksPolicy::REQUIRED)) {
      ProjMgrLogger::Error("required packs must be specified");
      return false;
    }
    if (pdscFiles.empty() || (m_loadPacksPolicy == LoadPacksPolicy::ALL) || (m_loadPacksPolicy == LoadPacksPolicy::LATEST)) {
      // Get installed packs
      const bool latest = (m_loadPacksPolicy == LoadPacksPolicy::LATEST) || (m_loadPacksPolicy == LoadPacksPolicy::DEFAULT);
      m_kernel->GetInstalledPacks(pdscFiles, latest);
    }
    if (!pdscFiles.empty()) {
      // Load packs and get loaded packs identifiers
      m_kernel->LoadAndInsertPacks(m_loadedPacks, pdscFiles);
      for (const auto& pack : m_loadedPacks) {
        packsSet.insert(ProjMgrUtils::GetPackageID(pack) + " (" + pack->GetPackageFileName() + ")");
      }
    }
    if (errMsgs.size() > 0) {
      std::for_each(errMsgs.begin(), errMsgs.end(), [](const auto& errMsg) {ProjMgrLogger::Error(errMsg); });
      reqOk = false;
    }
  }
  vector<string> packsVec(packsSet.begin(), packsSet.end());
  if (!filter.empty()) {
    vector<string> filteredPacks;
    ApplyFilter(packsVec, SplitArgs(filter), filteredPacks);
    if (filteredPacks.empty()) {
      ProjMgrLogger::Error("no pack was found with filter '" + filter + "'");
      return false;
    }
    packsVec = filteredPacks;
  }
  packs.assign(packsVec.begin(), packsVec.end());
  return reqOk;
}

bool ProjMgrWorker::ListBoards(vector<string>& boards, const string& filter) {
  set<string> boardsSet;
  for (const auto& selectedContext : m_selectedContexts) {
    ContextItem& context = m_contexts[selectedContext];
    if (!LoadPacks(context)) {
      return false;
    }
    const RteBoardMap& availableBoards = context.rteFilteredModel->GetBoards();
    for (const auto& [_, board] : availableBoards) {
      const string& boardVendor = board->GetVendorName();
      const string& boardName = board->GetName();
      const string& boardRevision = board->GetRevision();
      const string& boardPack = ProjMgrUtils::GetPackageID(board->GetPackage());
      boardsSet.insert(boardVendor + "::" + boardName + (!boardRevision.empty() ? ":" + boardRevision : "") + " (" + boardPack + ")");
    }
  }
  if (boardsSet.empty()) {
    ProjMgrLogger::Error("no installed board was found");
    return false;
  }
  vector<string> boardsVec(boardsSet.begin(), boardsSet.end());
  if (!filter.empty()) {
    vector<string> matchedBoards;
    ApplyFilter(boardsVec, SplitArgs(filter), matchedBoards);
    if (matchedBoards.empty()) {
      ProjMgrLogger::Error("no board was found with filter '" + filter + "'");
      return false;
    }
    boardsVec = matchedBoards;
  }
  boards.assign(boardsVec.begin(), boardsVec.end());
  return true;
}

bool ProjMgrWorker::ListDevices(vector<string>& devices, const string& filter) {
  set<string> devicesSet;
  for (const auto& selectedContext : m_selectedContexts) {
    ContextItem& context = m_contexts[selectedContext];
    if (!LoadPacks(context)) {
      return false;
    }
    list<RteDevice*> filteredModelDevices;
    context.rteFilteredModel->GetDevices(filteredModelDevices, "", "", RteDeviceItem::VARIANT);
    for (const auto& deviceItem : filteredModelDevices) {
      const string& deviceVendor = deviceItem->GetVendorName();
      const string& deviceName = deviceItem->GetFullDeviceName();
      const string& devicePack = ProjMgrUtils::GetPackageID(deviceItem->GetPackage());
      if (deviceItem->GetProcessorCount() > 1) {
        const auto& processors = deviceItem->GetProcessors();
        for (const auto& processor : processors) {
          devicesSet.insert(deviceVendor + "::" + deviceName + ":" + processor.first + " (" + devicePack + ")");
        }
      } else {
        devicesSet.insert(deviceVendor + "::" + deviceName + " (" + devicePack + ")");
      }
    }
  }
  if (devicesSet.empty()) {
    ProjMgrLogger::Error("no installed device was found");
    return false;
  }
  vector<string> devicesVec(devicesSet.begin(), devicesSet.end());
  if (!filter.empty()) {
    vector<string> matchedDevices;
    ApplyFilter(devicesVec, SplitArgs(filter), matchedDevices);
    if (matchedDevices.empty()) {
      ProjMgrLogger::Error("no device was found with filter '" + filter + "'");
      return false;
    }
    devicesVec = matchedDevices;
  }
  devices.assign(devicesVec.begin(), devicesVec.end());
  return true;
}

bool ProjMgrWorker::ListComponents(vector<string>& components, const string& filter) {
  RteCondition::SetVerboseFlags(m_verbose ? VERBOSE_DEPENDENCY : m_debug ? VERBOSE_FILTER | VERBOSE_DEPENDENCY : 0);
  RteComponentMap componentMap;
  set<string> componentIds;
  for (const auto& selectedContext : m_selectedContexts) {
    ContextItem& context = m_contexts[selectedContext];
    if (!LoadPacks(context)) {
      return false;
    }
    if (!selectedContext.empty()) {
      if (!ProcessPrecedences(context)) {
        return false;
      }
      if (!ProcessDevice(context)) {
        return false;
      }
    }
    if (!SetTargetAttributes(context, context.targetAttributes)) {
      return false;
    }
    RteComponentMap installedComponents = context.rteActiveTarget->GetFilteredComponents();
    if (installedComponents.empty()) {
      if (!selectedContext.empty()) {
        ProjMgrLogger::Error("no component was found for device '" + context.device + "'");
      }
      else {
        ProjMgrLogger::Error("no installed component was found");
      }
      return false;
    }
    for (auto& component : installedComponents) {
      const string& componentId = ProjMgrUtils::GetComponentID(component.second);
      componentIds.insert(componentId);
      componentMap[componentId] = component.second;
    }
  }
  vector<string> componentIdsVec(componentIds.begin(), componentIds.end());
  if (!filter.empty()) {
    vector<string> filteredIds;
    ApplyFilter(componentIdsVec, SplitArgs(filter), filteredIds);
    if (filteredIds.empty()) {
      ProjMgrLogger::Error("no component was found with filter '" + filter + "'");
      return false;
    }
    componentIdsVec = filteredIds;
  }
  for (const auto& componentId : componentIdsVec) {
    components.push_back(componentId + " (" + ProjMgrUtils::GetPackageID(componentMap[componentId]->GetPackage()) + ")");
  }
  return true;
}

bool ProjMgrWorker::ListDependencies(vector<string>& dependencies, const string& filter) {
  RteCondition::SetVerboseFlags(m_verbose ? VERBOSE_DEPENDENCY : m_debug ? VERBOSE_FILTER | VERBOSE_DEPENDENCY : 0);
  set<string>dependenciesSet;
  for (const auto& selectedContext : m_selectedContexts) {
    ContextItem& context = m_contexts[selectedContext];
    if (!ProcessContext(context, true, false, false)) {
      return false;
    }
    if (!ValidateContext(context)) {
      for (const auto& [result, component, expressions, _] : context.validationResults) {
        if ((result == RteItem::MISSING) || (result == RteItem::SELECTABLE)) {
          for (const auto& expression : expressions) {
            dependenciesSet.insert(component + " " + expression);
          }
        }
      }
    }
  }
  vector<string> dependenciesVec(dependenciesSet.begin(), dependenciesSet.end());
  if (!filter.empty()) {
    vector<string> filteredDependencies;
    ApplyFilter(dependenciesVec, SplitArgs(filter), filteredDependencies);
    if (filteredDependencies.empty()) {
      ProjMgrLogger::Error("no unresolved dependency was found with filter '" + filter + "'");
      return false;
    }
    dependenciesVec = filteredDependencies;
  }
  dependencies.assign(dependenciesVec.begin(), dependenciesVec.end());
  return true;
}

bool ProjMgrWorker::FormatValidationResults(set<string>& results, const ContextItem& context) {
  for (const auto& [result, component, expressions, aggregates] : context.validationResults) {
    string resultStr = RteItem::ConditionResultToString(result) + " " + component;
    for (const auto& expression : expressions) {
      resultStr += "\n  " + expression;
    }
    for (const auto& aggregate : aggregates) {
      resultStr += "\n  " + aggregate;
    }
    results.insert(resultStr);
  }
  return true;
}

bool ProjMgrWorker::ListContexts(vector<string>& contexts, const string& filter, const bool ymlOrder) {
  if (m_contexts.empty()) {
    return false;
  }
  vector<string> contextsVec = m_ymlOrderedContexts;
  if (!filter.empty()) {
    vector<string> filteredContexts;
    ApplyFilter(contextsVec, SplitArgs(filter), filteredContexts);
    if (filteredContexts.empty()) {
      ProjMgrLogger::Error("no context was found with filter '" + filter + "'");
      return false;
    }
    contextsVec = filteredContexts;
  }
  contexts.assign(contextsVec.begin(), contextsVec.end());

  if (!ymlOrder) {
    std::sort(contexts.begin(), contexts.end());
  }
  return true;
}

bool ProjMgrWorker::ListGenerators(vector<string>& generators) {
  set<string> generatorsSet;
  for (const auto& selectedContext : m_selectedContexts) {
    ContextItem& context = m_contexts[selectedContext];
    if (!ProcessContext(context, false, true, false)) {
      return false;
    }
    for (const auto& [id, generator] : context.generators) {
      generatorsSet.insert(id + " (" + generator->GetDescription() + ")");
    }
  }
  generators.assign(generatorsSet.begin(), generatorsSet.end());
  return true;
}

bool ProjMgrWorker::ListLayers(vector<string>& layers, const string& clayerSearchPath) {
  map<StrPair, StrSet> layersMap;
  for (const auto& selectedContext : m_selectedContexts) {
    ContextItem& context = m_contexts[selectedContext];
    if (!LoadPacks(context)) {
      return false;
    }
    if (selectedContext.empty()) {
      // get all layers from packs and from search path
      StrVecMap genericClayers;
      if (!CollectLayersFromPacks(context, genericClayers) ||
          !CollectLayersFromSearchPath(clayerSearchPath, genericClayers)) {
        return false;
      }
      for (const auto& [clayerType, clayerVec] : genericClayers) {
        for (const auto& clayer : clayerVec) {
          layersMap[{clayer, clayerType}].insert({});
        }
      }
    } else {
      // process board/device filtering
      if (!ProcessPrecedences(context)) {
        return false;
      }
      if (!ProcessDevice(context)) {
        return false;
      }
      if (!SetTargetAttributes(context, context.targetAttributes)) {
        return false;
      }
      // get matching layers for selected context
      if (!DiscoverMatchingLayers(context, clayerSearchPath)) {
        return false;
      }
      for (const auto& [clayer, clayerItem] : context.clayers) {
        const auto& validSets = GetValidSets(context, clayer);
        layersMap[{clayer, clayerItem->type}].insert(validSets.begin(), validSets.end());
      }
      for (const auto& [clayerType, clayerVec] : context.compatibleLayers) {
        for (const auto& clayer : clayerVec) {
          const auto& validSets = GetValidSets(context, clayer);
          layersMap[{clayer, clayerType}].insert(validSets.begin(), validSets.end());
        }
      }
    }
  }
  // print layer paths, types and valid sets for all selected contexts without duplicates
  for (const auto& [clayerPair, validSets] : layersMap) {
    const auto& clayer = clayerPair.first;
    const auto& type = clayerPair.second;
    string layerEntry = clayer + (type.empty() ? "" : " (layer type: " + type + ")");
    for (const auto& validSet : validSets) {
      layerEntry += "\n  set: " + validSet;
    }
    ProjMgrUtils::PushBackUniquely(layers, layerEntry);
  }
  return true;
}

ToolchainItem ProjMgrWorker::GetToolchain(const string& compiler) {
  ToolchainItem toolchain;
  if (compiler.find("@") != string::npos) {
    toolchain.name = RteUtils::RemoveSuffixByString(compiler, "@");
    toolchain.required = RteUtils::RemovePrefixByString(compiler, "@");
    if (toolchain.required.find(">=") != string::npos) {
      // minimum version
      toolchain.range = toolchain.required.substr(2);
    } else {
      // fixed version
      toolchain.range = toolchain.required + ":" + toolchain.required;
    }
  } else {
    toolchain.name = compiler;
    toolchain.required = ">=0.0.0";
    toolchain.range = "0.0.0";
  }
  return toolchain;
}

bool ProjMgrWorker::GetTypeContent(ContextItem& context) {
  if (!context.type.build.empty() || !context.type.target.empty()) {
    context.controls.build = context.csolution->buildTypes[context.type.build];
    TargetType targetType = context.csolution->targetTypes[context.type.target];
    context.controls.target = targetType.build;
    context.targetItem.board = targetType.board;
    context.targetItem.device = targetType.device;
  }
  context.controls.cproject = context.cproject->target.build;
  context.controls.csolution = context.csolution->target.build;
  for (const auto& [name, clayer] : context.clayers) {
    context.controls.clayers[name] = clayer->target.build;
  }
  return true;
}

bool ProjMgrWorker::GetProjectSetup(ContextItem& context) {
  for (const auto& setup : context.cproject->setups) {
    if (CheckContextFilters(setup.type, context) && CheckCompiler(setup.forCompiler, context.compiler)) {
      context.controls.setups.push_back(setup.build);
    }
  }
  return true;
}

void ProjMgrWorker::UpdateMisc(vector<MiscItem>& vec, const string& compiler) {
  // Filter and adjust vector of MiscItem, leaving a single compiler compatible item
  MiscItem dst;
  dst.forCompiler = compiler;
  AddMiscUniquely(dst, vec);
  vec.clear();
  vec.push_back(dst);
}

void ProjMgrWorker::AddMiscUniquely(MiscItem& dst, vector<vector<MiscItem>*>& vec) {
  for (auto& src : vec) {
    AddMiscUniquely(dst, *src);
  }
}

void ProjMgrWorker::AddMiscUniquely(MiscItem& dst, vector<MiscItem>& vec) {
  for (auto& src : vec) {
    if (ProjMgrUtils::AreCompilersCompatible(src.forCompiler, dst.forCompiler)) {
      // Copy individual flags
      AddStringItemsUniquely(dst.as, src.as);
      AddStringItemsUniquely(dst.c, src.c);
      AddStringItemsUniquely(dst.cpp, src.cpp);
      AddStringItemsUniquely(dst.c_cpp, src.c_cpp);
      AddStringItemsUniquely(dst.link, src.link);
      AddStringItemsUniquely(dst.link_c, src.link_c);
      AddStringItemsUniquely(dst.link_cpp, src.link_cpp);
      AddStringItemsUniquely(dst.lib, src.lib);
      AddStringItemsUniquely(dst.library, src.library);
      // Propagate C-CPP flags
      AddStringItemsUniquely(dst.c, dst.c_cpp);
      AddStringItemsUniquely(dst.cpp, dst.c_cpp);
    }
  }
}

void ProjMgrWorker::AddStringItemsUniquely(vector<string>& dst, const vector<string>& src) {
  for (const auto& value : src) {
    if (find(dst.cbegin(), dst.cend(), value) == dst.cend()) {
      dst.push_back(value);
    }
  }
}

void ProjMgrWorker::RemoveDefines(vector<string>& dst, vector<string>& src) {
  for (const auto& value : src) {
    if (value == "*") {
      dst.clear();
      return;
    }
    const auto& valueIt = find_if(dst.cbegin(), dst.cend(),
      [value](const string& defineStr) -> bool {
        if (defineStr == value) {
          return true;
        }
        auto defKey = RteUtils::GetPrefix(defineStr, '=');
        return (defKey == value);
      });
    if (valueIt != dst.cend()) {
      dst.erase(valueIt);
    }
  }
}

void ProjMgrWorker::RemoveStringItems(vector<string>& dst, vector<string>& src) {
  for (const auto& value : src) {
    if (value == "*") {
      dst.clear();
      return;
    }
    const auto& valueIt = find(dst.cbegin(), dst.cend(), value);
    if (valueIt != dst.cend()) {
      dst.erase(valueIt);
    }
  }
}

bool ProjMgrWorker::GetAccessSequence(size_t& offset, const string& src, string& sequence, const char start, const char end) {
  size_t delimStart = src.find_first_of(start, offset);
  if (delimStart != string::npos) {
    size_t delimEnd = src.find_first_of(end, ++delimStart);
    if (delimEnd != string::npos) {
      sequence = src.substr(delimStart, delimEnd - delimStart);
      offset = ++delimEnd;
      return true;
    } else {
      ProjMgrLogger::Error("missing access sequence delimiter: " + src);
      return false;
    }
  }
  offset = string::npos;
  return true;
}

bool ProjMgrWorker::ExecuteGenerator(std::string& generatorId) {
  if (m_selectedContexts.size() != 1) {
    ProjMgrLogger::Error("a single context must be specified");
    return false;
  }
  const string& selectedContext = m_selectedContexts.front();
  ContextItem& context = m_contexts[selectedContext];
  if (!ProcessContext(context, false)) {
    return false;
  }
  const auto& generators = context.generators;
  if (generators.find(generatorId) == generators.end()) {
    ProjMgrLogger::Error("generator '" + generatorId + "' was not found");
    return false;
  }
  RteGenerator* generator = generators.at(generatorId);

  // Create generate.yml file with context info and destination
  string generatorDestination;
  for (const auto& [gpdsc, item] : context.gpdscs) {
    if (item.generator == generatorId) {
      generatorDestination = item.workingDir;
    }
  }

  // Make sure the generatorDestination is absolute
  if (fs::path(generatorDestination).is_relative()) {
    generatorDestination = context.rteActiveProject->GetProjectPath() + generatorDestination;
  }

  // Make the generatorDestination is a folder by adding a '/' to the end
  if (!generatorDestination.empty() && generatorDestination.back() != '/') {
    generatorDestination += '/';
  }

  if (!ProjMgrYamlEmitter::GenerateCbuild(&context)) {
    return false;
  }

  // TODO: review RteGenerator::GetExpandedCommandLine and variables
  //const string generatorCommand = m_kernel->GetCmsisPackRoot() + "/" + generator->GetPackagePath() + generator->GetCommand();

  // check if generator executable has execute permissions
  const string generatorExe = generator->GetExecutable(context.rteActiveTarget);
  if (generatorExe.empty()) {
    ProjMgrLogger::Error("generator executable '" + generatorId + "' was not found");
    return false;
  }
  if(!RteFsUtils::Exists(generatorExe)) {
    ProjMgrLogger::Error("generator executable file '" + generatorExe + "' does not exist");
    return false;
  }

  if (!RteFsUtils::IsExecutableFile(generatorExe)) {
    ProjMgrLogger::Error("generator file '" + generatorExe + "' cannot be executed, check permissions");
    return false;
  }
  const string generatorCommand = generator->GetExpandedCommandLine(context.rteActiveTarget);

  error_code ec;
  const auto& workingDir = fs::current_path(ec);
  RteFsUtils::CreateDirectories(generatorDestination);
  fs::current_path(generatorDestination, ec);
  ProjMgrUtils::Result result = ProjMgrUtils::ExecCommand(generatorCommand);
  fs::current_path(workingDir, ec);

  ProjMgrLogger::Info("generator '" + generatorId + "' for context '" + selectedContext + "' reported:\n" + result.first);

  if (result.second) {
    ProjMgrLogger::Error("executing generator '" + generatorId + "' for context '" + selectedContext + "' failed");
    return false;
  }
  return true;
}

std::string ProjMgrWorker::GetDeviceInfoString(const std::string& vendor,
  const std::string& name, const std::string& processor) const {
  return vendor + (vendor.empty() ? "" : "::") +
    name + (processor.empty() ? "" : ":" + processor);
}

std::string ProjMgrWorker::GetBoardInfoString(const std::string& vendor,
  const std::string& name, const std::string& revision) const {
  return vendor + (vendor.empty() ? "" : "::") +
    name + (revision.empty() ? "" : ":" + revision);
}

bool ProjMgrWorker::ProcessSequencesRelatives(ContextItem& context, vector<string>& src, const string& ref, bool withHeadingDot) {
  for (auto& item : src) {
    if (!ProcessSequenceRelative(context, item, ref, withHeadingDot)) {
      return false;
    }
  }
  return true;
}

bool ProjMgrWorker::ProcessSequencesRelatives(ContextItem& context, BuildType& build, const string& ref) {
  if (!ProcessSequencesRelatives(context, build.addpaths, ref) ||
      !ProcessSequencesRelatives(context, build.delpaths, ref) ||
      !ProcessSequencesRelatives(context, build.defines)       ||
      !ProcessSequencesRelatives(context, build.undefines))    {
    return false;
  }
  for (auto& misc : build.misc) {
    if (!ProcessSequencesRelatives(context, misc.as,       "", true) ||
        !ProcessSequencesRelatives(context, misc.c,        "", true) ||
        !ProcessSequencesRelatives(context, misc.cpp,      "", true) ||
        !ProcessSequencesRelatives(context, misc.c_cpp,    "", true) ||
        !ProcessSequencesRelatives(context, misc.lib,      "", true) ||
        !ProcessSequencesRelatives(context, misc.library,  "", true) ||
        !ProcessSequencesRelatives(context, misc.link,     "", true) ||
        !ProcessSequencesRelatives(context, misc.link_c,   "", true) ||
        !ProcessSequencesRelatives(context, misc.link_cpp, "", true)) {
      return false;
    }
  }
  return true;
}

bool ProjMgrWorker::ParseContextSelection(const vector<string>& contextSelection) {
  vector<string> contexts;
  ListContexts(contexts);
  const auto& errContextFilters = ProjMgrUtils::GetSelectedContexts(m_selectedContexts, contexts, contextSelection);
  if (errContextFilters.size() != 0) {
    string errMsg = "following context(s) was not found:\n";
    for (const auto& filter : errContextFilters) {
      errMsg += "  " + filter + "\n";
    }
    ProjMgrLogger::Error(errMsg);
    return false;
  }
  return true;
}

bool ProjMgrWorker::IsContextSelected(const string& context) {
  if (find(m_selectedContexts.begin(), m_selectedContexts.end(), context) != m_selectedContexts.end()) {
    return true;
  }
  return false;
}

string ProjMgrWorker::ExpandString(const string& src, const StrMap& variables) {
  string ret = src;
  if (regex_match(ret, regex(".*\\$.*\\$.*"))) {
    for (const auto& [varName, replacement] : variables) {
      const string var = "$" + varName + "$";
      size_t index = 0;
      while ((index = ret.find(var)) != string::npos) {
        ret.replace(index, var.length(), replacement);
      }
    }
  }
  return ret;
}

bool ProjMgrWorker::ListToolchains(vector<ToolchainItem>& toolchains) {
  bool allSupported = true;
  for (const auto& selectedContext : m_selectedContexts) {
    ContextItem& context = m_contexts[selectedContext];
    if (selectedContext.empty()) {
      // list registered toolchains
      GetRegisteredToolchains();
      toolchains = m_toolchains;
      return true;
    }
    // list required toolchains for selected contexts
    if (!LoadPacks(context)) {
      return false;
    }
    if (!ProcessPrecedences(context)) {
      return false;
    }
    if (!context.toolchain.name.empty()) {
      PushBackUniquely(toolchains, context.toolchain);
    }
    if (context.toolchain.config.empty() || context.toolchain.root.empty()) {
      allSupported = false;
    }
  }
  return allSupported;
}

bool ProjMgrWorker::ListEnvironment(EnvironmentList& env) {
  env.cmsis_pack_root = GetPackRoot();
  env.cmsis_compiler_root = GetCompilerRoot();
  return true;
}

void ProjMgrWorker::GetRegisteredToolchains(void) {
  if (!m_toolchains.empty()) {
    return;
  }
  // extract toolchain info from environment variables
  map<string, map<string, string>> registeredToolchains;
  static const regex regEx = regex("(\\w+)_TOOLCHAIN_(\\d+)_(\\d+)_(\\d+)=(.*)");
  for (const auto& envVar : m_envVars) {
    smatch sm;
    try {
      regex_match(envVar, sm, regEx);
    } catch (exception&) {};
    if (sm.size() == 6) {
      registeredToolchains[sm[1]][string(sm[2]) + '.' + string(sm[3]) + '.' + string(sm[4])] = sm[5];
    }
  }
  // iterate over registered toolchains
  for (const auto& [toolchainName, toolchainVersions] : registeredToolchains) {
    for (const auto& [toolchainVersion, toolchainRoot] : toolchainVersions) {
      if (RteFsUtils::Exists(toolchainRoot)) {
        // check whether a config file is available for the registered version
        string configPath, configVersion;
        if (GetToolchainConfig(toolchainName, "0.0.0:" + toolchainVersion, configPath, configVersion)) {
          m_toolchains.push_back({toolchainName, toolchainVersion, "", "", toolchainRoot, configPath});
        }
      }
    }
  }
}

bool ProjMgrWorker::GetLatestToolchain(ToolchainItem& toolchain) {
  // get latest toolchain version
  GetRegisteredToolchains();
  bool found = false;
  for (const auto& registeredToolchain : m_toolchains) {
    if ((toolchain.name == registeredToolchain.name) &&
      (VersionCmp::RangeCompare(registeredToolchain.version, toolchain.range) == 0)) {
      toolchain.version = registeredToolchain.version;
      toolchain.config = registeredToolchain.config;
      toolchain.root = registeredToolchain.root;
      found = true;
    }
  }
  return found;
}

bool ProjMgrWorker::GetToolchainConfig(const string& toolchainName, const string& toolchainVersion, string& configPath, string& selectedConfigVersion) {
  // get toolchain configuration files
  const string& compilerRoot = GetCompilerRoot();
  StrVec toolchainConfigFiles;
  error_code ec;
  for (auto const& entry : fs::recursive_directory_iterator(compilerRoot, ec)) {
    string extn = entry.path().extension().string();
    if (entry.path().extension().string() != ".cmake") {
      continue;
    }
    toolchainConfigFiles.push_back(entry.path().generic_string());
  }
  // find greatest compatible file
  bool found = false;
  static const regex regEx = regex("(\\w+)\\.(\\d+\\.\\d+\\.\\d+)");
  for (const auto& file : toolchainConfigFiles) {
    smatch sm;
    const string& stem = fs::path(file).stem().generic_string();
    try {
      regex_match(stem, sm, regEx);
    } catch (exception&) {};
    if (sm.size() == 3) {
      const string& configName = sm[1];
      const string& configVersion = sm[2];
      if ((configName.compare(toolchainName) == 0) &&
        (toolchainVersion.empty() || (VersionCmp::RangeCompare(configVersion, toolchainVersion) == 0)) &&
        (VersionCmp::Compare(selectedConfigVersion, configVersion) <= 0)) {
        selectedConfigVersion = configVersion;
        configPath = file;
        found = true;
      }
    }
  }
  return found;
}

string ProjMgrWorker::GetCompilerRoot(void) {
  if (m_compilerRoot.empty()) {
    ProjMgrUtils::GetCompilerRoot(m_compilerRoot);
  }
  return m_compilerRoot;
}

void ProjMgrWorker::PushBackUniquely(ConnectionsCollectionVec& vec, const ConnectionsCollection& value) {
  for (const auto& item : vec) {
    if ((value.filename == item.filename) && (value.connections == item.connections)) {
      return;
    }
  }
  vec.push_back(value);
}

void ProjMgrWorker::PushBackUniquely(vector<ToolchainItem>& vec, const ToolchainItem& value) {
  for (const auto& item : vec) {
    if ((value.name == item.name) && (value.required == item.required)) {
      return;
    }
  }
  vec.push_back(value);
}

bool ProjMgrWorker::IsConnectionSubset(const ConnectionsCollection& connectionSubset, const ConnectionsCollection& connectionSuperset) {
  if ((connectionSubset.type == connectionSuperset.type) &&
    (connectionSubset.filename == connectionSuperset.filename)) {
    ConnectPtrVec subset = connectionSubset.connections;
    ConnectPtrVec superset = connectionSuperset.connections;
    sort(subset.begin(), subset.end());
    sort(superset.begin(), superset.end());
    return includes(superset.begin(), superset.end(), subset.begin(), subset.end());
  }
  return false;
}

bool ProjMgrWorker::IsCollectionSubset(const ConnectionsCollectionVec& collectionSubset, const ConnectionsCollectionVec& collectionSuperset) {
  for (const auto& subset : collectionSubset) {
    bool isSubset = false;
    for (const auto& superset : collectionSuperset) {
      if (IsConnectionSubset(subset, superset)) {
        isSubset = true;
        break;
      }
    }
    if (!isSubset) {
      return false;
    }
  }
  return true;
}

void ProjMgrWorker::RemoveRedundantSubsets(std::vector<ConnectionsCollectionVec>& validConnections) {
  const auto connections = validConnections;
  auto it = validConnections.begin();
  for (const auto& collection : connections) {
    bool isSubset = false;
    for (const auto& otherCollection : connections) {
      if (&collection == &otherCollection) {
        continue;
      }
      if (IsCollectionSubset(collection, otherCollection)) {
        isSubset = true;
        break;
      }
    }
    if (isSubset) {
      it = validConnections.erase(it);
    } else {
      it++;
    }
  }
}

StrSet ProjMgrWorker::GetValidSets(ContextItem& context, const string& clayer) {
  StrSet validSets;
  for (const auto& combination : context.validConnections) {
    for (const auto& item : combination) {
      if (item.filename == clayer) {
        for (const auto& connect : item.connections) {
          if (!connect->set.empty()) {
            validSets.insert(connect->set + " (" + connect->connect + (connect->info.empty() ? "" : " - " + connect->info) + ")");
          }
        }
      }
    }
  }
  return validSets;
}

bool ProjMgrWorker::ProcessOutputFilenames(ContextItem& context) {
  // get base name and output types from project and project setups
  context.outputTypes = {};
  context.cproject->output.baseName = ExpandString(context.cproject->output.baseName, context.variables);
  string baseName;
  StringCollection baseNameCollection = {
    &baseName,
    {
      &context.cproject->output.baseName,
    },
  };
  for (const auto& type : context.cproject->output.type) {
    ProjMgrUtils::SetOutputType(type, context.outputTypes);
  }
  for (auto& setup : context.cproject->setups) {
    if (CheckContextFilters(setup.type, context) && CheckCompiler(setup.forCompiler, context.compiler)) {
      setup.output.baseName = ExpandString(setup.output.baseName, context.variables);
      baseNameCollection.elements.push_back(&setup.output.baseName);
      for (const auto& type : setup.output.type) {
        ProjMgrUtils::SetOutputType(type, context.outputTypes);
      }
    }
  }
  if (!ProcessPrecedence(baseNameCollection)) {
    return false;
  }
  if (baseName.empty()) {
    baseName = context.cproject->name;
  }

  // secure project requires cmse output type
  if (context.controls.processed.processor.trustzone == "secure") {
    context.outputTypes.cmse.on = true;
  }

  // check type conflict
  if (context.outputTypes.lib.on &&
    (context.outputTypes.elf.on || context.outputTypes.hex.on || context.outputTypes.bin.on)) {
    ProjMgrLogger::Error("output 'lib' is incompatible with other output types");
    return false;
  }

  // default: elf
  if (!context.outputTypes.lib.on && !context.outputTypes.elf.on) {
    context.outputTypes.elf.on = true;
  }

  // set output filename for each required output type
  const string toolchain = affixesMap.find(context.toolchain.name) != affixesMap.end() ? context.toolchain.name : "";
  if (context.outputTypes.elf.on) {
    context.outputTypes.elf.filename = baseName + get<0>(affixesMap.at(toolchain));
  }
  if (context.outputTypes.lib.on) {
    context.outputTypes.lib.filename = get<1>(affixesMap.at(toolchain)) + baseName + get<2>(affixesMap.at(toolchain));
  }
  if (context.outputTypes.hex.on) {
    context.outputTypes.hex.filename = baseName + ".hex";
  }
  if (context.outputTypes.bin.on) {
    context.outputTypes.bin.filename = baseName + ".bin";
  }
  if (context.outputTypes.cmse.on) {
    context.outputTypes.cmse.filename = baseName + "_CMSE_Lib.o";
  }
  return true;
}

bool ProjMgrWorker::GetGeneratorDir(const RteGenerator* generator, ContextItem& context, const string& layer, string& genDir) {
  const string generatorId = generator->GetID();
  genDir.clear();

  // map with GeneratorsItem and base reference (clayer, cproject or csolution directory)
  const vector<pair<GeneratorsItem, string>> generatorsList = {
    layer.empty() ? pair<GeneratorsItem, string>() : make_pair(context.clayers.at(layer)->generators, context.clayers.at(layer)->directory),
    { context.cproject->generators, context.cproject->directory },
    { context.csolution->generators, context.csolution->directory }
  };

  // find first custom directory associated to generatorId in the order: clayer > cproject > csolution
  for (const auto& [generators, ref] : generatorsList) {
    if (generators.options.find(generatorId) != generators.options.end()) {
      genDir = generators.options.at(generatorId);
      if (!genDir.empty()) {
        if (!ProcessSequenceRelative(context, genDir, ref)) {
          return false;
        }
        genDir = RteFsUtils::RelativePath(fs::path(context.directories.cprj).append(genDir).generic_string(), context.cproject->directory);
        break;
      }
    }
  }
  if (genDir.empty()) {
    // find custom base directory in the order: clayer > cproject > csolution
    for (const auto& [generators, ref] : generatorsList) {
      if (!generators.baseDir.empty()) {
        genDir = generators.baseDir;
        if (!ProcessSequenceRelative(context, genDir, ref)) {
          return false;
        }
        genDir = RteFsUtils::RelativePath(fs::path(context.directories.cprj).append(genDir).append(generatorId).generic_string(), context.cproject->directory);
        break;
      }
    }
  }
  if (genDir.empty()) {
    // original working dir from PDSC
    if (!generator->GetWorkingDir().empty()) {
      genDir = RteFsUtils::RelativePath(generator->GetExpandedWorkingDir(context.rteActiveTarget), context.cproject->directory);
      if (!layer.empty()) {
        // adjust genDir if component belongs to layer
        genDir = RteFsUtils::RelativePath(fs::path(context.clayers.at(layer)->directory).append(genDir).generic_string(), context.cproject->directory);
      }
    } else {
      // default: $ProjectDir()$/generated/<generator-id>
      genDir = fs::path("generated").append(generatorId).generic_string();
    }
  }
  return true;
}

bool ProjMgrWorker::ListConfigFiles(vector<string>& configFiles) {
  for (const auto& contextName : m_selectedContexts) {
    const auto& context = m_contexts.at(contextName);
    if (!context.configFiles.empty()) {
      for (const auto& [component, fileInstances] : context.configFiles) {
        string configEntry = component + ":";
        for (const auto& fi : fileInstances) {
          // get absolute path to file instance
          const string absFile = fs::path(context.cproject->directory).append(fi.second->GetInstanceName()).generic_string();
          configEntry += "\n    - " + absFile;
          // get base version
          const string baseVersion = fi.second->GetVersionString();
          configEntry += " (base@" + baseVersion + ")";
          // get update version
          const string updateVersion = fi.second->GetFile(context.rteActiveTarget->GetName())->GetVersionString();
          if (updateVersion != baseVersion) {
            configEntry += " (update@" + updateVersion + ")";
          }
        }
        ProjMgrUtils::PushBackUniquely(configFiles, configEntry);
      }
    }
  }
  return true;
}