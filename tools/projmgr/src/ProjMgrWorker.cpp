/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgrWorker.h"
#include "ProjMgrLogger.h"
#include "ProjMgrUtils.h"
#include "ProjMgrYamlEmitter.h"

#include "CrossPlatformUtils.h"
#include "RteFsUtils.h"

#include <algorithm>
#include <iostream>
#include <regex>

using namespace std;

ProjMgrWorker::ProjMgrWorker(void) {
  m_loadPacksPolicy = LoadPacksPolicy::DEFAULT;
}

ProjMgrWorker::~ProjMgrWorker(void) {
  ProjMgrKernel::Destroy();
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

  // Default build-types
  if (context.cdefault) {
    for (const auto& defaultBuildType : context.cdefault->buildTypes) {
      if (context.csolution->buildTypes.find(defaultBuildType.first) == context.csolution->buildTypes.end()) {
        context.csolution->buildTypes.insert(defaultBuildType);
      }
    }
  }

  // No build/target-types
  if (context.csolution->buildTypes.empty() && context.csolution->targetTypes.empty()) {
    AddContext(parser, descriptor, { "" }, cprojectFile, context);
    return true;
  }

  // No build-types
  if (context.csolution->buildTypes.empty()) {
    for (const auto& targetTypeItem : context.csolution->targetTypes) {
      AddContext(parser, descriptor, {"", targetTypeItem.first}, cprojectFile, context);
    }
    return true;
  }

  // No target-types
  if (context.csolution->targetTypes.empty()) {
    for (const auto& buildTypeItem : context.csolution->buildTypes) {
      AddContext(parser, descriptor, {buildTypeItem.first, ""}, cprojectFile, context);
    }
    return true;
  }

  // Add contexts for project x build-type x target-type combinations
  for (const auto& buildTypeItem : context.csolution->buildTypes) {
    for (const auto& targetTypeItem : context.csolution->targetTypes) {
      AddContext(parser, descriptor, {buildTypeItem.first, targetTypeItem.first}, cprojectFile, context);
    }
  }
  return true;
}

bool ProjMgrWorker::AddContext(ProjMgrParser& parser, ContextDesc& descriptor, const TypePair& type, const string& cprojectFile, ContextItem& parentContext) {
  if (CheckType(descriptor.type, type)) {
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
    error_code ec;
    context.directories.gendir = fs::relative(context.cproject->directory + "/generated", context.csolution->directory, ec).generic_string();
    context.directories.rte = fs::relative(context.cproject->directory + "/RTE", context.csolution->directory, ec).generic_string();

    // customized directories
    if (m_outputDir.empty() && !context.csolution->directories.cprj.empty()) {
      context.directories.cprj = context.csolution->directory + "/" + context.csolution->directories.cprj;
    }
    if (!context.csolution->directories.gendir.empty()) {
      context.directories.gendir = context.csolution->directories.gendir;
    }
    if (!context.csolution->directories.intdir.empty()) {
      context.directories.intdir = context.csolution->directories.intdir;
    }
    if (!context.csolution->directories.outdir.empty()) {
      context.directories.outdir = context.csolution->directories.outdir;
    }
    if (!context.csolution->directories.rte.empty()) {
      context.directories.rte = context.csolution->directories.rte;
    }

    context.directories.cprj = fs::weakly_canonical(RteFsUtils::AbsolutePath(context.directories.cprj), ec).generic_string();

    for (const auto& clayer : context.cproject->clayers) {
      if (CheckType(clayer.type, type)) {
        string const& clayerFile = fs::canonical(fs::path(cprojectFile).parent_path().append(clayer.layer), ec).generic_string();
        std::map<std::string, ClayerItem>& clayers = parser.GetClayers();
        if (clayers.find(clayerFile) == clayers.end()) {
          ProjMgrLogger::Error(clayerFile, "clayer not parsed, adding context failed");
          return false;
        }
        context.clayers[clayerFile] = &clayers.at(clayerFile);
      }
    }

    m_contexts[context.name] = context;
  }
  return true;
}

void ProjMgrWorker::GetContexts(map<string, ContextItem>* &contexts) {
  m_contextsPtr = &m_contexts;
  contexts = m_contextsPtr;
}

void ProjMgrWorker::SetOutputDir(const std::string& outputDir) {
  m_outputDir = outputDir;
}

void ProjMgrWorker::SetLoadPacksPolicy(const LoadPacksPolicy& policy) {
  m_loadPacksPolicy = policy;
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

        RteAttributes attributes({
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

bool ProjMgrWorker::InitializeModel() {
  m_packRoot = CrossPlatformUtils::GetEnv("CMSIS_PACK_ROOT");
  if (m_packRoot.empty()) {
    m_packRoot = CrossPlatformUtils::GetDefaultCMSISPackRootDir();
  }
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
  error_code ec;
  m_packRoot = fs::weakly_canonical(fs::path(m_packRoot), ec).generic_string();
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
  if (!context.pdscFiles.empty()) {
    RteAttributesMap selectedPacks;
    for (const auto& pack : m_loadedPacks) {
      if (context.pdscFiles.find(pack->GetPackageFileName()) != context.pdscFiles.end()) {
        selectedPacks.insert({ pack->GetPackageID(), pack->GetAttributes() });
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
    for (const auto& rteWarningMessage : rteWarningMessages) {
      ProjMgrLogger::Warn(rteWarningMessage);
    }
    callback->ClearWarningMessages();
  }
  const list<string>& rteErrorMessages = callback->GetErrorMessages();
  if (!rteErrorMessages.empty()) {
    for (const auto& rteErrorMessage : rteErrorMessages) {
      ProjMgrLogger::Error(rteErrorMessage);
    }
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
    const string& targetName = context.name.empty() ? "CMSIS" : context.name;
    context.rteActiveProject->AddTarget(targetName, map<string, string>(), true, true);
    context.rteActiveProject->SetActiveTarget(targetName);
    context.rteActiveProject->SetName(targetName);
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

bool ProjMgrWorker::ProcessInterfaces(ContextItem& context) {
  // collect consumed and provided interfaces of project and layers
  vector<pair<string, string>*> consumedList;
  vector<pair<string, string>*> providedList;
  set<pair<string, string>> incompatibleList;
  for (auto& consumed : context.cproject->interfaces.consumes) {
    consumedList.push_back(&consumed);
  }
  for (auto& provided : context.cproject->interfaces.provides) {
    providedList.push_back(&provided);
  }
  for (const auto& [_, clayer] : context.clayers) {
    for (auto& consumed : clayer->interfaces.consumes) {
      consumedList.push_back(&consumed);
    }
    for (auto& provided : clayer->interfaces.provides) {
      providedList.push_back(&provided);
    }
  }
  // check whether consumed interfaces are compatible with provided ones
  // TODO: extend known interfaces validation
  for (const auto& consumed : consumedList) {
    bool found = false;
    for (const auto& provided : providedList) {
      if (provided->first == consumed->first) {
        // valid interface
        found = true;
        // add context define
        const string& define = "INTERFACE_" + provided->first + (provided->second.empty() ? "" : "=" + provided->second);
        ProjMgrUtils::PushBackUniquely(context.controls.processed.defines, define);
        break;
      }
    }
    if (!found) {
      incompatibleList.insert(*consumed);
    }
  }
  if (!incompatibleList.empty()) {
    string msg = "consumed interface(s) not provided:";
    for (const auto& [id, value] : incompatibleList) {
      msg += "\n" + id + (value.empty() ? "" : "=" + value);
    }
    ProjMgrLogger::Warn(msg);
  }
  return true;
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
  } else {
    context.targetAttributes["Dsecure"] = "Non-secure";
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
  std::vector<PackItem> packages;

  // Default package requirements
  if (context.cdefault) {
    for (const auto& packItem : context.cdefault->packs) {
      if (CheckType(packItem.type, context.type)) {
        packages.push_back(packItem);
      }
    }
  }
  // Add package requirements
  if (context.csolution) {
    for (const auto& packItem : context.csolution->packs) {
      if (CheckType(packItem.type, context.type)) {
        packages.push_back(packItem);
      }
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
    }
    else {
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
  if (context.toolchain.version.empty()) {
    static const map<string, string> DEF_MIN_VERSIONS = {
      {"AC5","5.6.7"},
      {"AC6","6.18.0"},
      {"GCC","11.2.1"},
      {"IAR","8.50.6"},
    };
    for (const auto& defMinVersion: DEF_MIN_VERSIONS) {
      if (context.toolchain.name == defMinVersion.first) {
        context.toolchain.version = defMinVersion.second;
        break;
      }
    }
    if (context.toolchain.version.empty()) {
      context.toolchain.version = "0.0.0";
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
  RteComponentMap installedComponents = context.rteActiveTarget->GetFilteredComponents();
  RteComponentMap componentMap;
  set<string> componentIds, filteredIds;
  for (auto& component : installedComponents) {
    const string& componentId = ProjMgrUtils::GetComponentID(component.second);
    componentIds.insert(componentId);
    componentMap[componentId] = component.second;
  }

  for (auto& [item, layer] : context.componentRequirements) {
    if (item.component.empty()) {
      continue;
    }
    // Filter components
    RteComponentMap filteredComponents;
    set<string> filteredIds;
    string componentDescriptor = item.component;

    set<string> filterSet;
    if (componentDescriptor.find_first_of(ProjMgrUtils::COMPONENT_DELIMITERS) != string::npos) {
      // Consider a full or partial component identifier was given
      filterSet.insert(RteUtils::GetPrefix(componentDescriptor, *(ProjMgrUtils::PREFIX_CVERSION)));
    } else {
      // Consider free text was given
      filterSet = SplitArgs(componentDescriptor);
    }

    ApplyFilter(componentIds, filterSet, filteredIds);
    for (const auto& filteredId : filteredIds) {
      filteredComponents[filteredId] = componentMap[filteredId];
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
    if (filteredComponents.size() > 1) {
      RteComponentMap matchedComponents;
      for (const auto& [id, component] : filteredComponents) {
        // Get component id without vendor and version
        const string& componentId = ProjMgrUtils::GetPartialComponentID(component);
        string requiredComponentId = RteUtils::RemovePrefixByString(item.component, ProjMgrUtils::SUFFIX_CVENDOR);
        requiredComponentId = RteUtils::GetPrefix(requiredComponentId, *ProjMgrUtils::PREFIX_CVERSION);
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
      ProjMgrLogger::Error("no component was found with identifier '" + item.component + "'");
      return false;
    }
    else {
      // One or multiple matches found
      set<string> availableComponentVersions;
      for_each(filteredComponents.begin(), filteredComponents.end(),
        [&](const pair<std::string, RteComponent*> &component) {
          availableComponentVersions.insert(RteUtils::GetSuffix(component.first, *ProjMgrUtils::PREFIX_CVERSION));
        });
      const string& filterVersion = RteUtils::GetSuffix(item.component, *ProjMgrUtils::PREFIX_CVERSION, true);
      const string& matchedVersion = VersionCmp::GetMatchingVersion(filterVersion, availableComponentVersions);
      if (matchedVersion.empty()) {
        ProjMgrLogger::Error("no component was found with identifier '" + item.component + "'");
        return false;
      }
      auto itr = std::find_if(filteredComponents.begin(), filteredComponents.end(),
        [&](const pair<std::string, RteComponent*>& item) {
          return (item.first.find(matchedVersion) != string::npos);
        });
      auto matchedComponent = itr->second;
      const auto& componentId = ProjMgrUtils::GetComponentID(matchedComponent);
      UpdateMisc(item.build.misc, context.toolchain.name);

      // Init matched component instance
      RteComponentInstance* matchedComponentInstance = new RteComponentInstance(matchedComponent);
      matchedComponentInstance->InitInstance(matchedComponent);

      // Set layer's rtePath attribute
      if (!layer.empty() && context.csolution->directories.rte.empty()) {
        error_code ec;
        const string& rteDir = fs::relative(context.clayers[layer]->directory, context.cproject->directory, ec).append("RTE").generic_string();
        matchedComponentInstance->AddAttribute("rtedir", rteDir);
      }

      // Insert matched component into context list
      context.components.insert({ componentId, { matchedComponentInstance, &item }});
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
  }

  // Get generators
  for (auto& [componentId, component] : context.components) {
    RteGenerator* generator = component.instance->GetParent()->GetComponent()->GetGenerator();
    if (generator) {
      const string generatorId = generator->GetID();
      component.generator = generatorId;
      context.generators.insert({ generatorId, generator });
      error_code ec;
      const string gpdsc = fs::weakly_canonical(generator->GetExpandedGpdsc(context.rteActiveTarget), ec).generic_string();
      context.gpdscs.insert({ gpdsc, {componentId, generatorId} });
    }
  }

  // Add required components into RTE
  if (!AddRequiredComponents(context)) {
    return false;
  }

  return CheckRteErrors();
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
      msg += "\n" + ProjMgrUtils::GetComponentID(component->GetComponent());
    }
    ProjMgrLogger::Error(msg);
    return false;
  }
  context.rteActiveProject->CollectSettings();

  // Generate RTE headers
  if (!context.rteActiveTarget->GenerateRteHeaders()) {
    return false;
  }
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
  if (context.linkerScript.empty()) {
    const auto& groups = context.rteActiveTarget->GetProjectGroups();
    for (auto group : groups) {
      for (auto file : group.second) {
        if (file.second.m_cat == RteFile::Category::LINKER_SCRIPT) {
          error_code ec;
          context.linkerScript = fs::relative(context.cproject->directory + "/" + file.first, context.directories.cprj, ec).generic_string();
          break;
        }
      }
    }
  }
  return true;
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
        const auto& attr = componentFile->GetAttribute("attr");
        const auto& category = componentFile->GetAttribute("category");
        const auto& version = attr == "config" ? componentFile->GetVersionString() : "";
        context.componentFiles[ProjMgrUtils::GetComponentID(component)].push_back({ file, attr, category, version });
      }
    }
  }
  // iterate over components
  for (const auto& [componentId, component] : context.components) {
    const RteComponent* rteComponent = component.instance->GetParent()->GetComponent();
    // pre-include files from packs
    for (const auto& componentFile : rteComponent->GetFileContainer()->GetChildren()) {
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
    for (const auto& configFileMap : context.configFiles) {
      if (configFileMap.first == componentId) {
        for (const auto& [_, configFile] : configFileMap.second) {
          const auto& filename = configFile->GetAbsolutePath();
          const auto& category = configFile->GetAttribute("category");
          const auto& originalFile = configFile->GetFile(context.rteActiveTarget->GetName());
          const auto& version = originalFile->GetVersionString();
          context.componentFiles[componentId].push_back({ filename, "config", category, version });
        }
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
    unique_ptr<RteGeneratorModel> gpdscModel = make_unique<RteGeneratorModel>();
    error_code ec;
    const string gpdscFile = fs::weakly_canonical(file, ec).generic_string();
    if (!ProjMgrUtils::ReadGpdscFile(gpdscFile, gpdscModel.get())) {
      ProjMgrLogger::Error(gpdscFile, "generator '" + context.gpdscs.at(gpdscFile).second +
        "' from component '" + context.gpdscs.at(gpdscFile).first + "': reading gpdsc failed");
      gpdscModel.reset();
      return false;
    } else {
      // Release pointer ownership
      info->SetGeneratorModel(gpdscModel.release());
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
     &context.controls.setup.compiler,
     &context.controls.csolution.compiler,
     &context.controls.target.compiler,
     &context.controls.build.compiler,
   },
  };
  for (const auto& [_, clayer] : context.clayers) {
    compiler.elements.push_back(&clayer->target.build.compiler);
  }
  if (!ProcessPrecedence(compiler)) {
    return false;
  }
  if (!ProcessToolchain(context)) {
    return false;
  }

  // Access sequences and relative path references must be processed
  // after board, device and compiler precedences (due to $Bname$, $Dname$ and $Compiler$)
  // but before processing misc, defines and includes precedences
  if (!ProcessSequencesRelatives(context)) {
    return false;
  }

  // Get content of project setup
  if (!GetProjectSetup(context)) {
    return false;
  }

  StringCollection trustzone = {
    &context.controls.processed.processor.trustzone,
    {
      &context.controls.cproject.processor.trustzone,
      &context.controls.setup.processor.trustzone,
      &context.controls.csolution.processor.trustzone,
      &context.controls.target.processor.trustzone,
      &context.controls.build.processor.trustzone,
    },
  };
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
      &context.controls.setup.processor.fpu,
      &context.controls.csolution.processor.fpu,
      &context.controls.target.processor.fpu,
      &context.controls.build.processor.fpu,
    },
  };
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
      &context.controls.setup.processor.endian,
      &context.controls.csolution.processor.endian,
      &context.controls.target.processor.endian,
      &context.controls.build.processor.endian,
    },
  };
  for (auto& [_, clayer] : context.controls.clayers) {
    endian.elements.push_back(&clayer.processor.endian);
  }
  if (!ProcessPrecedence(endian)) {
    return false;
  }

  // Optimize
  StringCollection optimize = {
    &context.controls.processed.optimize,
    {
      &context.controls.cproject.optimize,
      &context.controls.setup.optimize,
      &context.controls.csolution.optimize,
      &context.controls.target.optimize,
      &context.controls.build.optimize,
    },
  };
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
      &context.controls.setup.debug,
      &context.controls.csolution.debug,
      &context.controls.target.debug,
      &context.controls.build.debug,
    },
  };
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
      &context.controls.setup.warnings,
      &context.controls.csolution.warnings,
      &context.controls.target.warnings,
      &context.controls.build.warnings,
    },
  };
  for (auto& [_, clayer] : context.controls.clayers) {
    warnings.elements.push_back(&clayer.warnings);
  }
  if (!ProcessPrecedence(warnings)) {
    return false;
  }

  // Misc
  vector<vector<MiscItem>*> miscVec = {
    &context.controls.cproject.misc,
    &context.controls.setup.misc,
    &context.controls.csolution.misc,
    &context.controls.build.misc,
    &context.controls.target.misc,
  };
  for (auto& [_, clayer] : context.controls.clayers) {
    miscVec.push_back(&clayer.misc);
  }
  context.controls.processed.misc.push_back({});
  context.controls.processed.misc.front().compiler = context.toolchain.name;
  AddMiscUniquely(context.controls.processed.misc.front(), miscVec);

  // Defines
  vector<string> projectDefines, projectUndefines;
  AddStringItemsUniquely(projectDefines, context.controls.cproject.defines);
  for (auto& [_, clayer] : context.controls.clayers) {
    AddStringItemsUniquely(projectDefines, clayer.defines);
  }
  AddStringItemsUniquely(projectUndefines, context.controls.cproject.undefines);
  for (auto& [_, clayer] : context.controls.clayers) {
    AddStringItemsUniquely(projectUndefines, clayer.undefines);
  }
  StringVectorCollection defines = {
    &context.controls.processed.defines,
    {
      {&projectDefines, &projectUndefines},
      {&context.controls.setup.defines, &context.controls.setup.undefines},
      {&context.controls.csolution.defines, &context.controls.csolution.undefines},
      {&context.controls.target.defines, &context.controls.target.undefines},
      {&context.controls.build.defines, &context.controls.build.undefines},
    }
  };
  MergeStringVector(defines);

  // Includes
  vector<string> projectAddPaths, projectDelPaths;
  AddStringItemsUniquely(projectAddPaths, context.controls.cproject.addpaths);
  for (auto& [_, clayer] : context.controls.clayers) {
    AddStringItemsUniquely(projectAddPaths, clayer.addpaths);
  }
  AddStringItemsUniquely(projectDelPaths, context.controls.cproject.delpaths);
  for (auto& [_, clayer] : context.controls.clayers) {
    AddStringItemsUniquely(projectDelPaths, clayer.delpaths);
  }
  StringVectorCollection includes = {
    &context.controls.processed.addpaths,
    {
      {&projectAddPaths, &projectDelPaths},
      {&context.controls.setup.addpaths, &context.controls.setup.delpaths},
      {&context.controls.csolution.addpaths, &context.controls.csolution.delpaths},
      {&context.controls.target.addpaths, &context.controls.target.delpaths},
      {&context.controls.build.addpaths, &context.controls.build.delpaths},
    }
  };
  MergeStringVector(includes);

  return true;
}

bool ProjMgrWorker::ProcessSequencesRelatives(ContextItem& context) {

  // directories
  const string ref = m_outputDir.empty() ? context.csolution->directory : RteFsUtils::AbsolutePath(m_outputDir).generic_string();
  if (!ProcessSequenceRelative(context, context.directories.cprj) ||
      !ProcessSequenceRelative(context, context.directories.rte, context.csolution->directory) ||
      !ProcessSequenceRelative(context, context.directories.gendir, context.csolution->directory) ||
      !ProcessSequenceRelative(context, context.directories.outdir, ref) ||
      !ProcessSequenceRelative(context, context.directories.intdir, ref)) {
    return false;
  }

  // project, solution, target-type and build-type translation controls
  if (!ProcessSequencesRelatives(context, context.controls.cproject, context.cproject->directory) ||
      !ProcessSequencesRelatives(context, context.controls.setup, context.cproject->directory) ||
      !ProcessSequencesRelatives(context, context.controls.csolution, context.csolution->directory) ||
      !ProcessSequencesRelatives(context, context.controls.target, context.csolution->directory) ||
      !ProcessSequencesRelatives(context, context.controls.build, context.csolution->directory)) {
    return false;
  }

  // components translation controls
  for (ComponentItem component : context.cproject->components) {
    if (!ProcessSequencesRelatives(context, component.build, context.cproject->directory)) {
      return false;
    }
    if (!AddComponent(component, "", context.componentRequirements, context.type)) {
      return false;
    }
  }

  // layers translation controls
  for (const auto& [name, clayer] : context.clayers) {
    context.controls.clayers[name] = clayer->target.build;
    if (!ProcessSequencesRelatives(context, context.controls.clayers[name], clayer->directory)) {
      return false;
    }
    for (ComponentItem component : clayer->components) {
      if (!ProcessSequencesRelatives(context, component.build, clayer->directory)) {
        return false;
      }
      if (!AddComponent(component, name, context.componentRequirements, context.type)) {
        return false;
      }
    }
  }
  return true;
}

bool ProjMgrWorker::ProcessSequenceRelative(ContextItem& context, string& item, const string& ref) {
  size_t offset = 0;
  bool pathReplace = false;
  const string input = item;
  while (offset != string::npos) {
    string sequence;
    // get next access sequence
    if (!GetAccessSequence(offset, input, sequence, '$', '$')) {
      return false;
    }
    if (offset != string::npos) {
      string replacement;
      regex regEx;
      if (sequence == "Dname") {
        regEx = regex("\\$Dname\\$");
        replacement = context.device;
      }
      else if (sequence == "Bname") {
        regEx = regex("\\$Bname\\$");
        replacement = context.board;
      }
      else if (sequence == "Project") {
        regEx = regex("\\$Project\\$");
        replacement = context.cproject->name;
      }
      else if (sequence == "BuildType") {
        regEx = regex("\\$BuildType\\$");
        replacement = context.type.build;
      }
      else if (sequence == "TargetType") {
        regEx = regex("\\$TargetType\\$");
        replacement = context.type.target;
      }
      else if (sequence == "Compiler") {
        regEx = regex("\\$Compiler\\$");
        replacement = context.toolchain.name;
      }
      else if (regex_match(sequence, regex("(Output|OutDir|Source)\\(.*"))) {
        // Output, OutDir and Source access sequences lead to path replacement
        pathReplace = true;
        string contextName;
        size_t offsetContext = 0;
        if (!GetAccessSequence(offsetContext, sequence, contextName, '(', ')')) {
          return false;
        }
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
        if (m_contexts.find(contextName) != m_contexts.end()) {
          error_code ec;
          auto& depContext = m_contexts.at(contextName);
          if (!depContext.precedences) {
            if (!ProcessPrecedences(depContext)) {
              return false;
            }
          }
          const string& depContextOutDir = depContext.directories.cprj + "/" + depContext.directories.outdir;
          const string& relOutDir = fs::relative(depContextOutDir, context.directories.cprj, ec).generic_string();
          const string& relSrcDir = fs::relative(depContext.cproject->directory, context.directories.cprj, ec).generic_string();
          if (regex_match(sequence, regex("^OutDir\\(.*"))) {
            regEx = regex("\\$OutDir\\(.*\\)\\$");
            replacement = relOutDir;
          }
          else if (regex_match(sequence, regex("^Output\\(.*"))) {
            regEx = regex("\\$Output\\(.*\\)\\$");
            replacement = relOutDir + "/" + contextName;
          }
          else if (regex_match(sequence, regex("^Source\\(.*"))) {
            regEx = regex("\\$Source\\(.*\\)\\$");
            replacement = relSrcDir;
          }
        }
        else {
          // full or partial context name cannot be resolved to a valid context
          ProjMgrLogger::Error("context '" + contextName + "' referenced by access sequence '" + sequence + "' does not exist");
          return false;
        }
      }
      else {
        // access sequence is unknown
        ProjMgrLogger::Warn("unknown access sequence: '" + sequence + "'");
        continue;
      }
      item = regex_replace(item, regEx, replacement);
    }
  }
  if (!pathReplace && !ref.empty()) {
    // adjust relative path according to the given reference
    error_code ec;
    if (!fs::equivalent(context.directories.cprj, ref, ec)) {
      const string& absPath = fs::weakly_canonical(fs::path(item).is_relative() ? ref + "/" + item : item, ec).generic_string();
      item = fs::relative(absPath, context.directories.cprj, ec).generic_string();
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
  if (CheckType(src.type, context.type) && CheckCompiler(src.forCompiler, context.compiler)) {
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
  if (CheckType(src.type, context.type) && CheckCompiler(src.forCompiler, context.compiler)) {
    for (auto& dstNode : dst) {
      if (dstNode.file == src.file) {
        ProjMgrLogger::Error("conflict: file '" + dstNode.file + "' is declared multiple times");
        return false;
      }
    }

    // Set file category
    FileNode srcNode = src;
    if (srcNode.category.empty()) {
      srcNode.category = ProjMgrUtils::GetCategory(srcNode.file);
    }

    // Replace sequences and/or adjust file relative paths
    ProcessSequenceRelative(context, srcNode.file, root);
    ProcessSequencesRelatives(context, srcNode.build, root);
    UpdateMisc(srcNode.build.misc, context.toolchain.name);

    dst.push_back(srcNode);

    // Set linker script
    if ((srcNode.category == "linkerScript") && (context.linkerScript.empty())) {
      context.linkerScript = srcNode.file;
    }

    // Store absolute file path
    error_code ec;
    const string& filePath = fs::weakly_canonical(root + "/" + srcNode.file, ec).generic_string();
    context.filePaths.insert({ srcNode.file, filePath });
  }
  return true;
}

bool ProjMgrWorker::AddComponent(const ComponentItem& src, const string& layer, vector<pair<ComponentItem, string>>& dst, TypePair type) {
  if (CheckType(src.type, type)) {
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

bool ProjMgrWorker::CheckCompiler(const vector<string>& forCompiler, const string& selectedCompiler) {
  if (forCompiler.empty()) {
    return true;
  }
  const ToolchainItem& selectedToolchain = GetToolchain(selectedCompiler);
  for (const auto& compiler : forCompiler) {
    const ToolchainItem& toolchain = GetToolchain(compiler);
    if (toolchain.name == selectedToolchain.name &&
       (toolchain.version.empty() || toolchain.version == selectedToolchain.version)) {
      return true;
    }
  }
  return false;
}

bool ProjMgrWorker::CheckType(const TypeFilter& typeFilter, const TypePair& type) {
  const auto& exclude = typeFilter.exclude;
  const auto& include = typeFilter.include;

  if (include.empty()) {
    if (exclude.empty()) {
      return true;
    } else {
      // check not-for types
      for (const auto& excType : typeFilter.exclude) {
        if (((excType.build == type.build) && excType.target.empty()) ||
          ((excType.target == type.target) && excType.build.empty()) ||
          ((excType.build == type.build) && (excType.target == type.target))) {
          return false;
        }
      }
      return true;
    }
  } else {
    // check for-types
    for (const auto& incType : typeFilter.include) {
      if (((incType.build == type.build) && incType.target.empty()) ||
        ((incType.target == type.target) && incType.build.empty()) ||
        ((incType.build == type.build) && (incType.target == type.target))) {
        return true;
      }
    }
    return false;
  }
}

bool ProjMgrWorker::ProcessContext(ContextItem& context, bool loadGpdsc, bool resolveDependencies, bool updateRteFiles) {
  context.outputType = context.cproject->outputType.empty() ? "exe" : context.cproject->outputType;

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
  if (!ProcessInterfaces(context)) {
    return false;
  }
  if (!SetTargetAttributes(context, context.targetAttributes)) {
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
      ProjMgrLogger::Warn(msg);
    }
  }
  return true;
}

void ProjMgrWorker::ApplyFilter(const set<string>& origin, const set<string>& filter, set<string>& result) {
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
      result.insert(item);
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
  if (!filter.empty()) {
    set<string> filteredPacks;
    ApplyFilter(packsSet, SplitArgs(filter), filteredPacks);
    if (filteredPacks.empty()) {
      ProjMgrLogger::Error("no pack was found with filter '" + filter + "'");
      return false;
    }
    packsSet = filteredPacks;
  }
  packs.assign(packsSet.begin(), packsSet.end());
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
  if (!filter.empty()) {
    set<string> matchedBoards;
    ApplyFilter(boardsSet, SplitArgs(filter), matchedBoards);
    if (matchedBoards.empty()) {
      ProjMgrLogger::Error("no board was found with filter '" + filter + "'");
      return false;
    }
    boardsSet = matchedBoards;
  }
  boards.assign(boardsSet.begin(), boardsSet.end());
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
  if (!filter.empty()) {
    set<string> matchedDevices;
    ApplyFilter(devicesSet, SplitArgs(filter), matchedDevices);
    if (matchedDevices.empty()) {
      ProjMgrLogger::Error("no device was found with filter '" + filter + "'");
      return false;
    }
    devicesSet = matchedDevices;
  }
  devices.assign(devicesSet.begin(), devicesSet.end());
  return true;
}

bool ProjMgrWorker::ListComponents(vector<string>& components, const string& filter) {
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
      if (!ProcessToolchain(context)) {
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
  if (!filter.empty()) {
    set<string> filteredIds;
    ApplyFilter(componentIds, SplitArgs(filter), filteredIds);
    if (filteredIds.empty()) {
      ProjMgrLogger::Error("no component was found with filter '" + filter + "'");
      return false;
    }
    componentIds = filteredIds;
  }
  for (const auto& componentId : componentIds) {
    components.push_back(componentId + " (" + ProjMgrUtils::GetPackageID(componentMap[componentId]->GetPackage()) + ")");
  }
  return true;
}

bool ProjMgrWorker::ListDependencies(vector<string>& dependencies, const string& filter) {
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
  if (!filter.empty()) {
    set<string> filteredDependencies;
    ApplyFilter(dependenciesSet, SplitArgs(filter), filteredDependencies);
    if (filteredDependencies.empty()) {
      ProjMgrLogger::Error("no unresolved dependency was found with filter '" + filter + "'");
      return false;
    }
    dependenciesSet = filteredDependencies;
  }
  dependencies.assign(dependenciesSet.begin(), dependenciesSet.end());
  return true;
}

bool ProjMgrWorker::FormatValidationResults(set<string>& results, const ContextItem& context) {
  for (const auto& [result, component, expressions, aggregates] : context.validationResults) {
    static const map<RteItem::ConditionResult, string> RESULTS = {
      { RteItem::UNDEFINED             , "UNDEFINED"            },
      { RteItem::R_ERROR               , "R_ERROR"              },
      { RteItem::FAILED                , "FAILED"               },
      { RteItem::MISSING               , "MISSING"              },
      { RteItem::MISSING_API           , "MISSING_API"          },
      { RteItem::MISSING_API_VERSION   , "MISSING_API_VERSION"  },
      { RteItem::UNAVAILABLE           , "UNAVAILABLE"          },
      { RteItem::UNAVAILABLE_PACK      , "UNAVAILABLE_PACK"     },
      { RteItem::INCOMPATIBLE          , "INCOMPATIBLE"         },
      { RteItem::INCOMPATIBLE_VERSION  , "INCOMPATIBLE_VERSION" },
      { RteItem::INCOMPATIBLE_VARIANT  , "INCOMPATIBLE_VARIANT" },
      { RteItem::CONFLICT              , "CONFLICT"             },
      { RteItem::INSTALLED             , "INSTALLED"            },
      { RteItem::SELECTABLE            , "SELECTABLE"           },
      { RteItem::FULFILLED             , "FULFILLED"            },
      { RteItem::IGNORED               , "IGNORED"              },
    };
    string resultStr = (RESULTS.find(result) == RESULTS.end() ? RESULTS.at(RteItem::UNDEFINED) : RESULTS.at(result)) + " " + component;
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

bool ProjMgrWorker::ListContexts(vector<string>& contexts, const string& filter) {
  if (m_contexts.empty()) {
    return false;
  }
  set<string>contextsSet;
  for (auto& context : m_contexts) {
    contextsSet.insert(context.first);
  }
  if (!filter.empty()) {
    set<string> filteredContexts;
    ApplyFilter(contextsSet, SplitArgs(filter), filteredContexts);
    if (filteredContexts.empty()) {
      ProjMgrLogger::Error("no context was found with filter '" + filter + "'");
      return false;
    }
    contextsSet = filteredContexts;
  }
  contexts.assign(contextsSet.begin(), contextsSet.end());
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
      generatorsSet.insert(id + " (" + generator->GetText() + ")");
    }
  }
  generators.assign(generatorsSet.begin(), generatorsSet.end());
  return true;
}

ToolchainItem ProjMgrWorker::GetToolchain(const string& compiler) {
  ToolchainItem toolchain;
  if (compiler.find("@") != string::npos) {
    toolchain.name = RteUtils::RemoveSuffixByString(compiler, "@");
    toolchain.version = RteUtils::RemovePrefixByString(compiler, "@");
  }
  else {
    toolchain.name = compiler;
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
  return true;
}

bool ProjMgrWorker::GetProjectSetup(ContextItem& context) {
  for (const auto& setup : context.cproject->setups) {
    if (CheckType(setup.type, context.type) && CheckCompiler(setup.forCompiler, context.compiler)) {
      context.controls.setup = setup.build;
      break;
    }
  }
  return true;
}

void ProjMgrWorker::UpdateMisc(vector<MiscItem>& vec, const string& compiler) {
  // Filter and adjust vector of MiscItem, leaving a single compiler compatible item
  MiscItem dst;
  dst.compiler = compiler;
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
    if (src.compiler.empty() || (src.compiler == dst.compiler)) {
      // Copy individual flags
      AddStringItemsUniquely(dst.as, src.as);
      AddStringItemsUniquely(dst.c, src.c);
      AddStringItemsUniquely(dst.cpp, src.cpp);
      AddStringItemsUniquely(dst.c_cpp, src.c_cpp);
      AddStringItemsUniquely(dst.link, src.link);
      AddStringItemsUniquely(dst.link_c, src.link_c);
      AddStringItemsUniquely(dst.link_cpp, src.link_cpp);
      AddStringItemsUniquely(dst.lib, src.lib);
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
  for (const auto& selectedContext : m_selectedContexts) {
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
    string generatorDestination = context.directories.gendir;
    if (generatorDestination.empty()) {
      generatorDestination = generator->GetExpandedWorkingDir(context.rteActiveTarget);  // Fallback to working dir if no specific generator directory was specified
    }

    // Make sure the generatorDestination is absolute
    if (fs::path(generatorDestination).is_relative()) {
      generatorDestination = context.rteActiveProject->GetProjectPath() + generatorDestination;
    }

    // Make the generatorDestination is a folder by adding a '/' to the end
    if (!generatorDestination.empty() && generatorDestination.back() != '/') {
      generatorDestination += '/';
    }

    const auto generatorInputFilePath = ProjMgrYamlEmitter::EmitContextInfo(context, generatorDestination);

    if (!generatorInputFilePath) {
      ProjMgrLogger::Error("Failed to create the generator input file for '" + generatorId + "'");
      return false;
    }

    // Update RteTarget with current generatorInputFilePath that was created
    context.rteActiveTarget->SetGeneratorInputFile(*generatorInputFilePath);

    // TODO: review RteGenerator::GetExpandedCommandLine and variables
    //const string generatorCommand = m_kernel->GetCmsisPackRoot() + "/" + generator->GetPackagePath() + generator->GetCommand();
    const string generatorCommand = generator->GetExpandedCommandLine(context.rteActiveTarget);
    if (generatorCommand.empty()) {
      ProjMgrLogger::Error("generator command for '" + generatorId + "' was not found");
      return false;
    }


    error_code ec;
    const auto& workingDir = fs::current_path(ec);
    fs::current_path(generatorDestination, ec);
    ProjMgrUtils::Result result = ProjMgrUtils::ExecCommand(generatorCommand);
    fs::current_path(workingDir, ec);

    ProjMgrLogger::Info("generator '" + generatorId + "' for context '" + selectedContext + "' reported:\n" + result.first);

    if (result.second) {
      ProjMgrLogger::Error("executing generator '" + generatorId + "' for context '" + selectedContext + "' failed");
      return false;
    }
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

bool ProjMgrWorker::ProcessSequencesRelatives(ContextItem& context, vector<string>& src, const string& ref) {
  for (auto& item : src) {
    if (!ProcessSequenceRelative(context, item, ref)) {
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
    if (!ProcessSequencesRelatives(context, misc.as)     ||
        !ProcessSequencesRelatives(context, misc.c)      ||
        !ProcessSequencesRelatives(context, misc.cpp)    ||
        !ProcessSequencesRelatives(context, misc.c_cpp)  ||
        !ProcessSequencesRelatives(context, misc.lib)    ||
        !ProcessSequencesRelatives(context, misc.link)   ||
        !ProcessSequencesRelatives(context, misc.link_c) ||
        !ProcessSequencesRelatives(context, misc.link_cpp)) {
      return false;
    }
  }
  return true;
}

bool ProjMgrWorker::ParseContextSelection(const string& contextSelection) {
  vector<string> contexts;
  ListContexts(contexts);
  m_selectedContexts.clear();
  if (contextSelection.empty()) {
    if (contexts.empty()) {
      // default context
      m_selectedContexts.push_back("");
    } else {
      // select all contexts
      for (const auto& context : contexts) {
        m_selectedContexts.push_back(context);
      }
    }
  } else {
    bool match = false;
    for (const auto& context : contexts) {
      if (WildCards::Match(context, contextSelection)) {
        m_selectedContexts.push_back(context);
        match = true;
      }
    }
    if (!match) {
      ProjMgrLogger::Error("context '" + contextSelection + "' was not found");
      return false;
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
