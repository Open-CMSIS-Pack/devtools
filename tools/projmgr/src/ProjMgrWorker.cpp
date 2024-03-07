/*
 * Copyright (c) 2020-2024 Arm Limited. All rights reserved.
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
  RteConstants::AS_SOLUTION_DIR + "|" +
  RteConstants::AS_PROJECT_DIR  + "|" +
  RteConstants::AS_OUT_DIR      + "|" +
  RteConstants::AS_BIN          + "|" +
  RteConstants::AS_ELF          + "|" +
  RteConstants::AS_HEX          + "|" +
  RteConstants::AS_LIB          + "|" +
  RteConstants::AS_CMSE         + ")" +
  "\\((.*)\\)$"
);

static const map<const string, tuple<const string, const string, const string>> affixesMap = {
  { ""   ,   {RteConstants::DEFAULT_ELF_SUFFIX, RteConstants::DEFAULT_LIB_PREFIX, RteConstants::DEFAULT_LIB_SUFFIX }},
  { "AC6",   {RteConstants::AC6_ELF_SUFFIX    , RteConstants::AC6_LIB_PREFIX    , RteConstants::AC6_LIB_SUFFIX     }},
  { "GCC",   {RteConstants::GCC_ELF_SUFFIX    , RteConstants::GCC_LIB_PREFIX    , RteConstants::GCC_LIB_SUFFIX     }},
  { "CLANG", {RteConstants::GCC_ELF_SUFFIX    , RteConstants::GCC_LIB_PREFIX    , RteConstants::GCC_LIB_SUFFIX     }},
  { "IAR",   {RteConstants::IAR_ELF_SUFFIX    , RteConstants::IAR_LIB_PREFIX    , RteConstants::IAR_LIB_SUFFIX     }},
};

ProjMgrWorker::ProjMgrWorker(ProjMgrParser* parser, ProjMgrExtGenerator* extGenerator) :
  m_parser(parser),
  m_extGenerator(extGenerator),
  m_loadPacksPolicy(LoadPacksPolicy::DEFAULT),
  m_checkSchema(false),
  m_verbose(false),
  m_debug(false),
  m_dryRun(false),
  m_relativePaths(false)

{
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
    AddContext(descriptor, { "" }, context);
    return true;
  }

  // No build-types
  if (context.csolution->buildTypes.empty()) {
    for (const auto& targetTypeItem : context.csolution->targetTypes) {
      AddContext(descriptor, { "", targetTypeItem.first }, context);
    }
    return true;
  }

  // Add contexts for project x build-type x target-type combinations
  for (const auto& buildTypeItem : context.csolution->buildTypes) {
    for (const auto& targetTypeItem : context.csolution->targetTypes) {
      AddContext(descriptor, { buildTypeItem.first, targetTypeItem.first }, context);
    }
  }
  return true;
}

void ProjMgrWorker::AddContext(ContextDesc& descriptor, const TypePair& type, ContextItem& parentContext) {
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

    context.directories.cprj = RteFsUtils::MakePathCanonical(RteFsUtils::AbsolutePath(context.directories.cprj).generic_string());

    // context variables
    context.variables[RteConstants::AS_SOLUTION] = context.csolution->name;
    context.variables[RteConstants::AS_PROJECT] = context.cproject->name;
    context.variables[RteConstants::AS_BUILD_TYPE] = context.type.build;
    context.variables[RteConstants::AS_TARGET_TYPE] = context.type.target;

    CollectionUtils::PushBackUniquely(m_ymlOrderedContexts, context.name);
    m_contexts[context.name] = context;
  }
}

bool ProjMgrWorker::ParseContextLayers(ContextItem& context) {
  // user defined variables
  auto userVariablesList = {
    context.csolution->target.build.variables,
    context.csolution->buildTypes[context.type.build].variables,
    context.csolution->targetTypes[context.type.target].build.variables,
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
      error_code ec;
      string const& clayerRef = RteUtils::ExpandAccessSequences(clayer.layer, context.variables);
      string const& clayerFile = fs::canonical(fs::path(context.cproject->directory).append(clayerRef), ec).generic_string();
      if (clayerFile.empty()) {
        if (regex_match(clayer.layer, regex(".*\\$.*\\$.*"))) {
          ProjMgrLogger::Warn(clayer.layer, "variable was not defined for context '" + context.name +"'");
        } else {
          ProjMgrLogger::Error(clayer.layer, "clayer file was not found");
          return false;
        }
      } else {
        if (!m_parser->ParseClayer(clayerFile, m_checkSchema)) {
          return false;
        }
        context.clayers[clayerFile] = &m_parser->GetClayers().at(clayerFile);
      }
    }
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

void ProjMgrWorker::SetRootDir(const std::string& rootDir) {
  m_rootDir = rootDir;
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

void ProjMgrWorker::SetDryRun(bool dryRun) {
  m_dryRun = dryRun;
}

void ProjMgrWorker::SetPrintRelativePaths(bool bRelativePaths) {
  m_relativePaths = bRelativePaths;
}


void ProjMgrWorker::SetLoadPacksPolicy(const LoadPacksPolicy& policy) {
  m_loadPacksPolicy = policy;
}

void ProjMgrWorker::SetEnvironmentVariables(const StrVec& envVars) {
  m_envVars = envVars;
}

bool ProjMgrWorker::CollectRequiredPdscFiles(ContextItem& context, const std::string& packRoot)
{
  if (!ProcessPackages(context, packRoot)) {
    return false;
  }
  bool errFound = false;
  for (auto packItem : context.packRequirements) {
    // parse required version range
    const auto& pack = packItem.pack;
    const auto& reqVersion = pack.version;
    string reqVersionRange = ProjMgrUtils::ConvertToVersionRange(reqVersion);

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
            errFound = true;
            m_contextErrMap[context.name].insert("required pack: " + packageName + " not installed");
            context.missingPacks.push_back(filteredPack);
          }
          continue;
        }
        context.pdscFiles.insert({ pdscFile, {"", reqVersionRange } });
      }
      if (bPackFilter && context.pdscFiles.empty()) {
        std::string filterStr = pack.vendor +
          (pack.name.empty() ? "" : "::" + pack.name) +
          (reqVersion.empty() ? "" : "@" + reqVersion);
        m_contextErrMap[context.name].insert("no match found for pack filter: " + filterStr);
        errFound = true;
      }
    }
    else {
      if (!reqVersion.empty()) {
        m_contextErrMap[context.name].insert("pack '" + (pack.vendor.empty() ? "" : pack.vendor + "::") + pack.name
          + "' specified with 'path' must not have a version");
        errFound = true;
      }
      string packPath = packItem.path;
      if (!RteFsUtils::Exists(packPath)) {
        m_contextErrMap[context.name].insert("pack path: " + packItem.path + " does not exist");
        errFound = true;
        break;
      }
      string pdscFile = pack.vendor + '.' + pack.name + ".pdsc";
      RteFsUtils::NormalizePath(pdscFile, packPath + "/");
      if (!RteFsUtils::Exists(pdscFile)) {
        m_contextErrMap[context.name].insert("pdsc file was not found in: " + packItem.path);
        errFound = true;
        break;
      }
      else {
        context.pdscFiles.insert({ pdscFile, {packPath, reqVersionRange} });
      }
    }
  }
  return !errFound;
}

string ProjMgrWorker::GetPackRoot() {
  string packRoot;
  packRoot = CrossPlatformUtils::GetEnv("CMSIS_PACK_ROOT");
  if (packRoot.empty()) {
    packRoot = CrossPlatformUtils::GetDefaultCMSISPackRootDir();
  }
  packRoot = RteFsUtils::MakePathCanonical(packRoot);
  return packRoot;
}

bool ProjMgrWorker::InitializeModel() {
  if(m_kernel) {
    return true; // already initialized
  }
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
  return m_kernel->Init();
}

bool ProjMgrWorker::LoadAllRelevantPacks() {
  // Get required pdsc files
  std::list<std::string> pdscFiles;
  if (m_selectedContexts.empty()) {
    for (const auto& [context,_] : m_contexts) {
      m_selectedContexts.push_back(context);
    }
  }
  bool success = true;
  m_contextErrMap.clear();
  for (const auto& context : m_selectedContexts) {
    auto& contextItem = m_contexts.at(context);
    if (!CollectRequiredPdscFiles(contextItem, m_packRoot)) {
      success &= false;
      continue;
    }
    for (const auto& [pdscFile, pathVer] : contextItem.pdscFiles) {
      const string& path = pathVer.first;
      if (!path.empty()) {
        CollectionUtils::PushBackUniquely(pdscFiles, pdscFile);
      }
    }
    // then all others
    for (const auto& [pdscFile, pathVer] : contextItem.pdscFiles) {
      const string& path = pathVer.first;
      if (path.empty()) {
        CollectionUtils::PushBackUniquely(pdscFiles, pdscFile);
      }
    }
  }
  if (!success) {
    return false;
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
    PrintContextErrors(context.name);
    return false;
  }
  if (!InitializeTarget(context)) {
    return false;
  }
  // Filter context specific packs
  set<string> selectedPacks;
  const bool allOrLatest = (m_loadPacksPolicy == LoadPacksPolicy::ALL) || (m_loadPacksPolicy == LoadPacksPolicy::LATEST);
  for (const auto& pack : m_loadedPacks) {
    if (allOrLatest || (context.pdscFiles.find(pack->GetPackageFileName()) != context.pdscFiles.end())) {
      selectedPacks.insert(pack->GetPackageID());
    }
  }
  RtePackageFilter filter;
  filter.SetSelectedPackages(selectedPacks);
  context.rteActiveTarget->SetPackageFilter(filter);
  context.rteActiveTarget->UpdateFilterModel();

  RtePackageMap allRequiredPacks;
  // check if all pack requirements are fulfilled
  for (auto pack : m_loadedPacks) {
    pack->GetRequiredPacks(allRequiredPacks, m_model);
  }
  for (auto [id, pack] : allRequiredPacks) {
    if (!pack) {
      string msg("context '");
      msg += context.name;
      msg += "': required pack '";
      msg += id + "' is not loaded";
      ProjMgrLogger::Warn(msg);
    }
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
    if (!item.filename.empty()) {
      combination.push_back(item);
    }
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
  std::vector<ConnectPtrVec>& combinations) {
  // combine items from a vector of 'select' nodes
  // see an example in the test case ProjMgrWorkerUnitTests.GetAllSelectCombinations
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
    CollectionUtils::PushBackUniquely(clayers[clayerItem->GetTypeString()], clayerFile);
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
          CollectionUtils::PushBackUniquely(clayers[clayer->type], clayerFile);
        }
      }
    }
  }
  return true;
}

void ProjMgrWorker::GetRequiredLayerTypes(ContextItem& context, LayersDiscovering& discover) {
  for (const auto& clayer : context.cproject->clayers) {
    if (clayer.type.empty() || !CheckContextFilters(clayer.typeFilter, context) ||
      (RteUtils::ExpandAccessSequences(clayer.layer, context.variables) != clayer.layer)) {
      continue;
    }
    discover.requiredLayerTypes.push_back(clayer.type);
    discover.optionalTypeFlags[clayer.type] = clayer.optional;
  }
}

bool ProjMgrWorker::ProcessCandidateLayers(ContextItem& context, LayersDiscovering& discover) {
  // get all candidate layers
  if (!GetCandidateLayers(discover)) {
    return false;
  }
  // load device/board specific packs specified in candidate layers
  vector<PackItem> packRequirements;
  for (const auto& [type, clayers] : discover.candidateClayers) {
    for (const auto& clayer : clayers) {
      const ClayerItem& clayerItem = m_parser->GetGenericClayers()[clayer];
      if (!clayerItem.forBoard.empty() || !clayerItem.forDevice.empty()) {
        InsertPackRequirements(clayerItem.packs, packRequirements, clayerItem.directory);
      }
    }
  }
  if (packRequirements.size() > 0) {
    AddPackRequirements(context, packRequirements);
    if (!LoadAllRelevantPacks() || !LoadPacks(context)) {
      PrintContextErrors(context.name);
      return false;
    }
  }
  // process board/device filtering
  if (!ProcessDevice(context)) {
    return false;
  }
  if (!SetTargetAttributes(context, context.targetAttributes)) {
    return false;
  }
  // recollect layers from packs after filtering
  discover.genericClayersFromPacks.clear();
  if (!CollectLayersFromPacks(context, discover.genericClayersFromPacks)) {
    return false;
  }
  discover.candidateClayers.clear();
  if (!GetCandidateLayers(discover)) {
    return false;
  }
  return true;
}

bool ProjMgrWorker::GetCandidateLayers(LayersDiscovering& discover) {
  // clayers matching required types
  StrVecMap genericClayers = CollectionUtils::MergeStrVecMap(discover.genericClayersFromSearchPath, discover.genericClayersFromPacks);
  for (const auto& requiredType : discover.requiredLayerTypes) {
    if (genericClayers.find(requiredType) != genericClayers.end()) {
      for (const auto& clayer : genericClayers.at(requiredType)) {
        discover.candidateClayers[requiredType].push_back(clayer);
      }
    } else {
      CollectionUtils::PushBackUniquely(discover.missedRequiredTypes, requiredType);
    }
  }
  // parse matched type layers
  for (const auto& [type, clayers] : discover.candidateClayers) {
    for (const auto& clayer : clayers) {
      if (!m_parser->ParseGenericClayer(clayer, m_checkSchema)) {
        return false;
      }
    }
  }
  return true;
}

bool ProjMgrWorker::DiscoverMatchingLayers(ContextItem& context, string clayerSearchPath) {
  // get all layers from packs and from search path
  LayersDiscovering discover;
  if (!CollectLayersFromPacks(context, discover.genericClayersFromPacks)) {
    return false;
  }
  if (!CollectLayersFromSearchPath(clayerSearchPath, discover.genericClayersFromSearchPath)) {
    return false;
  }
  // get required layer types
  GetRequiredLayerTypes(context, discover);
  // process candidate layers
  if (!ProcessCandidateLayers(context, discover)) {
    return false;
  }
  // process layer combinations
  if (!ProcessLayerCombinations(context, discover)) {
    return false;
  }
  return true;
}

bool ProjMgrWorker::ProcessLayerCombinations(ContextItem& context, LayersDiscovering& discover) {
  // debug message
  string debugMsg;
  if (m_debug) {
    debugMsg = "check for context '" + context.name + "'\n";

    for (const auto& missedRequiredType : discover.missedRequiredTypes) {
      debugMsg += "no clayer matches type '" + missedRequiredType + "'\n";
    }
  }

  // collect connections from candidate layers
  ConnectionsCollectionVec allConnections;
  if (!discover.requiredLayerTypes.empty()) {
    for (const auto& [type, clayers] : discover.candidateClayers) {
      for (const auto& clayer : clayers) {
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
  ConnectionsCollectionMap classifiedConnections = ClassifyConnections(allConnections, discover.optionalTypeFlags);

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
    ConnectionsValidationResult result = ValidateConnections(combination);

    // update list of compatible layers
    if (result.valid) {
      context.validConnections.push_back(combination);
      for (const auto& [type, _] : discover.candidateClayers) {
        for (const auto& collection : combination) {
          if (collection.type == type) {
            CollectionUtils::PushBackUniquely(context.compatibleLayers[type], collection.filename);
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
  if (!discover.candidateClayers.empty()) {
    if (!context.compatibleLayers.empty()) {
      for (const auto& [type, _] : discover.candidateClayers) {
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

  if (!discover.candidateClayers.empty() && context.compatibleLayers.empty()) {
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
      msg += "connections provided multiple times:";
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

    if (!result.missedCollections.empty()) {
      msg += "provided combined connections not consumed:";
      for (const auto& missedCollection : result.missedCollections) {
        msg += "\n  " + missedCollection.filename + (missedCollection.type.empty() ? "" : " (layer type: " + missedCollection.type + ")");
        for (const auto& connect : missedCollection.connections) {
          for (const auto& provided : connect->provides) {
            msg += "\n    " + provided.first;
          }
        }
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

ConnectionsCollectionMap ProjMgrWorker::ClassifyConnections(const ConnectionsCollectionVec& connections, map<string, bool> optionalTypeFlags) {
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
  // add empty connection for optional handling in combinatory flow, unless differently specified
  for (auto& [type, collectionVec] : classifiedConnections) {
    if (optionalTypeFlags[type]) {
      collectionVec.push_back({ RteUtils::EMPTY_STRING, RteUtils::EMPTY_STRING });
    }
  }

  return classifiedConnections;
}

void ProjMgrWorker::GetConsumesProvides(const ConnectionsCollectionVec& collection, ConnectionsList& connections) {
  // collect consumed and provided connections
  ConnectPtrVec visitedConnect;
  for (const auto& item : collection) {
    for (const auto& connect : item.connections) {
      if (find(visitedConnect.begin(), visitedConnect.end(), connect) != visitedConnect.end()) {
        continue;
      }
      visitedConnect.push_back(connect);
      for (const auto& consumed : connect->consumes) {
        connections.consumes.push_back(&consumed);
      }
      for (const auto& provided : connect->provides) {
        connections.provides.push_back(&provided);
      }
    }
  }
}

bool ProjMgrWorker::ProvidedConnectionsMatch(ConnectionsCollection collection, ConnectionsList connections) {
  // for a given collection check if provided connections match at least a consumed one
  if (collection.connections.size() == 0) {
    return true;
  }
  for (const auto& connect : collection.connections) {
    if (connect->provides.size() == 0) {
      return true;
    }
    for (const auto& provided : connect->provides) {
      for (const auto& consumed : connections.consumes) {
        if (provided.first == (*consumed).first) {
          return true;
        }
      }
    }
  }
  return false;
}

ConnectionsValidationResult ProjMgrWorker::ValidateConnections(ConnectionsCollectionVec combination) {
  // get connections
  ConnectionsList connections;
  GetConsumesProvides(combination, connections);

  // elaborate provided list
  StrMap providedValues;
  StrVec conflicts;
  vector<ConnectionsCollection> missedCollections;
  for (const auto& provided : connections.provides) {
    const auto& key = provided->first;
    const auto& value = provided->second;
    if ((providedValues.find(key) != providedValues.end())) {
      // connection is provided multiple times
      CollectionUtils::PushBackUniquely(conflicts, key);
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
      consumedAddedValues[id] += RteUtils::StringToInt(value, 0);
      it = connections.consumes.erase(it);
    } else {
      it++;
    }
  }

  // validate consumedAddedValues against provided values
  StrPairVec overflows;
  for (const auto& [consumedKey, consumedValue] : consumedAddedValues) {
    const int providedValue = providedValues.find(consumedKey) == providedValues.end() ? 0 :
      RteUtils::StringToInt(providedValues.at(consumedKey), 0);
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
  // validate provided connections of each combination match at least one consumed
  for (const auto& collection : combination) {
    if (!ProvidedConnectionsMatch(collection, connections)) {
      missedCollections.push_back(collection);
    }
  }

  // set results
  bool result = !conflicts.empty() || !overflows.empty() || !incompatibles.empty() || !missedCollections.empty() ? false : true;
  return { result, conflicts, overflows, incompatibles, missedCollections, connections.provides };
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

    context.boardPack = matchedBoard->GetPackage();
    if (context.boardPack) {
      context.packages.insert({ context.boardPack->GetID(), context.boardPack });
    }
    context.targetAttributes["Bname"]    = matchedBoard->GetName();
    context.targetAttributes["Bvendor"]  = matchedBoard->GetVendorName();
    context.targetAttributes["Brevision"] = matchedBoard->GetRevision();
    context.targetAttributes["Bversion"] = matchedBoard->GetRevision(); // deprecated

    // find device from the matched board
    Collection<RteItem*> mountedDevices;
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

  const auto& attr = context.controls.processed.processor;

  // Check attributes support compatibility
  CheckDeviceAttributes(context.device, attr, context.targetAttributes);

  // Set or update target attributes
  const StrMap attrMap = {
    { attr.fpu,              RteConstants::RTE_DFPU        },
    { attr.dsp,              RteConstants::RTE_DDSP        },
    { attr.mve,              RteConstants::RTE_DMVE        },
    { attr.endian,           RteConstants::RTE_DENDIAN     },
    { attr.trustzone,        RteConstants::RTE_DSECURE     },
    { attr.branchProtection, RteConstants::RTE_DBRANCHPROT },
  };
  for (const auto& [yamlValue, rteKey] : attrMap) {
    if (!yamlValue.empty()) {
      const auto& rteValue = RteConstants::GetDeviceAttribute(rteKey, yamlValue);
      if (!rteValue.empty()) {
        context.targetAttributes[rteKey] = rteValue;
      }
    }
  }

  context.devicePack = matchedDevice->GetPackage();
  if (context.devicePack) {
    context.packages.insert({ context.devicePack->GetID(), context.devicePack });
  }
  GetDeviceItem(context.device, context.deviceItem);
  context.variables[RteConstants::AS_DNAME] = context.deviceItem.name;
  context.variables[RteConstants::AS_PNAME] = context.deviceItem.pname;
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

void ProjMgrWorker::CheckDeviceAttributes(const string& device, const ProcessorItem& userSelection, const StrMap& targetAttributes) {
  // check endian compatibility
  if (!userSelection.endian.empty()) {
    if (targetAttributes.find(RteConstants::RTE_DENDIAN) != targetAttributes.end()) {
      const auto& endian = targetAttributes.at(RteConstants::RTE_DENDIAN);
      if ((endian != RteConstants::RTE_ENDIAN_CONFIGURABLE) &&
        (endian != RteConstants::GetDeviceAttribute(RteConstants::RTE_DENDIAN, userSelection.endian))) {
        ProjMgrLogger::Warn("device '" + device + "' does not support '" + RteConstants::YAML_ENDIAN + ": " + userSelection.endian + "'");
      }
    }
  }
  // check dp vs sp fpu
  if ((userSelection.fpu == RteConstants::YAML_FPU_DP) &&
    (targetAttributes.find(RteConstants::RTE_DFPU) != targetAttributes.end()) &&
    (targetAttributes.at(RteConstants::RTE_DFPU) == RteConstants::RTE_SP_FPU)) {
    ProjMgrLogger::Warn("device '" + device + "' does not support '" + RteConstants::YAML_FPU + ": " + userSelection.fpu + "'");
  }
  // check disabled capabilities
  const vector<tuple<string, string, string, string>> attrMapCompatibility = {
    { RteConstants::YAML_FPU              , userSelection.fpu,              RteConstants::RTE_DFPU,    RteConstants::RTE_NO_FPU    },
    { RteConstants::YAML_DSP              , userSelection.dsp,              RteConstants::RTE_DDSP,    RteConstants::RTE_NO_DSP    },
    { RteConstants::YAML_MVE              , userSelection.mve,              RteConstants::RTE_DMVE,    RteConstants::RTE_NO_MVE    },
    { RteConstants::YAML_TRUSTZONE        , userSelection.trustzone,        RteConstants::RTE_DTZ,     RteConstants::RTE_NO_TZ     },
    { RteConstants::YAML_BRANCH_PROTECTION, userSelection.branchProtection, RteConstants::RTE_DPACBTI, RteConstants::RTE_NO_PACBTI },
  };
  for (const auto& [yamlKey, yamlValue, rteKey, rteValue] : attrMapCompatibility) {
    if (!yamlValue.empty() && (yamlValue != RteConstants::YAML_OFF)) {
      if ((targetAttributes.find(rteKey) == targetAttributes.end()) ||
        (targetAttributes.at(rteKey) == rteValue)) {
        ProjMgrLogger::Warn("device '" + device + "' does not support '" + yamlKey + ": " + yamlValue + "'");
      }
    }
  }
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

/**
 * @brief Takes a loosely defined needle pack id and tries to match it to a number of
 * resolved pack items from the `cbuild-pack.yml` file.
 * Project local packs are ignored.
 *
 * @param needle The loosely defined pack id
 * @param resolvedPacks The cbuild-pack.yml resolved items
 * @return The list of matched resolved packIds
 */
vector<string> ProjMgrWorker::FindMatchingPackIdsInCbuildPack(const PackItem& needle, const vector<ResolvedPackItem>& resolvedPacks) {
  // This should never happen, assuming that the parser validates the pack ID
  if (needle.pack.empty()) {
    return {};
  }

  // Only consider non-project-local packs
  if (!needle.path.empty()) {
    return {};
  }

  PackInfo needleInfo;
  ProjMgrUtils::ConvertToPackInfo(needle.pack, needleInfo);

  vector<string> matches;
  for (const auto& resolvedPack : resolvedPacks) {
    // First try exact matching
    if (find(resolvedPack.selectedByPack.cbegin(), resolvedPack.selectedByPack.cend(), needle.pack) != resolvedPack.selectedByPack.end()) {
      if (!needleInfo.name.empty() && !WildCards::IsWildcardPattern(needle.pack)) {
        // Exact match means only one result
        return {resolvedPack.pack};
      }

      // Needle is a wildcard, so just collect and continue
      matches.push_back(resolvedPack.pack);
    } else {
      // Next, try fuzzy matching
      PackInfo resolvedInfo;
      ProjMgrUtils::ConvertToPackInfo(resolvedPack.pack, resolvedInfo);

      if (ProjMgrUtils::IsMatchingPackInfo(resolvedInfo, needleInfo)) {
        matches.push_back(resolvedPack.pack);
      }
    }
  }

  if (matches.size() <= 1) {
    return matches;
  }

  // If wildcard, allow it to match more than one pack id
  if (needleInfo.name.empty() || WildCards::IsWildcardPattern(needle.pack)) {
    return matches;
  }

  // Order latest version first
  std::sort(matches.begin(), matches.end(), [](const string &packId1, const string &packId2) {
    PackInfo packInfo1, packInfo2;
    ProjMgrUtils::ConvertToPackInfo(packId1, packInfo1);
    ProjMgrUtils::ConvertToPackInfo(packId2, packInfo2);
    return VersionCmp::Compare(packInfo1.version, packInfo2.version) > 0;
  });

  // Non-wildcard returns the pack id with the highest version.
  // This should only happen the first time the needle is not included in the selected-by-pack-list.
  return {matches[0]};
}

bool ProjMgrWorker::ProcessPackages(ContextItem& context, const string& packRoot) {
  vector<PackItem> packRequirements;

  // Solution package requirements
  if (context.csolution) {
    InsertPackRequirements(context.csolution->packs, packRequirements, context.csolution->directory);
  }
  // Project package requirements
  if (context.cproject) {
    InsertPackRequirements(context.cproject->packs, packRequirements, context.cproject->directory);
  }
  // Layers package requirements
  for (const auto& [_, clayer] : context.clayers) {
    InsertPackRequirements(clayer->packs, packRequirements, clayer->directory);
  }
  return AddPackRequirements(context, packRequirements);
}

void ProjMgrWorker::InsertPackRequirements(const vector<PackItem>& src, vector<PackItem>& dst, string base) {
  for (auto item : src) {
    if (!item.path.empty()) {
      RteFsUtils::NormalizePath(item.path, base);
    }
    dst.push_back(item);
  }
}

/**
 * @brief Add the required packs for the project context to the list of packages to import into the RTE model.
 * The list of input pack items will be filtered for the currently select build-type/target-type/for-context context.
 * cbuild-pack information will be considered and prefered to select version of a pack.
 *
 * @param context The currently active context that will be filled with pack requirements
 * @param packRequirements List of pack items
 * @return True if sucessful
 */
bool ProjMgrWorker::AddPackRequirements(ContextItem& context, const vector<PackItem>& packRequirements) {
  const bool ignoreCBuildPack = m_loadPacksPolicy == LoadPacksPolicy::ALL || m_loadPacksPolicy == LoadPacksPolicy::LATEST;
  const vector<ResolvedPackItem>& resolvedPacks = context.csolution && !ignoreCBuildPack ? context.csolution->cbuildPack.packs : vector<ResolvedPackItem>();
 // Filter context specific package requirements
  vector<PackItem> packages;
  for (const auto& packItem : packRequirements) {
    if (CheckContextFilters(packItem.type, context)) {
      packages.push_back(packItem);
    }
  }

  // Process packages
  for (const auto& packageEntry : packages) {
    if (packageEntry.path.empty()) {
      // System wide package
      vector<string> matchedPackIds = FindMatchingPackIdsInCbuildPack(packageEntry, resolvedPacks);
      if (matchedPackIds.size()) {
        // Cbuild pack content matches, so use it
        for (const auto& resolvedPackId : matchedPackIds) {
          PackageItem package;
          ProjMgrUtils::ConvertToPackInfo(resolvedPackId, package.pack);
          context.userInputToResolvedPackIdMap[packageEntry.pack].insert(resolvedPackId);
          context.packRequirements.push_back(package);
        }
      } else {
        // Not matching cbuild pack, add it unless a wildcard entry
        PackageItem package;
        ProjMgrUtils::ConvertToPackInfo(packageEntry.pack, package.pack);

        // Resolve version range using installed packs
        if (!package.pack.name.empty() && !WildCards::IsWildcardPattern(package.pack.name)) {
          string reqVersionRange = ProjMgrUtils::ConvertToVersionRange(package.pack.version);
          string path = m_packRoot + '/' + package.pack.vendor + '/' + package.pack.name;
          string installedVersion = RteFsUtils::GetInstalledPackVersion(path, reqVersionRange);

          // Only remember the version of the pack if we had it installed
          // Will be used when serializing the cbuild-pack.yml file later
          if (!installedVersion.empty()) {
            const string newPackId = RtePackage::ComposePackageID(package.pack.vendor, package.pack.name, installedVersion);
            context.userInputToResolvedPackIdMap[packageEntry.pack].insert(newPackId);
            package.pack.version = installedVersion;
          } else {
            // Remember that we had the user input, but it does not match any installed pack
            context.userInputToResolvedPackIdMap[packageEntry.pack] = {};
          }
          context.packRequirements.push_back(package);
        }
      }
    } else {
      // Project local pack - add as-is
      PackageItem package;
      package.path = packageEntry.path;
      RteFsUtils::NormalizePath(package.path, context.csolution->directory + "/");
      if (!RteFsUtils::Exists(package.path)) {
        ProjMgrLogger::Error("pack path: " + packageEntry.path + " does not exist");
        return false;
      }
      ProjMgrUtils::ConvertToPackInfo(packageEntry.pack, package.pack);
      string pdscFile = package.pack.vendor + '.' + package.pack.name + ".pdsc";
      RteFsUtils::NormalizePath(pdscFile, package.path + "/");
      if (!RteFsUtils::Exists(pdscFile)) {
        ProjMgrLogger::Error("pdsc file was not found in: " + packageEntry.path);
        return false;
      }
      context.packRequirements.push_back(package);
      context.localPackPaths.insert(package.path);
    }
  }

  // Add wildcard entries last so that they can be re-expanded if needed
  for (const auto& packageEntry : packages) {
    PackageItem package;
    package.path = packageEntry.path;
    ProjMgrUtils::ConvertToPackInfo(packageEntry.pack, package.pack);

    if (package.pack.name.empty() || WildCards::IsWildcardPattern(package.pack.name)) {
      context.packRequirements.push_back(package);
    }
  }

  // In case there is no packs-list in the project files, reduce the scope to the locked pack list
  if (context.packRequirements.empty()) {
    for (const auto& resolvedPack : resolvedPacks) {
      PackageItem package;
      ProjMgrUtils::ConvertToPackInfo(resolvedPack.pack, package.pack);
      context.packRequirements.push_back(package);
    }
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
  if (context.toolchain.name == "AC6") {
    context.targetAttributes["Tcompiler"] = "ARMCC";
    context.targetAttributes["Toptions"] = context.toolchain.name;
  } else {
    context.targetAttributes["Tcompiler"] = context.toolchain.name;
  }
  return true;
}

bool ProjMgrWorker::ProcessComponents(ContextItem& context) {
  bool error = false;

  if (!context.rteActiveTarget) {
    ProjMgrLogger::Error("missing RTE target");
    return false;
  }

  // Get installed components map
  const RteComponentMap& installedComponents = context.rteActiveTarget->GetFilteredComponents();
  RteComponentMap componentMap;
  for (auto& component : installedComponents) {
    const string& componentId = component.second->GetComponentID(true);
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
      error = true;
      continue;
    }

    UpdateMisc(item.build.misc, context.toolchain.name);

    const auto& componentId = matchedComponent->GetComponentID(true);
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
    string generatorId = matchedComponent->GetGeneratorName();
    RteGenerator* generator = matchedComponent->GetGenerator();
    if (generator && !generator->IsExternal()) {
      context.generators.insert({ generatorId, generator });
      string genDir;
      if (!GetGeneratorDir(generator, context, layer, genDir)) {
        return false;
      }
      matchedComponentInstance->AddAttribute("gendir", genDir);
      const string gpdsc = RteFsUtils::MakePathCanonical(generator->GetExpandedGpdsc(context.rteActiveTarget, genDir));
      context.gpdscs.insert({ gpdsc, {componentId, generatorId, genDir} });
    } else {
      // Get external generator id
      if (!generatorId.empty()) {
        // check if required global generator is registered
        if (!m_extGenerator->CheckGeneratorId(generatorId, componentId)) {
          return false;
        }
        string genDir;
        if (!GetExtGeneratorDir(generatorId, context, layer, genDir)) {
          return false;
        }
        // keep track of used generators
        m_extGenerator->AddUsedGenerator(generatorId, genDir, context.name);
        context.extGenDir[generatorId] = genDir;
      }
    }

    // Component instances
    if (item.instances > matchedComponentInstance->GetMaxInstances()) {
      ProjMgrLogger::Error("component '" + item.component + "' does not accept more than " + to_string(matchedComponentInstance->GetMaxInstances()) + " instance(s)");
      error = true;
    } else if (item.instances > 1) {
      matchedComponentInstance->AddAttribute("instances", to_string(item.instances));
    }

    // Insert matched component into context list
    context.components.insert({ componentId, { matchedComponentInstance, &item, generatorId } });
    const auto& componentPackage = matchedComponent->GetPackage();
    if (componentPackage) {
      context.packages.insert({ componentPackage->GetID(), componentPackage });
    }
    if (matchedComponent->HasApi(context.rteActiveTarget)) {
      const auto& api = matchedComponent->GetApi(context.rteActiveTarget, false);
      if (api) {
        const auto& apiPackage = api->GetPackage();
        if (apiPackage) {
          context.packages.insert({ apiPackage->GetID(), apiPackage });
        }
      }
    }
  }

  // Add required components into RTE
  if (!AddRequiredComponents(context)) {
    return false;
  }

  if (!CheckRteErrors()) {
    return false;
  }

  return !error;
}

RteComponent* ProjMgrWorker::ProcessComponent(ContextItem& context, ComponentItem& item, RteComponentMap& componentMap)
{
  if (!item.condition.empty()) {
    RteComponentInstance ci(nullptr);
    ci.SetTag("component");

    ci.SetAttributesFomComponentId(item.component);
    ci.AddAttribute("condition", item.condition);
    auto ti = ci.EnsureTargetInfo(context.rteActiveTarget->GetName());
    ti->SetVersionMatchMode(VersionCmp::MatchMode::ENFORCED_VERSION);
    RtePackageInstanceInfo packInfo(nullptr, item.fromPack);
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
  if (componentDescriptor.find_first_of(RteConstants::COMPONENT_DELIMITERS) != string::npos) {
    // Consider a full or partial component identifier was given
    filterSet.insert(RteUtils::GetPrefix(componentDescriptor, RteConstants::PREFIX_CVERSION_CHAR));
  } else {
    // Consider free text was given
    filterSet = RteUtils::SplitStringToSet(componentDescriptor);
  }

  vector<string> componentIdVec;
  componentIdVec.reserve(componentMap.size());
  for (auto [componentId, _] :componentMap) {
    componentIdVec.push_back(componentId);
  }
  RteUtils::ApplyFilter(componentIdVec, filterSet, filteredIds);
  for (const auto& filteredId : filteredIds) {
    auto c = componentMap[filteredId];
    filteredComponents[filteredId] = c;
  }

  // Multiple matches, search best matched identifier
  if (filteredComponents.size() > 1) {
    RteComponentMap fullMatchedComponents;
    for (const auto& component : filteredComponents) {
      if (RteUtils::SplitStringToSet(component.first, RteConstants::COMPONENT_DELIMITERS) ==
          RteUtils::SplitStringToSet(componentDescriptor, RteConstants::COMPONENT_DELIMITERS)) {
        fullMatchedComponents.insert(component);
      }
    }
    if (fullMatchedComponents.size() > 0) {
      filteredComponents = fullMatchedComponents;
    }
  }

  // Multiple matches, check exact partial identifier
  string requiredComponentId = RteUtils::RemovePrefixByString(item.component, RteConstants::SUFFIX_CVENDOR);
  if (filteredComponents.size() > 1) {
    RteComponentMap matchedComponents;
    for (const auto& [id, component] : filteredComponents) {
      // Get component id without vendor and version
      const string& componentId = component->GetPartialComponentID(true);
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
      availableComponentVersions.insert(RteUtils::GetSuffix(component.first, RteConstants::PREFIX_CVERSION_CHAR));
    });
  const string filterVersion = RteUtils::GetSuffix(item.component, RteConstants::PREFIX_CVERSION_CHAR, true);
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
  Collection<RteItem*> selItems;
  for (auto& [_, component] : context.components) {
    selItems.push_back(component.instance);
  }
  set<RteComponentInstance*> unresolvedComponents;
  context.rteActiveProject->AddCprjComponents(selItems, context.rteActiveTarget, unresolvedComponents);
  if (!unresolvedComponents.empty()) {
    string msg = "unresolved components:";
    for (const auto& component : unresolvedComponents) {
      msg += "\n" + component->GetComponentID(true);
    }
    ProjMgrLogger::Error(msg);
    return false;
  }
  // Check regions header, generate it if needed
  if (!context.linker.regions.empty()) {
    CheckAndGenerateRegionsHeader(context);
  }

  return true;
}

void ProjMgrWorker::CheckAndGenerateRegionsHeader(ContextItem& context) {
  const string regionsHeader = RteFsUtils::MakePathCanonical(fs::path(context.directories.cprj).append(context.linker.regions).generic_string());
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

string ProjMgrWorker::GetContextRteFolder(ContextItem& context) {
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
  return RteFsUtils::MakePathCanonical(rteFolder);
}

bool ProjMgrWorker::GenerateRegionsHeader(ContextItem& context, string& generatedRegionsFile) {
  // generate regions header
  const auto& rteFolder = GetContextRteFolder(context);
  if (!context.rteActiveTarget->GenerateRegionsHeader(rteFolder + "/")) {
    ProjMgrLogger::Warn("regions header file generation failed");
    return false;
  }
  generatedRegionsFile = RteFsUtils::MakePathCanonical(fs::path(rteFolder).append(context.rteActiveTarget->GetRegionsHeader()).generic_string());
  return true;
}

bool ProjMgrWorker::ProcessConfigFiles(ContextItem& context) {
  if (!context.rteActiveTarget) {
    ProjMgrLogger::Error("missing RTE target");
    return false;
  }
  map<string, RteFileInstance*> configFiles = context.rteActiveProject->GetFileInstances();
  if (!configFiles.empty()) {
    for (auto fi : configFiles) {
      const string& componentID = fi.second->GetComponentID(true);
      context.configFiles[componentID].insert(fi);
    }
  }
  // Linker script
  if (context.linker.autoGen) {
    if (!context.linker.script.empty()) {
      ProjMgrLogger::Warn("conflict: automatic linker script generation overrules specified script '" + context.linker.script + "'");
      context.linker.script.clear();
    }
  } else if (context.linker.script.empty() && context.linker.regions.empty() && context.linker.defines.empty()) {
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
      string stem = entry.path().stem().stem().generic_string();
      if (RteUtils::EqualNoCase(context.toolchain.name + "_linker_script", stem)) {
        linkerScript = entry.path().generic_string();
        break;
      }
    }
    if (linkerScript.empty()) {
      ProjMgrLogger::Warn("linker script template for compiler '" + context.toolchain.name + "' was not found");
      return;
    }
    const string& rteFolder = GetContextRteFolder(context);
    const string& deviceFolder = context.rteActiveTarget->GetDeviceFolder();
    string linkerScriptDestination = fs::path(linkerScript).filename().generic_string();
    RteFsUtils::NormalizePath(linkerScriptDestination, fs::path(rteFolder).append(deviceFolder).generic_string());

    // Copy default linker script into device folder if it does not exist
    if (!RteFsUtils::Exists(linkerScriptDestination)) {
      RteFsUtils::CopyCheckFile(linkerScript, linkerScriptDestination, false);
    }
    context.linker.script = fs::relative(linkerScriptDestination, context.directories.cprj).generic_string();

    // Generate regions file if it is not set
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
          const auto& language = componentFile->GetAttribute("language");
          const auto& scope = componentFile->GetAttribute("scope");
          const auto& version = attr == "config" ? componentFile->GetVersionString() : "";
          context.componentFiles[component->GetComponentID(true)].push_back({ file, attr, category, language, scope, version });
        }
      }
    }
  }
  // iterate over components
  for (const auto& [componentId, component] : context.components) {
    RteComponent* rteComponent = component.instance->GetParent()->GetComponent();
    const auto& files = rteComponent->GetFileContainer() ? rteComponent->GetFileContainer()->GetChildren() : Collection<RteItem*>();
    // private includes
    for (const auto& privateIncludes : {
      context.rteActiveTarget->GetPrivateIncludePaths(rteComponent, RteFile::Language::LANGUAGE_C),
      context.rteActiveTarget->GetPrivateIncludePaths(rteComponent, RteFile::Language::LANGUAGE_CPP),
      context.rteActiveTarget->GetPrivateIncludePaths(rteComponent, RteFile::Language::LANGUAGE_C_CPP),
      context.rteActiveTarget->GetPrivateIncludePaths(rteComponent, RteFile::Language::LANGUAGE_NONE)
      }) {
      for (const auto& privateInclude : privateIncludes) {
        const string& include = fs::path(privateInclude).is_relative() ? fs::path(context.cproject->directory).append(privateInclude).generic_string() : privateInclude;
        component.item->build.addpaths.push_back(RteFsUtils::RelativePath(include, context.directories.cprj));
      }
    }
    // hidden files and pre-include files from packs
    for (const auto& componentFile : files) {
      const auto& name = rteComponent->GetPackage()->GetAbsolutePackagePath() + componentFile->GetAttribute("name");
      const auto& category = componentFile->GetAttribute("category");
      const auto& attr = componentFile->GetAttribute("attr");
      const auto& scope = componentFile->GetAttribute("scope");
      const auto& language = componentFile->GetAttribute("language");
      const auto& version = componentFile->GetVersionString();
      if ((scope == "hidden") ||
        ((((category == "preIncludeGlobal") || (category == "preIncludeLocal")) && attr.empty()) && (IsPreIncludeByTarget(context.rteActiveTarget, name)))){
        context.componentFiles[componentId].push_back({ name, attr, category, language, scope, version });
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
          const auto& language = configFile->GetAttribute("language");
          const auto& scope = configFile->GetAttribute("scope");
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
          context.componentFiles[componentId].push_back({ filename, "config", category, language, scope, version });
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
        const auto& language = rteFile->GetAttribute("language");
        const auto& scope = rteFile->GetAttribute("scope");
        const auto& filename = (attr == "config" && configFilePaths.find(rteFile) != configFilePaths.end()) ?
                                configFilePaths[rteFile] : rteFile->GetOriginalAbsolutePath();
        context.generatorInputFiles[componentId].push_back({ filename, attr, category, language, scope, version });
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
          const string& componentID = component->GetComponentID(true);
          context.componentFiles[componentID].push_back({ filename, "", "preIncludeLocal", "", "", ""});
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
    const auto& componentID = component->GetComponentID(true);
    const auto& depResults = result.GetResults();
    const auto& aggregates = result.GetComponentAggregates();

    set<string> aggregatesSet;
    for (const auto& aggregate : aggregates) {
      aggregatesSet.insert(aggregate->GetComponentAggregateID());
    }
    set<string> expressionsSet;
    for (const auto& [item, _] : depResults) {
      expressionsSet.insert(item->GetDependencyExpressionID());
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
    const string gpdscFile = RteFsUtils::MakePathCanonical(file);
    if(!contains_key(context.gpdscs, gpdscFile)) {
      // skip external cgen.yml files, they are processed separately
      continue;
    }
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
    const auto& gpdscComponents = gpdscPack->GetComponents();
    if (gpdscComponents) {
      for (const auto& gpdscComponent : gpdscComponents->GetChildren()) {
        Collection<RteItem*> components;
        if (gpdscComponent->GetTag() == "component") {
          components.push_back(gpdscComponent);
        } else if (gpdscComponent->GetTag() == "bundle") {
          components = gpdscComponent->GetChildren();
        }
        for (const auto component : components) {
          const auto& componentId = component->GetComponentID(true);
          const auto& item = context.components[context.gpdscs.at(gpdscFile).component].item;
          RteComponentInstance* componentInstance = new RteComponentInstance(component);
          componentInstance->InitInstance(component);
          context.components.insert({ componentId, { componentInstance, item, component->GetGeneratorName()} });
        }
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
          *item.assign = *element;
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


bool ProjMgrWorker::ProcessPrecedences(ContextItem& context, bool rerun) {
  // Notes: defines, includes and misc are additive. All other keywords overwrite previous settings.
  // The target-type and build-type definitions are additive, but an attempt to
  // redefine an already existing type results in an error.
  // The settings of the target-type are processed first; then the settings of the
  // build-type that potentially overwrite the target-type settings.

  if (!rerun && context.precedences) {
    return true;
  }
  context.precedences = true;
  context.components.clear();
  context.componentRequirements.clear();
  context.groups.clear();

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
  context.variables[RteConstants::AS_DNAME] = deviceItem.name;
  context.variables[RteConstants::AS_PNAME] = deviceItem.pname;
  context.variables[RteConstants::AS_BNAME] = context.board;
  context.variables[RteConstants::AS_COMPILER] = context.toolchain.name;

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
  if (!ProcessSequencesRelatives(context, rerun)) {
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
  CollectionUtils::AddStringItemsUniquely(projectDefines, context.controls.cproject.defines);
  for (auto& [_, clayer] : context.controls.clayers) {
    CollectionUtils::AddStringItemsUniquely(projectDefines, clayer.defines);
  }
  for (auto& setup : context.controls.setups) {
    CollectionUtils::AddStringItemsUniquely(projectDefines, setup.defines);
  }
  CollectionUtils::AddStringItemsUniquely(projectUndefines, context.controls.cproject.undefines);
  for (auto& [_, clayer] : context.controls.clayers) {
    CollectionUtils::AddStringItemsUniquely(projectUndefines, clayer.undefines);
  }
  for (auto& setup : context.controls.setups) {
    CollectionUtils::AddStringItemsUniquely(projectUndefines, setup.undefines);
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
  CollectionUtils::MergeDefines(defines);

  // Includes
  vector<string> projectAddPaths, projectDelPaths;
  CollectionUtils::AddStringItemsUniquely(projectAddPaths, context.controls.cproject.addpaths);
  for (auto& [_, clayer] : context.controls.clayers) {
    CollectionUtils::AddStringItemsUniquely(projectAddPaths, clayer.addpaths);
  }
  for (auto& setup : context.controls.setups) {
    CollectionUtils::AddStringItemsUniquely(projectAddPaths, setup.addpaths);
  }
  CollectionUtils::AddStringItemsUniquely(projectDelPaths, context.controls.cproject.delpaths);
  for (auto& [_, clayer] : context.controls.clayers) {
    CollectionUtils::AddStringItemsUniquely(projectDelPaths, clayer.delpaths);
  }
  for (auto& setup : context.controls.setups) {
    CollectionUtils::AddStringItemsUniquely(projectDelPaths, setup.delpaths);
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
  CollectionUtils::MergeStringVector(includes);

  return true;
}

bool ProjMgrWorker::ProcessProcessorOptions(ContextItem& context) {
  // process precedences of 'trustzone' options
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

  // process precedences of 'fpu' options
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

  // process precedences of 'dsp' options
  StringCollection dsp = {
    &context.controls.processed.processor.dsp,
    {
      &context.controls.cproject.processor.dsp,
      &context.controls.csolution.processor.dsp,
      &context.controls.target.processor.dsp,
      &context.controls.build.processor.dsp,
    },
  };
  for (auto& setup : context.controls.setups) {
    dsp.elements.push_back(&setup.processor.dsp);
  }
  for (auto& [_, clayer] : context.controls.clayers) {
    dsp.elements.push_back(&clayer.processor.dsp);
  }
  if (!ProcessPrecedence(dsp)) {
    return false;
  }

  // process precedences of 'mve' options
  StringCollection mve = {
    &context.controls.processed.processor.mve,
    {
      &context.controls.cproject.processor.mve,
      &context.controls.csolution.processor.mve,
      &context.controls.target.processor.mve,
      &context.controls.build.processor.mve,
    },
  };
  for (auto& setup : context.controls.setups) {
    mve.elements.push_back(&setup.processor.mve);
  }
  for (auto& [_, clayer] : context.controls.clayers) {
    mve.elements.push_back(&clayer.processor.mve);
  }
  if (!ProcessPrecedence(mve)) {
    return false;
  }

  // process precedences of 'endian' options
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

  // process precedences of 'branchProtection' options
  StringCollection branchProtection = {
  &context.controls.processed.processor.branchProtection,
  {
    &context.controls.cproject.processor.branchProtection,
    &context.controls.csolution.processor.branchProtection,
    &context.controls.target.processor.branchProtection,
    &context.controls.build.processor.branchProtection,
  },
  };
  for (auto& setup : context.controls.setups) {
    branchProtection.elements.push_back(&setup.processor.branchProtection);
  }
  for (auto& [_, clayer] : context.controls.clayers) {
    branchProtection.elements.push_back(&clayer.processor.branchProtection);
  }
  if (!ProcessPrecedence(branchProtection)) {
    return false;
  }
  return true;
}

bool ProjMgrWorker::ProcessLinkerOptions(ContextItem& context) {
  // clear processing lists
  context.linker.scriptList.clear();
  context.linker.regionsList.clear();
  context.linker.autoGen = false;

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
    CollectionUtils::AddStringItemsUniquely(context.linker.defines, linker.defines);
    if (linker.autoGen) {
      context.linker.autoGen = true;
    }
  }
  return true;
}

bool ProjMgrWorker::ProcessSequencesRelatives(ContextItem & context, bool rerun) {
  if (!rerun) {
    // directories
    const string ref = m_outputDir.empty() ? context.csolution->directory : RteFsUtils::AbsolutePath(m_outputDir).generic_string();
    if (!ProcessSequenceRelative(context, context.directories.cprj) ||
      !ProcessSequenceRelative(context, context.directories.rte, context.cproject->directory) ||
      !ProcessSequenceRelative(context, context.directories.outdir, ref) ||
      !ProcessSequenceRelative(context, context.directories.intdir, ref)) {
      return false;
    }
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
  const string refContextOutDir = refContext.directories.cprj + "/" + refContext.directories.outdir;
  const string relOutDir = RteFsUtils::RelativePath(refContextOutDir, context.directories.cprj, withHeadingDot);
  string regExStr = "\\$";
  string replacement;
  if (sequence == RteConstants::AS_SOLUTION_DIR) {
    regExStr += RteConstants::AS_SOLUTION_DIR;
    replacement = RteFsUtils::RelativePath(refContext.csolution->directory, context.directories.cprj, withHeadingDot);
  } else if (sequence == RteConstants::AS_PROJECT_DIR) {
    regExStr += RteConstants::AS_PROJECT_DIR;
    replacement = RteFsUtils::RelativePath(refContext.cproject->directory, context.directories.cprj, withHeadingDot);
  } else if (sequence == RteConstants::AS_OUT_DIR) {
    regExStr += RteConstants::AS_OUT_DIR;
    replacement = relOutDir;
  } else if (sequence == RteConstants::AS_ELF) {
    regExStr += RteConstants::AS_ELF;
    replacement = refContext.outputTypes.elf.on ? relOutDir + "/" + refContext.outputTypes.elf.filename : "";
  } else if (sequence == RteConstants::AS_BIN) {
    regExStr += RteConstants::AS_BIN;
    replacement = refContext.outputTypes.bin.on ? relOutDir + "/" + refContext.outputTypes.bin.filename : "";
  } else if (sequence == RteConstants::AS_HEX) {
    regExStr += RteConstants::AS_HEX;
    replacement = refContext.outputTypes.hex.on ? relOutDir + "/" + refContext.outputTypes.hex.filename : "";
  } else if (sequence == RteConstants::AS_LIB) {
    regExStr += RteConstants::AS_LIB;
    replacement = refContext.outputTypes.lib.on ? relOutDir + "/" + refContext.outputTypes.lib.filename : "";
  } else if (sequence == RteConstants::AS_CMSE) {
    regExStr += RteConstants::AS_CMSE;
    replacement = refContext.outputTypes.cmse.on ? relOutDir + "/" + refContext.outputTypes.cmse.filename : "";
  }
  regex regEx = regex(regExStr + "\\(.*\\)\\$");
  item = regex_replace(item, regEx, replacement);
}

bool ProjMgrWorker::ProcessSequenceRelative(ContextItem& context, string& item, const string& ref, bool withHeadingDot) {
  size_t offset = 0;
  bool pathReplace = false;
  // expand variables (static access sequences)
  const string input = item = RteUtils::ExpandAccessSequences(item, context.variables);
  // expand dynamic access sequences
  while (offset != string::npos) {
    string sequence;
    // get next access sequence
    if (!RteUtils::GetAccessSequence(offset, input, sequence, '$', '$')) {
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
            if (!ParseContextLayers(refContext)) {
              return false;
            }
            if (!ProcessPrecedences(refContext)) {
              return false;
            }
          }
          // expand access sequence
          ExpandAccessSequence(context, refContext, sequenceName, item, withHeadingDot);
          // store dependency information
          if (refContext.name != context.name) {
            CollectionUtils::PushBackUniquely(context.dependsOn, refContext.name);
          }
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
     error_code ec;
    // adjust relative path according to the given reference
    if (!fs::equivalent(context.directories.cprj, ref, ec)) {
      const string absPath = RteFsUtils::MakePathCanonical(fs::path(item).is_relative() ? ref + "/" + item : item);
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
      srcNode.category = RteFsUtils::FileCategoryFromExtension(srcNode.file);
    }

    dst.push_back(srcNode);

    // Set linker script
    if ((srcNode.category == "linkerScript") && (!context.linker.autoGen &&
      context.linker.script.empty() && context.linker.regions.empty() && context.linker.defines.empty())) {
      context.linker.script = srcNode.file;
    }

    // Store absolute file path
    const string filePath = RteFsUtils::MakePathCanonical(root + "/" + srcNode.file);
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
    CheckCompilerFilterSpelling(compiler);
    if (ProjMgrUtils::AreCompilersCompatible(compiler, selectedCompiler)) {
      return true;
    }
  }
  return false;
}

void ProjMgrWorker::CheckCompilerFilterSpelling(const string& compiler) {
  const string compilerName = RteUtils::GetPrefix(compiler, '@');
  for (const auto& m_toolchainConfigFile : m_toolchainConfigFiles) {
    if (fs::path(m_toolchainConfigFile).stem().generic_string().find(compilerName) == 0) {
      return;
    }
  }
  CollectionUtils::PushBackUniquely(m_missingToolchains, compilerName);
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

  CheckTypeFilterSpelling(typeFilter);

  return CheckType(typeFilter, typeVec);
}

void ProjMgrWorker::RetrieveAllContextTypes(void) {
  const auto& csolution = m_parser->GetCsolution();
  for (const auto& [buildType, item] : csolution.buildTypes) {
    CollectionUtils::PushBackUniquely(m_types.allBuildTypes, buildType);
    for (const auto& mappedContext : item.contextMap) {
      if (!mappedContext.build.empty()) {
        CollectionUtils::PushBackUniquely(m_types.allBuildTypes, mappedContext.build);
      }
      if (!mappedContext.target.empty()) {
        CollectionUtils::PushBackUniquely(m_types.allTargetTypes, mappedContext.target);
      }
    }
  }
  for (const auto& [targetType, item] : csolution.targetTypes) {
    CollectionUtils::PushBackUniquely(m_types.allTargetTypes, targetType);
    for (const auto& mappedContext : item.build.contextMap) {
      if (!mappedContext.build.empty()) {
        CollectionUtils::PushBackUniquely(m_types.allBuildTypes, mappedContext.build);
      }
      if (!mappedContext.target.empty()) {
        CollectionUtils::PushBackUniquely(m_types.allTargetTypes, mappedContext.target);
      }
    }
  }
}

void ProjMgrWorker::CheckTypeFilterSpelling(const TypeFilter& typeFilter) {
  for (const auto& typePairs : { typeFilter.include, typeFilter.exclude }) {
    for (const auto& typePair : typePairs) {
      if (!typePair.build.empty() &&
        find(m_types.allBuildTypes.begin(), m_types.allBuildTypes.end(), typePair.build) == m_types.allBuildTypes.end()) {
        bool misspelled = find(m_types.allTargetTypes.begin(), m_types.allTargetTypes.end(), typePair.build) != m_types.allTargetTypes.end();
        m_types.missingBuildTypes[typePair.build] = misspelled;
      }
      if (!typePair.target.empty() &&
        find(m_types.allTargetTypes.begin(), m_types.allTargetTypes.end(), typePair.target) == m_types.allTargetTypes.end()) {
        bool misspelled = find(m_types.allBuildTypes.begin(), m_types.allBuildTypes.end(), typePair.target) != m_types.allBuildTypes.end();
        m_types.missingTargetTypes[typePair.target] = misspelled;
      }
    }
  }
}

void ProjMgrWorker::PrintMissingFilters(void) {
  for (const auto& [type, misspelled] : m_types.missingBuildTypes) {
    ProjMgrLogger::Warn("build-type '." + type + "' does not exist in solution" +
      (misspelled ? ", did you mean '+" + type + "'?" : ""));
  }
  for (const auto& [type, misspelled] : m_types.missingTargetTypes) {
    ProjMgrLogger::Warn("target-type '+" + type + "' does not exist in solution" +
      (misspelled ? ", did you mean '." + type + "'?" : ""));
  }
  for (const auto& toolchain : m_missingToolchains) {
    ProjMgrLogger::Warn("compiler '" + toolchain + "' is not supported");
  }
}

bool ProjMgrWorker::ProcessContext(ContextItem& context, bool loadGenFiles, bool resolveDependencies, bool updateRteFiles) {
  bool ret = true;
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
  ret &= ProcessLinkerOptions(context);
  ret &= ProcessGroups(context);
  ret &= ProcessComponents(context);
  if (loadGenFiles) {
    ret &= ProcessGpdsc(context);
    ret &= ProcessGeneratedLayers(context);
  }
  ret &= ProcessConfigFiles(context);
  ret &= ProcessComponentFiles(context);
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
  return ret;
}

bool ProjMgrWorker::ListPacks(vector<string>&packs, bool bListMissingPacksOnly, const string& filter) {
  map<string, string, RtePackageComparator> packsMap;
  list<string> pdscFiles;
  if (!InitializeModel()) {
    return false;
  }
  bool reqOk = true;
  m_contextErrMap.clear();
  for (const auto& selectedContext : m_selectedContexts) {
    ContextItem& context = m_contexts[selectedContext];
    if(!CollectRequiredPdscFiles(context, m_packRoot)) {
      // in case of explicit query for missing pack list,
      // the method should return true unless other errors occur
      // That should keep the calling code happy:
      // https://github.com/Open-CMSIS-Pack/cbuild/blob/main/pkg/builder/csolution/builder.go#L90
      reqOk = bListMissingPacksOnly ? // explicit query for missing packs (-m)
        !context.missingPacks.empty() // missing packs found (expected error)
        : false;  // some other kind of error, return false
    }
    // Get missing packs identifiers
    for (const auto& pack : context.missingPacks) {
      string packID = RtePackage::ComposePackageID(pack.vendor, pack.name, pack.version);
      packsMap[packID] = RteUtils::EMPTY_STRING;
    }
    if(!bListMissingPacksOnly && !context.packRequirements.empty()) {
      // Get context required packs
      // first the packs with explicit paths
      for(const auto& [pdscFile, pathVer] : context.pdscFiles) {
        const string& path = pathVer.first;
        if(!path.empty()) {
          CollectionUtils::PushBackUniquely(pdscFiles, pdscFile);
        }
      }
      // then all others
      for(const auto& [pdscFile, pathVer] : context.pdscFiles) {
        const string& path = pathVer.first;
        if(path.empty()) {
          CollectionUtils::PushBackUniquely(pdscFiles, pdscFile);
        }
      }
    }
  }
  if (!bListMissingPacksOnly) {
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
        packsMap[pack->GetID()] = pack->GetPackageFileName();
      }
    }

    if (!m_contextErrMap.empty()) {
      for (const auto& selectedContext : m_selectedContexts) {
        PrintContextErrors(selectedContext);
      }
      reqOk = false;
    }
  }

  vector<string> packsVec;
  packsVec.reserve(packsMap.size());
  for (auto [id, fileName] : packsMap) {
    string s = id;
    if (!fileName.empty()) {
      string str = fileName;
      if(m_relativePaths) {
        if(str.find(m_packRoot) == 0) {
           str.replace(0, m_packRoot.size(), "${CMSIS_PACK_ROOT}");
        } else {
          str = RteFsUtils::RelativePath(str, m_rootDir, true);
        }
      }
      s += " (" + str + ")";
    }
    packsVec.push_back(s);
  }
  if (!filter.empty()) {
    vector<string> filteredPacks;
    RteUtils::ApplyFilter(packsVec, RteUtils::SplitStringToSet(filter), filteredPacks);
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
      const string& boardPack = board->GetPackageID(true);
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
    RteUtils::ApplyFilter(boardsVec, RteUtils::SplitStringToSet(filter), matchedBoards);
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
      const string& devicePack = deviceItem->GetPackageID();
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
    RteUtils::ApplyFilter(devicesVec, RteUtils::SplitStringToSet(filter), matchedDevices);
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
      const string& componentId = component.second->GetComponentID(true);
      componentIds.insert(componentId);
      componentMap[componentId] = component.second;
    }
  }
  vector<string> componentIdsVec(componentIds.begin(), componentIds.end());
  if (!filter.empty()) {
    vector<string> filteredIds;
    RteUtils::ApplyFilter(componentIdsVec, RteUtils::SplitStringToSet(filter), filteredIds);
    if (filteredIds.empty()) {
      ProjMgrLogger::Error("no component was found with filter '" + filter + "'");
      return false;
    }
    componentIdsVec = filteredIds;
  }
  for (const auto& componentId : componentIdsVec) {
    components.push_back(componentId + " (" + componentMap[componentId]->GetPackageID() + ")");
  }
  return true;
}

bool ProjMgrWorker::ListConfigs(vector<string>& configFiles, const string& filter) {
  set<string>configSet;
  for (const auto& selectedContext : m_selectedContexts) {
    ContextItem& context = m_contexts[selectedContext];
    if (!ProcessContext(context, true, false, false)) {
      return false;
    }
    const string& targetName = context.rteActiveTarget->GetName();
    for (auto [_, fi] : context.rteActiveProject->GetFileInstances()) {
      configSet.insert(fi->GetInfoString(targetName, context.csolution->path));
    }
  }
  vector<string> configVec(configSet.begin(), configSet.end());
  if (!filter.empty()) {
    vector<string> filteredConfigs;
    RteUtils::ApplyFilter(configVec, RteUtils::SplitStringToSet(filter), filteredConfigs);
    if (filteredConfigs.empty()) {
      ProjMgrLogger::Error("no unresolved dependency was found with filter '" + filter + "'");
      return false;
    }
    configVec = filteredConfigs;
  }
  configFiles.assign(configVec.begin(), configVec.end());
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
    RteUtils::ApplyFilter(dependenciesVec, RteUtils::SplitStringToSet(filter), filteredDependencies);
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
    RteUtils::ApplyFilter(contextsVec, RteUtils::SplitStringToSet(filter), filteredContexts);
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
  GeneratorContextVecMap generatorsMap;
  StrMap generatorsDescription;
  for (const auto& selectedContext : m_selectedContexts) {
    ContextItem& context = m_contexts[selectedContext];
    if (!ProcessContext(context, false, true, false)) {
      return false;
    }
    for (const auto& [id, generator] : context.generators) {
      for (const auto& [_, item] : context.gpdscs) {
        if (item.generator == id) {
          const string workingDir = fs::path(context.cproject->directory).append(item.workingDir).generic_string();
          generatorsMap[id][workingDir].push_back(context.name);
          generatorsDescription[id] = generator->GetDescription();
          break;
        }
      }
    }
  }
  GeneratorContextVecMap extGeneratorsMap(m_extGenerator->GetUsedGenerators());
  generatorsMap.insert(extGeneratorsMap.begin(), extGeneratorsMap.end());
  for (const auto& [id, dirs] : generatorsMap) {
    string generatorEntry = id + " (" + (m_extGenerator->IsGlobalGenerator(id) ?
      m_extGenerator->GetGlobalDescription(id) : generatorsDescription[id]) + ")";
    if (m_verbose) {
      for (const auto& [dir, contexts] : dirs) {
        generatorEntry += "\n  base-dir: " + RteFsUtils::RelativePath(dir, m_parser->GetCsolution().directory);
        for (const auto& context : contexts) {
          generatorEntry += "\n    context: " + context;
        }
      }
    }
    generatorsSet.insert(generatorEntry);
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
      // process precedences
      if (!ProcessPrecedences(context)) {
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
    CollectionUtils::PushBackUniquely(layers, layerEntry);
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
      CollectionUtils::AddStringItemsUniquely(dst.as, src.as);
      CollectionUtils::AddStringItemsUniquely(dst.c, src.c);
      CollectionUtils::AddStringItemsUniquely(dst.cpp, src.cpp);
      CollectionUtils::AddStringItemsUniquely(dst.c_cpp, src.c_cpp);
      CollectionUtils::AddStringItemsUniquely(dst.link, src.link);
      CollectionUtils::AddStringItemsUniquely(dst.link_c, src.link_c);
      CollectionUtils::AddStringItemsUniquely(dst.link_cpp, src.link_cpp);
      CollectionUtils::AddStringItemsUniquely(dst.lib, src.lib);
      CollectionUtils::AddStringItemsUniquely(dst.library, src.library);
      // Propagate C-CPP flags
      CollectionUtils::AddStringItemsUniquely(dst.c, dst.c_cpp);
      CollectionUtils::AddStringItemsUniquely(dst.cpp, dst.c_cpp);
    }
  }
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

  if (!ProjMgrYamlEmitter::GenerateCbuild(&context, false, generator->GetGeneratorName(),
    RtePackage::GetPackageIDfromAttributes(*generator->GetPackage()))) {
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
  if (m_dryRun && !generator->IsDryRunCapable(generatorExe)) {
    ProjMgrLogger::Error("generator '" + generatorId + "' is not dry-run capable");
    return false;
  }
  const string generatorCommand = generator->GetExpandedCommandLine(context.rteActiveTarget, RteUtils::EMPTY_STRING, m_dryRun);

  error_code ec;
  const auto& workingDir = fs::current_path(ec);
  RteFsUtils::CreateDirectories(generatorDestination);
  fs::current_path(generatorDestination, ec);
  StrIntPair result = CrossPlatformUtils::ExecCommand(generatorCommand);
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

bool ProjMgrWorker::ParseContextSelection(
  const vector<string>& contextSelection, const bool checkCbuildSet)
{
  vector<string> contexts;
  ListContexts(contexts);

  auto csolutionItem = m_parser->GetCsolution();
  string cbuildSetFile = csolutionItem.directory + "/" + csolutionItem.name + ".cbuild-set.yml";
  if (checkCbuildSet && RteFsUtils::Exists(cbuildSetFile)) {
    if (!m_parser->ParseCbuildSet(cbuildSetFile, m_checkSchema)) {
      return false;
    }
    const auto& cbuildSetItem = m_parser->GetCbuildSetItem();
    m_selectedContexts = cbuildSetItem.contexts;
    if (m_selectedToolchain.empty()) {
      m_selectedToolchain = cbuildSetItem.compiler;
    }
  }
  else {
    const auto& filterError = ProjMgrUtils::GetSelectedContexts(
      m_selectedContexts, contexts, contextSelection);
    if (filterError) {
      ProjMgrLogger::Error(filterError.m_errMsg);
      return false;
    }
  }

  if (!((m_selectedContexts.size() == 1) && (m_selectedContexts.front() == RteUtils::EMPTY_STRING))) {
    for (const auto& context : m_selectedContexts) {
      if (!ParseContextLayers(m_contexts[context])) {
        return false;
      }
    }
  }
  return true;
}

bool ProjMgrWorker::IsContextSelected(const string& context) {
  if (find(m_selectedContexts.begin(), m_selectedContexts.end(), context) != m_selectedContexts.end()) {
    return true;
  }
  return false;
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
  if (m_toolchainConfigFiles.empty()) {
    const string& compilerRoot = GetCompilerRoot();
    error_code ec;
    for (auto const& entry : fs::recursive_directory_iterator(compilerRoot, ec)) {
      string extn = entry.path().extension().string();
      if (entry.path().extension().string() != ".cmake") {
        continue;
      }
      m_toolchainConfigFiles.push_back(entry.path().generic_string());
    }
  }
  // find greatest compatible file
  bool found = false;
  static const regex regEx = regex("(\\w+)\\.(\\d+\\.\\d+\\.\\d+)");
  for (const auto& file : m_toolchainConfigFiles) {
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
  context.cproject->output.baseName =
    RteUtils::ExpandAccessSequences(context.cproject->output.baseName, context.variables);
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
      setup.output.baseName = RteUtils::ExpandAccessSequences(setup.output.baseName, context.variables);
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

  // from 'generators' node
  if (!GetGeneratorDir(generatorId, context, layer, genDir)) {
    return false;
  }

  if (!genDir.empty()) {
    genDir = RteFsUtils::RelativePath(genDir, context.cproject->directory);
  } else {
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

bool ProjMgrWorker::GetExtGeneratorDir(const string& generatorId, ContextItem& context, const string& layer, string& genDir) {
  // from 'generators' node
  if (!GetGeneratorDir(generatorId, context, layer, genDir)) {
    return false;
  }
  if (genDir.empty()) {
    // from global register
    genDir = m_extGenerator->GetGlobalGenDir(generatorId);
    if (!ProcessSequenceRelative(context, genDir, layer.empty() ? context.cproject->directory : context.clayers.at(layer)->directory)) {
      return false;
    }
  }
  if (genDir.empty()) {
    ProjMgrLogger::Error("generator output directory was not set");
    return false;
  }
  RteFsUtils::NormalizePath(genDir, context.directories.cprj);
  return true;
}

bool ProjMgrWorker::GetGeneratorDir(const string& generatorId, ContextItem& context, const string& layer, string& genDir) {
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
        RteFsUtils::NormalizePath(genDir, context.directories.cprj);
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
        genDir = fs::path(genDir).append(generatorId).generic_string();
        RteFsUtils::NormalizePath(genDir, context.directories.cprj);
        break;
      }
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
        CollectionUtils::PushBackUniquely(configFiles, configEntry);
      }
    }
  }
  return true;
}

std::string ProjMgrWorker::GetSelectedToochain(void) {
  return m_selectedToolchain;
}

bool ProjMgrWorker::ProcessGlobalGenerators(ContextItem* selectedContext, const string& generatorId,
  string& projectType, StrVec& siblings) {

  // iterate over contexts with same build and target types
  for (auto& [_, context] : m_contexts) {
    if ((context.type.build != selectedContext->type.build) ||
      (context.type.target != selectedContext->type.target)) {
      continue;
    }
    if (!ParseContextLayers(context)) {
      return false;
    }
    if (!ProcessContext(context, false, true, false)) {
      return false;
    }
  }
  const string& genDir = selectedContext->extGenDir[generatorId];
  const auto& contextVec = m_extGenerator->GetUsedGenerators().at(generatorId).at(genDir);

  // classify generator contexts according to device, processor and project name
  map<string, map<string, StrMap>> classifiedMap;
  for (const auto& contextId : contextVec) {
    auto& context = m_contexts[contextId];
    DeviceItem deviceItem;
    GetDeviceItem(context.device, deviceItem);
    classifiedMap[deviceItem.name][deviceItem.pname][context.cproject->name] = contextId;
  }

  // find project-type related contexts
  StrVecMap projectTypeMap;
  for (const auto& [device, processors] : classifiedMap) {
    for (const auto& [processor, projects] : processors) {
      for (const auto& [project, context] : projects) {
        string type;
        if (processors.size() >= 2) {
          type = TYPE_MULTI_CORE;
        } else {
          const auto& trustzone = m_contexts[context].controls.processed.processor.trustzone;
          type = (trustzone.empty() || trustzone == "off") ? TYPE_SINGLE_CORE : TYPE_TRUSTZONE;
        }
        projectTypeMap[type].push_back(context);
      }
    }
  }

  // get selected context project-type and siblings
  for (const auto& [type, contexts] : projectTypeMap) {
    if (find(contexts.begin(), contexts.end(), selectedContext->name) != contexts.end()) {
      projectType = type;
      siblings = contexts;
      return true;
    }
  }
  return false;
}

bool ProjMgrWorker::ExecuteExtGenerator(std::string& generatorId) {
  if (m_selectedContexts.size() != 1) {
    ProjMgrLogger::Error("a single context must be specified");
    return false;
  }
  const string& selectedContextId = m_selectedContexts.front();
  ContextItem* selectedContext = &m_contexts[selectedContextId];
  string projectType;
  StrVec siblings;
  if (!ProcessGlobalGenerators(selectedContext, generatorId, projectType, siblings)) {
    return false;
  }
  const string& genDir = selectedContext->extGenDir[generatorId];
  vector<ContextItem*> siblingContexts;
  for (const auto& sibling : siblings) {
    siblingContexts.push_back(&m_contexts[sibling]);
  }

  // Generate cbuild-gen files
  string cbuildgenOutput = selectedContext->directories.intdir;
  RteFsUtils::NormalizePath(cbuildgenOutput, selectedContext->directories.cprj);
  if (!ProjMgrYamlEmitter::GenerateCbuildGenIndex(*m_parser, siblingContexts, projectType, cbuildgenOutput, genDir)) {
    return false;
  }
  for (const auto& siblingContext : siblingContexts) {
    if (!ProjMgrYamlEmitter::GenerateCbuild(siblingContext, false, generatorId)) {
      return false;
    }
  }

  // Execute generator command
  string binDir = ProjMgrKernel::Get()->GetCmsisToolboxDir() + "/bin";
  string runCmd = m_extGenerator->GetGlobalGenRunCmd(generatorId);
  RteFsUtils::NormalizePath(runCmd, binDir);
  runCmd += " " + fs::path(cbuildgenOutput).append(m_parser->GetCsolution().name + ".cbuild-gen-idx.yml").generic_string();
  error_code ec;
  const auto& workingDir = fs::current_path(ec);
  fs::current_path(genDir, ec);
  StrIntPair result = CrossPlatformUtils::ExecCommand(runCmd);
  fs::current_path(workingDir, ec);
  ProjMgrLogger::Info("generator '" + generatorId + "' for context '" + selectedContextId + "' reported:\n" + result.first);
  if (result.second) {
    ProjMgrLogger::Error("executing generator '" + generatorId + "' for context '" + selectedContextId + "' failed");
    return false;
  }
  return true;
}

bool ProjMgrWorker::ProcessGeneratedLayers(ContextItem& context) {
  bool success;
  ClayerItem* cgen = m_extGenerator->GetGeneratorImport(context.name, success);
  if (!success) {
    return false;
  }
  if (cgen) {
    context.clayers[cgen->path] = cgen;
    if (cgen->packs.size() > 0) {
      vector<PackItem> packRequirements;
      InsertPackRequirements(cgen->packs, packRequirements, cgen->directory);
      AddPackRequirements(context, packRequirements);
      if (!LoadAllRelevantPacks() || !LoadPacks(context)) {
        PrintContextErrors(context.name);
        return false;
      }
    }
    if (!ProcessPrecedences(context, true)) {
      return false;
    }
    if (!ProcessGroups(context)) {
      return false;
    }
    if (!ProcessComponents(context)) {
      return false;
    }
  }
  return true;
}

void ProjMgrWorker::PrintContextErrors(const string& contextName) {
  auto elem = m_contextErrMap.find(contextName);
  if (elem != m_contextErrMap.end()) {
    std::for_each(elem->second.begin(), elem->second.end(),
      [](const auto& errMsg) {
        ProjMgrLogger::Error(errMsg);
      });
  }
}
