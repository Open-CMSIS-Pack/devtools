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
  // Reserved
}

ProjMgrWorker::~ProjMgrWorker(void) {
  // Reserved
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
  context.directories.cproject = fs::path(cprojectFile).parent_path().generic_string();

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
    const string& projectName = fs::path(cprojectFile).stem().stem().generic_string();
    const string& contextName = projectName + buildType + targetType;
    context.name = contextName;

    context.directories.intdir = context.name + "_IntDir/";
    context.directories.outdir = context.name + "_OutDir/";
    error_code ec;
    const string& cprjDir = m_outputDir.empty() ? context.directories.cproject : m_outputDir + "/" + context.name;
    context.directories.cprj = fs::absolute(cprjDir, ec).generic_string();

    for (const auto& clayer : context.cproject->clayers) {
      if (CheckType(clayer.type, type)) {
        error_code ec;
        string const& clayerFile = fs::canonical(fs::path(cprojectFile).parent_path().append(clayer.layer), ec).generic_string();
        std::map<std::string, ClayerItem>& clayers = parser.GetClayers();
        if (clayers.find(clayerFile) == clayers.end()) {
          ProjMgrLogger::Error(clayerFile, "clayer not parsed, adding context failed");
          return false;
        }
        context.clayers[clayerFile] = &clayers.at(clayerFile);
      }
    }

    m_contexts[contextName] = context;
  }
  return true;
}

void ProjMgrWorker::GetContexts(map<string, ContextItem>* &contexts) {
  contexts = &m_contexts;
}

void ProjMgrWorker::SetOutputDir(const std::string& outputDir) {
  m_outputDir = outputDir;
}

bool ProjMgrWorker::GetRequiredPdscFiles(ContextItem& context, const std::string& packRoot) {
  std::vector<std::string> errMsgs;
  if (!ProcessPackages(context)) {
    return false;
  }
  for (auto packItem : context.packRequirements) {
    if (packItem.path.empty()) {
      auto pack = packItem.pack;
      bool bPackFilter = (pack.name.empty() || WildCards::IsWildcardPattern(pack.name));
      auto filteredPackItems = GetFilteredPacks(packItem, packRoot);
      for (const auto& filteredPackItem : filteredPackItems) {
        auto filteredPack = filteredPackItem.pack;
        string packId, pdscFile, packversion;
        if (!filteredPack.version.empty()) {
          packversion = filteredPack.version + ":" + filteredPack.version;
        }

        RteAttributes attributes({
          {"name",    filteredPack.name},
          {"vendor",  filteredPack.vendor},
          {"version", packversion},
          });

        pdscFile = m_kernel->GetLocalPdscFile(attributes, packRoot, packId);
        if (pdscFile.empty()) {
          pdscFile = m_kernel->GetInstalledPdscFile(attributes, packRoot, packId);
        }
        if (pdscFile.empty()) {
          if (!bPackFilter) {
            std::string packageName =
              (filteredPack.vendor.empty() ? "" : filteredPack.vendor + "::") +
              filteredPack.name +
              (filteredPack.version.empty() ? "" : "@" + filteredPack.version);
            errMsgs.push_back("required pack: " + packageName + " not found");
          }
          continue;
        }
        context.pdscFiles.insert({ pdscFile, "" });
      }
      if (bPackFilter && context.pdscFiles.empty()) {
        std::string filterStr = pack.vendor +
          (pack.name.empty() ? "" : "::" + pack.name) +
          (pack.version.empty() ? "" : "@" + pack.version);
        errMsgs.push_back("no match found for pack filter: " + filterStr);
      }
    }
    else {
      error_code ec;
      std::string packPath = fs::path(context.csolution->path).parent_path().generic_string() + "/" + packItem.path;
      if (!fs::exists(packPath)) {
        errMsgs.push_back("pack path: " + packItem.path + " does not exist");
        break;
      }
      packPath = fs::canonical(packPath, ec).generic_string();
      auto pdscFilesList = RteFsUtils::FindFiles(packPath, ".pdsc");
      if (pdscFilesList.empty()) {
        errMsgs.push_back("no pdsc file found under: " + packItem.path);
        break;
      }

      if (pdscFilesList.size() > 1) {
        ProjMgrLogger::Warn("no pack loaded as multiple pdsc files found under: " + packItem.path);
      }
      else {
        context.pdscFiles.insert({ pdscFilesList[0].generic_string(), fs::path(packItem.path).generic_string() });
      }
    }
  }
  std::for_each(errMsgs.begin(), errMsgs.end(), [](const auto& errMsg) {ProjMgrLogger::Error(errMsg); });
  return (0 == errMsgs.size());
}

bool ProjMgrWorker::InitializeModel() {
  string packRoot = CrossPlatformUtils::GetEnv("CMSIS_PACK_ROOT");
  if (packRoot.empty()) {
    packRoot = CrossPlatformUtils::GetDefaultCMSISPackRootDir();
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
  m_kernel->SetCmsisPackRoot(packRoot);
  m_model->SetCallback(m_kernel->GetCallback());

  // Get pack selection for each context
  std::set<std::string> pdscFiles;
  for (auto& [_, contextItem] : m_contexts) {
    if (!GetRequiredPdscFiles(contextItem, packRoot)) {
      return false;
    }
    for (const auto& [pdscFile, _] : contextItem.pdscFiles) {
      pdscFiles.insert(pdscFile);
    }
  }
  if (pdscFiles.empty()) {
    // Get installed packs
    if (!m_kernel->GetInstalledPacks(pdscFiles)) {
      ProjMgrLogger::Error("parsing installed packs failed");
      return false;
    }
  }
  if (!m_kernel->LoadAndInsertPacks(m_loadedPacks, pdscFiles)) {
    ProjMgrLogger::Error("failed to load and insert packs");
    return false;
  }
  return true;
}

bool ProjMgrWorker::LoadPacks(ContextItem& context) {
  if (m_loadedPacks.empty() && !InitializeModel()) {
    return false;
  }
  if (!InitializeTarget(context)) {
    return false;
  }
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
          filteredPacks.push_back({{ dirName, pack.vendor, packItem.pack.version }});
        }
      }
    }
  }
  return filteredPacks;
}

bool ProjMgrWorker::CheckRteErrors(void) {
  const list<string>& rteErrorMessages = m_kernel->GetCallback()->GetErrorMessages();
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
    context.rteActiveProject->AddTarget("CMSIS", map<string, string>(), true, true);
    context.rteActiveProject->SetActiveTarget("CMSIS");
    context.rteActiveProject->SetName(context.name);
    context.rteActiveProject->SetProjectPath(context.directories.cprj + "/");
    context.rteActiveTarget = context.rteActiveProject->GetActiveTarget();
    context.rteFilteredModel = context.rteActiveTarget->GetFilteredModel();
  }
  return CheckRteErrors();
}

bool ProjMgrWorker::SetTargetAttributes(ContextItem& context, map<string, string>& attributes) {
  if (context.rteActiveTarget == nullptr) {
    InitializeTarget(context);
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
  if (!element.empty()) {
    const size_t vendorDelimiter = element.find("::");
    if (vendorDelimiter != string::npos) {
      board.vendor = element.substr(0, vendorDelimiter);
      board.name = element.substr(vendorDelimiter + 2, string::npos);
    }
    else {
      board.name = element;
    }
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
    for (const auto& [_, board] : availableBoards) {
      if (board->GetName() == boardItem.name) {
        if (boardItem.vendor.empty() || (boardItem.vendor == DeviceVendor::GetCanonicalVendorName(board->GetVendorName()))) {
          matchedBoard = board;
          const auto& boardPackage = matchedBoard->GetPackage();
          context.packages.insert({ ProjMgrUtils::GetPackageID(boardPackage), boardPackage });
          context.targetAttributes["Bname"]    = matchedBoard->GetName();
          context.targetAttributes["Bvendor"]  = matchedBoard->GetVendorName();
          context.targetAttributes["Bversion"] = matchedBoard->GetAttribute("revision");
          break;
        }
      }
    }
    if (!matchedBoard) {
      ProjMgrLogger::Error("board '" + context.board + "' was not found");
      return false;
    }

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
      ProjMgrLogger::Error("specified device '" + deviceItem.name + "' was not found");
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
      ProjMgrLogger::Error("specified device '" + DeviceInfoString + "' and board mounted device '" +
        BoardDeviceInfoString + "' are different");
      return false;
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
    for (const auto& processor : processors) {
      msg += "\n" + matchedDevice->GetDeviceName() + ":" + processor.first;
    }
    ProjMgrLogger::Error(msg);
    return false;
  }

  const auto& processorAttributes = processor->GetAttributes();
  context.targetAttributes.insert(processorAttributes.begin(), processorAttributes.end());
  context.targetAttributes["Dvendor"] = matchedDevice->GetEffectiveAttribute("Dvendor");
  context.targetAttributes["Dname"] = matchedDevice->GetFullDeviceName();

  if (!context.fpu.empty()) {
    if (context.fpu == "on") {
      context.targetAttributes["Dfpu"] = "FPU";
    } else if (context.fpu == "off") {
      context.targetAttributes["Dfpu"] = "NO_FPU";
    }
  }
  if (!context.endian.empty()) {
    if (context.endian == "big") {
      context.targetAttributes["Dendian"] = "Big-endian";
    } else if (context.endian == "little") {
      context.targetAttributes["Dendian"] = "Little-endian";
    }
  }
  if (!context.trustzone.empty()) {
    if (context.trustzone == "secure") {
      context.targetAttributes["Dsecure"] = "Secure";
    } else if (context.trustzone == "non-secure") {
      context.targetAttributes["Dsecure"] = "Non-secure";
    } else if (context.trustzone == "off") {
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
  string boardVendor, boardName;

  for (const auto& element : item.elements) {
    GetBoardItem(*element, board);
    if (!(GetPrecedentValue(boardVendor, board.vendor) &&
      GetPrecedentValue(boardName, board.name))) {
      return false;
    }
  }
  *item.assign = (boardVendor.empty() ? "" : boardVendor) +
    (boardVendor.empty() ? "" : "::") + boardName;
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

  const size_t delimiter = context.compiler.find('@');
  context.toolchain.name = context.compiler.substr(0, delimiter);
  if (delimiter == string::npos) {
    static const map<string, string> DEF_MIN_VERSIONS = {
      {"AC5","5.6.7"},
      {"AC6","6.16.0"},
      {"GCC","10.2.1"},
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
  } else {
    context.toolchain.version = context.compiler.substr(delimiter + 1, string::npos);
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
  // Add cproject component requirements
  for (const auto& component : context.cproject->components) {
    if (!AddComponent(component, context.componentRequirements, context.type)) {
      return false;
    }
  }
  // Add clayers component requirements
  for (const auto& clayer : context.clayers) {
    for (const auto& component : clayer.second->components) {
      if (!AddComponent(component, context.componentRequirements, context.type)) {
        return false;
      }
    }
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
  bool valid = true;
  for (auto& item : context.componentRequirements) {
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
      filterSet.insert(componentDescriptor);
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
        const string& requiredComponentId = RteUtils::RemovePrefixByString(item.component, ProjMgrUtils::SUFFIX_CVENDOR);
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
    if (filteredComponents.size() == 1) {
      // Single match
      auto matchedComponent = filteredComponents.begin()->second;
      const auto& componentId = ProjMgrUtils::GetComponentID(matchedComponent);
      MergeMiscCPP(item.build.misc);
      context.components.insert({ componentId, { matchedComponent, &item }});
      const auto& componentPackage = matchedComponent->GetPackage();
      context.packages.insert({ ProjMgrUtils::GetPackageID(componentPackage), componentPackage });
      if (matchedComponent->HasApi(context.rteActiveTarget)) {
        const auto& api = matchedComponent->GetApi(context.rteActiveTarget, false);
        if (api) {
          const auto& apiPackage = api->GetPackage();
          context.packages.insert({ ProjMgrUtils::GetPackageID(apiPackage), apiPackage });
        }
      }
    } else if (filteredComponents.empty()) {
      // No match
      ProjMgrLogger::Error("no component was found with identifier '" + item.component + "'");
      valid = false;
    } else {
      // Multiple matches
      string msg = "multiple components were found for identifier '" + item.component + "'";
      for (const auto& component : filteredComponents) {
        msg += "\n" + ProjMgrUtils::GetComponentID(component.second) + " in pack " + component.second->GetPackage()->GetPackageFileName();
      }
      ProjMgrLogger::Error(msg);
      valid = false;
    }
  }
  if (!valid) {
    return false;
  }

  // Get generators
  for (const auto& [componentId, component] : context.components) {
    RteGenerator* generator = component.first->GetGenerator();
    if (generator) {
      const string generatorId = generator->GetID();
      context.generators.insert({ generatorId, generator });
      const string gpdsc = generator->GetExpandedGpdsc(context.rteActiveTarget);
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
  for (const auto& component : context.components) {
    selItems.push_back(component.second.first);
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
          context.linkerScript = file.first;
          break;
        }
      }
    }
  }
  return true;
}

bool ProjMgrWorker::ProcessDependencies(ContextItem& context) {
  // Read gpdsc
  const map<string, RteGpdscInfo*>& gpdscInfos = context.rteActiveProject->GetGpdscInfos();
  for (const auto& [_, info] : gpdscInfos) {
    unique_ptr<RteGeneratorModel> gpdscModel = make_unique<RteGeneratorModel>();
    const string& gpdscFile = info->GetAbsolutePath();
    if (!ProjMgrUtils::ReadGpdscFile(gpdscFile, gpdscModel.get())) {
      ProjMgrLogger::Warn(gpdscFile, "generator '" + context.gpdscs.at(gpdscFile).second +
        "' from component '" + context.gpdscs.at(gpdscFile).first + "': reading gpdsc failed");
      gpdscModel.reset();
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

  // Get dependency results
  map<const RteItem*, RteDependencyResult> results;
  context.rteActiveTarget->GetSelectedDepsResult(results, context.rteActiveTarget);
  for (const auto& [component, result] : results) {
    const auto& deps = result.GetResults();
    RteItem::ConditionResult r = result.GetResult();
    if ((r == RteItem::MISSING) || (r == RteItem::SELECTABLE)) {
      set<string> dependenciesSet;
      for (const auto& dep : deps) {
        dependenciesSet.insert(ProjMgrUtils::GetConditionID(dep.first));
      }
      if (!dependenciesSet.empty()) {
        context.dependencies.insert({ ProjMgrUtils::GetComponentID(component), dependenciesSet });
      }
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

  // Get content of build and target types
  if (!GetTypeContent(context)) {
    return false;
  }

  // Misc
  vector<vector<MiscItem>*> miscVec = {
    &context.cproject->target.build.misc,
    &context.csolution->target.build.misc,
    &context.buildType.misc,
    &context.targetType.build.misc,
  };
  for (const auto& clayer : context.clayers) {
    miscVec.push_back(&clayer.second->target.build.misc);
  }
  AddMiscUniquely(context.misc, miscVec);
  MergeMiscCPP(context.misc);

  // Defines
  vector<string> projectDefines, projectUndefines;
  AddStringItemsUniquely(projectDefines, context.cproject->target.build.defines);
  for (const auto& clayer : context.clayers) {
    AddStringItemsUniquely(projectDefines, clayer.second->target.build.defines);
  }
  AddStringItemsUniquely(projectUndefines, context.cproject->target.build.undefines);
  for (const auto& clayer : context.clayers) {
    AddStringItemsUniquely(projectUndefines, clayer.second->target.build.undefines);
  }
  StringVectorCollection defines = {
    &context.defines,
    {
      {&projectDefines, &projectUndefines},
      {&context.csolution->target.build.defines, &context.csolution->target.build.undefines},
      {&context.targetType.build.defines, &context.targetType.build.undefines},
      {&context.buildType.defines, &context.buildType.undefines},
    }
  };
  MergeStringVector(defines);

  // Includes
  vector<string> projectAddPaths, projectDelPaths;
  AddStringItemsUniquely(projectAddPaths, context.cproject->target.build.addpaths);
  for (const auto& clayer : context.clayers) {
    AddStringItemsUniquely(projectAddPaths, clayer.second->target.build.addpaths);
  }
  AddStringItemsUniquely(projectDelPaths, context.cproject->target.build.delpaths);
  for (const auto& clayer : context.clayers) {
    AddStringItemsUniquely(projectDelPaths, clayer.second->target.build.delpaths);
  }
  StringVectorCollection includes = {
    &context.includes,
    {
      {&projectAddPaths, &projectDelPaths},
      {&context.csolution->target.build.addpaths, &context.csolution->target.build.delpaths},
      {&context.targetType.build.addpaths, &context.targetType.build.delpaths},
      {&context.buildType.addpaths, &context.buildType.delpaths},
    }
  };
  MergeStringVector(includes);

  StringCollection compiler = {
    &context.compiler,
    {
      &context.cproject->target.build.compiler,
      &context.csolution->target.build.compiler,
      &context.targetType.build.compiler,
      &context.buildType.compiler,
    },
  };
  for (const auto& clayer : context.clayers) {
    compiler.elements.push_back(&clayer.second->target.build.compiler);
  }
  if (!ProcessPrecedence(compiler)) {
    return false;
  }

  StringCollection board = {
  &context.board,
    {
      &context.cproject->target.board,
      &context.csolution->target.board,
      &context.targetType.board,
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
      &context.targetType.device,
    },
  };
  for (const auto& clayer : context.clayers) {
    device.elements.push_back(&clayer.second->target.device);
  }
  if (!ProcessDevicePrecedence(device)) {
    return false;
  }

  StringCollection trustzone = {
    &context.trustzone,
    {
      &context.cproject->target.build.processor.trustzone,
      &context.csolution->target.build.processor.trustzone,
      &context.targetType.build.processor.trustzone,
      &context.buildType.processor.trustzone,
    },
  };
  for (const auto& clayer : context.clayers) {
    trustzone.elements.push_back(&clayer.second->target.build.processor.trustzone);
  }
  if (!ProcessPrecedence(trustzone)) {
    return false;
  }

  StringCollection fpu = {
    &context.fpu,
    {
      &context.cproject->target.build.processor.fpu,
      &context.csolution->target.build.processor.fpu,
      &context.targetType.build.processor.fpu,
      &context.buildType.processor.fpu,
    },
  };
  for (const auto& clayer : context.clayers) {
    fpu.elements.push_back(&clayer.second->target.build.processor.fpu);
  }
  if (!ProcessPrecedence(fpu)) {
    return false;
  }

  StringCollection endian = {
    &context.endian,
    {
      &context.cproject->target.build.processor.endian,
      &context.csolution->target.build.processor.endian,
      &context.targetType.build.processor.endian,
      &context.buildType.processor.endian,
    },
  };
  for (const auto& clayer : context.clayers) {
    endian.elements.push_back(&clayer.second->target.build.processor.endian);
  }
  if (!ProcessPrecedence(endian)) {
    return false;
  }

  return true;
}

bool ProjMgrWorker::ProcessAccessSequences(ContextItem& context) {
  // Collect pointers to fields that accept access sequences
  vector<string*> fields;
  InsertVectorPointers(fields, context.defines);
  InsertVectorPointers(fields, context.includes);
  for (auto& misc : context.misc) {
    InsertVectorPointers(fields, misc.as);
    InsertVectorPointers(fields, misc.c);
    InsertVectorPointers(fields, misc.cpp);
    InsertVectorPointers(fields, misc.c_cpp);
    InsertVectorPointers(fields, misc.lib);
    InsertVectorPointers(fields, misc.link);
  }
  // Files
  InsertFilesPointers(fields, context.groups);

  // Iterate over fields and expand access sequences
  for (auto& item : fields) {
    if (item) {
      size_t offset = 0;
      while (offset != string::npos) {
        string sequence;
        if (!GetAccessSequence(offset, *item, sequence, '$', '$')) {
          return false;
        }
        if (offset != string::npos) {
          string replacement;
          regex regEx;
          if (sequence == "Dname") {
            regEx = regex("\\$Dname\\$");
            replacement = context.device;
          } else if (sequence == "Bname") {
            regEx = regex("\\$Bname\\$");
            replacement = context.board;
          } else if (regex_match(sequence, regex("(Output|OutDir|Source)\\(.*"))) {
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
            if (m_contexts.find(contextName) != m_contexts.end()) {
              error_code ec;
              const auto& depContext = m_contexts.at(contextName);
              const string& relSrcDir = fs::relative(depContext.directories.cprj, context.directories.cprj, ec).generic_string() + "/";
              const string& relOutDir = relSrcDir + depContext.directories.outdir;
              if (regex_match(sequence, regex("^OutDir\\(.*"))) {
                regEx = regex("\\$OutDir\\(.*\\$");
                replacement = relOutDir;
              } else if (regex_match(sequence, regex("^Output\\(.*"))) {
                regEx = regex("\\$Output\\(.*\\$");
                replacement = relOutDir + contextName;
              } else if (regex_match(sequence, regex("^Source\\(.*"))) {
                regEx = regex("\\$Source\\(.*\\$");
                replacement = relSrcDir;
              }
            } else {
              ProjMgrLogger::Error("context '" + contextName + "' referenced by access sequence '" + sequence + "' does not exist");
              return false;
            }
          } else {
            ProjMgrLogger::Warn("unknown access sequence: '" + sequence + "'");
          }
          *item = regex_replace(*item, regEx, replacement);
        }
      }
    }
  }
  return true;
}

bool ProjMgrWorker::ProcessGroups(ContextItem& context) {
  // Add cproject groups
  for (const auto& group : context.cproject->groups) {
    if (!AddGroup(group, context.groups, context, context.directories.cproject)) {
      return false;
    }
  }
  // Add clayers groups
  for (const auto& clayer : context.clayers) {
    for (const auto& group : clayer.second->groups) {
      const string& layerRoot = fs::path(clayer.first).parent_path().generic_string();
      if (!AddGroup(group, context.groups, context, layerRoot)) {
        return false;
      }
    }
  }
  return true;
}

bool ProjMgrWorker::AddGroup(const GroupNode& src, vector<GroupNode>& dst, ContextItem& context, const string root) {
  if (CheckType(src.type, context.type)) {
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
    dst.push_back({ src.group, files, groups, src.build, src.type });
  }
  return true;
}

bool ProjMgrWorker::AddFile(const FileNode& src, vector<FileNode>& dst, ContextItem& context, const string root) {
  if (CheckType(src.type, context.type)) {
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

bool ProjMgrWorker::AddComponent(const ComponentItem& src, vector<ComponentItem>& dst, TypePair type) {
  if (CheckType(src.type, type)) {
    for (auto& dstNode : dst) {
      if (dstNode.component == src.component) {
        ProjMgrLogger::Error("conflict: component '" + dstNode.component + "' is declared multiple times");
        return false;
      }
    }
    dst.push_back(src);
  }
  return true;
}

bool ProjMgrWorker::CheckType(TypeFilter typeFilter, TypePair type) {
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

bool ProjMgrWorker::ProcessContext(ContextItem& context, bool resolveDependencies) {
  context.outputType = context.cproject->outputType.empty() ? "exe" : context.cproject->outputType;

  if (!LoadPacks(context)) {
    return false;
  }
  if (!ProcessPrecedences(context)) {
    return false;
  }
  if (!ProcessToolchain(context)) {
    return false;
  }
  if (!ProcessDevice(context)) {
    return false;
  }
  if (!SetTargetAttributes(context, context.targetAttributes)) {
    return false;
  }
  if (!ProcessGroups(context)) {
    return false;
  }
  if (!ProcessAccessSequences(context)) {
    return false;
  }
  if (!ProcessComponents(context)) {
    return false;
  }
  if (!ProcessConfigFiles(context)) {
    return false;
  }
  if (resolveDependencies) {
    if (!ProcessDependencies(context)) {
      return false;
    }
    // TODO: Add uniquely identified missing dependencies to RTE Model

    if (!context.dependencies.empty()) {
      for (const auto& [component, dependencies] : context.dependencies) {
        string msg = "component '" + component + "' has unresolved dependencies:";
        for (const auto& dependency : dependencies) {
          msg += "\n  " + dependency;
        }
        ProjMgrLogger::Warn(msg);
      }
    }
  }
  return true;
}

void ProjMgrWorker::ApplyFilter(const set<string>& origin, const set<string>& filter, set<string>& result) {
  result.clear();
  for (const auto& word : filter) {
    if (word.empty()) {
      continue;
    }
    if (result.empty()) {
      for (const auto& item : origin) {
        if (item.find(word) != string::npos) {
          result.insert(item);
        }
      }
    } else {
      const auto temp = result;
      for (const auto& item : temp) {
        if (item.find(word) == string::npos) {
          result.erase(result.find(item));
        }
        if (result.empty()) {
          return;
        }
      }
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

bool ProjMgrWorker::ListPacks(vector<string>&packs, const string& contextName, const string& filter) {
  if (!contextName.empty()) {
    // Get selected context
    if (m_contexts.empty() || (m_contexts.find(contextName) == m_contexts.end())) {
      ProjMgrLogger::Error("context '" + contextName + "' was not found");
      return false;
    }
  }
  ContextItem& context = m_contexts[contextName];
  if (!LoadPacks(context)) {
    return false;
  }
  if (m_loadedPacks.empty()) {
    ProjMgrLogger::Error("no installed pack was found");
    return false;
  }
  set<string> packsSet;
  for (const auto& pack : m_loadedPacks) {
    packsSet.insert(ProjMgrUtils::GetPackageID(pack));
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
  return true;
}

bool ProjMgrWorker::ListBoards(vector<string>& boards, const string& contextName, const string& filter) {
  if (!contextName.empty()) {
    // Get selected context
    if (m_contexts.empty() || (m_contexts.find(contextName) == m_contexts.end())) {
      ProjMgrLogger::Error("context '" + contextName + "' was not found");
      return false;
    }
  }
  ContextItem& context = m_contexts[contextName];
  if (!LoadPacks(context)) {
    return false;
  }
  set<string> boardsSet;
  const RteBoardMap& availableBoards = context.rteFilteredModel->GetBoards();
  for (const auto& [_, board] : availableBoards) {
    boardsSet.insert(board->GetName());
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

bool ProjMgrWorker::ListDevices(vector<string>& devices, const string& contextName, const string& filter) {
  if (!contextName.empty()) {
    // Get selected context
    if (m_contexts.empty() || (m_contexts.find(contextName) == m_contexts.end())) {
      ProjMgrLogger::Error("context '" + contextName + "' was not found");
      return false;
    }
  }
  ContextItem& context = m_contexts[contextName];
  if (!LoadPacks(context)) {
    return false;
  }
  set<string> devicesSet;

  list<RteDevice*> filteredModelDevices;
  context.rteFilteredModel->GetDevices(filteredModelDevices, "", "", RteDeviceItem::VARIANT);
  for (const auto& deviceItem : filteredModelDevices) {
    const string& deviceName = deviceItem->GetFullDeviceName();
    if (deviceItem->GetProcessorCount() > 1) {
      const auto& processors = deviceItem->GetProcessors();
      for (const auto& processor : processors) {
        devicesSet.insert(deviceName + ":" + processor.first);
      }
    } else {
      devicesSet.insert(deviceName);
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

bool ProjMgrWorker::ListComponents(vector<string>& components, const string& contextName, const string& filter) {
  RteComponentMap installedComponents;
  if (!contextName.empty()) {
    // Get selected context
    if (m_contexts.empty() || (m_contexts.find(contextName) == m_contexts.end())) {
      ProjMgrLogger::Error("context '" + contextName + "' was not found");
      return false;
    }
  }
  ContextItem& context = m_contexts[contextName];
  if (!LoadPacks(context)) {
    return false;
  }
  if (!contextName.empty()) {
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
  installedComponents = context.rteActiveTarget->GetFilteredComponents();
  if (installedComponents.empty()) {
    if (!contextName.empty()) {
      ProjMgrLogger::Error("no component was found for device '" + context.device + "'");
    } else {
      ProjMgrLogger::Error("no installed component was found");
    }
    return false;
  }
  RteComponentMap componentMap;
  set<string> componentIds, filteredIds;
  for (auto& component : installedComponents) {
    const string& componentId = ProjMgrUtils::GetComponentID(component.second);
    componentIds.insert(componentId);
    componentMap[componentId] = component.second;
  }
  if (!filter.empty()) {
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

bool ProjMgrWorker::ListDependencies(vector<string>& dependencies, const string& contextName, const string& filter) {
  if (m_contexts.empty() || (m_contexts.find(contextName) == m_contexts.end())) {
    ProjMgrLogger::Error("context '" + contextName + "' was not found");
    return false;
  }
  ContextItem& context = m_contexts.at(contextName);
  if (!ProcessContext(context)) {
    return false;
  }
  if (!ProcessDependencies(context)) {
    return false;
  }
  set<string>dependenciesSet;
  for (const auto& [component, deps] : context.dependencies) {
    for (const auto& dep : deps) {
      dependenciesSet.insert(component + " " + dep);
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

bool ProjMgrWorker::ListGenerators(const string& context, vector<string>& generators) {
  if (m_contexts.find(context) == m_contexts.end()) {
    ProjMgrLogger::Error("context '" + context + "' was not found");
    return false;
  }
  if (!ProcessContext(m_contexts.at(context))) {
    return false;
  }
  for (const auto& [id, generator] : m_contexts.at(context).generators) {
    generators.push_back(id +" (" + generator->GetText() + ")");
  }
  return true;
}

bool ProjMgrWorker::GetTypeContent(ContextItem& context) {
  if (!context.type.build.empty() || !context.type.target.empty()) {
    string buildType, targetType;
    context.buildType = context.csolution->buildTypes[context.type.build];
    context.targetType = context.csolution->targetTypes[context.type.target];
  }
  return true;
}

void ProjMgrWorker::MergeMiscCPP(vector<MiscItem>& vec) {
  for (auto& vecIt : vec) {
    AddStringItemsUniquely(vecIt.c, vecIt.c_cpp);
    AddStringItemsUniquely(vecIt.cpp, vecIt.c_cpp);
  }
}

void ProjMgrWorker::AddMiscUniquely(vector<MiscItem>& dst, vector<vector<MiscItem>*>& srcVec) {
  for (auto& src : srcVec) {
    for (auto& srcIt : *src) {
      if (dst.empty()) {
        dst.push_back(srcIt);
        continue;
      }
      for (auto& dstIt : dst) {
        if (dstIt.compiler.empty() || srcIt.compiler.empty() || (dstIt.compiler == srcIt.compiler)) {
          AddStringItemsUniquely(dstIt.as, srcIt.as);
          AddStringItemsUniquely(dstIt.c, srcIt.c);
          AddStringItemsUniquely(dstIt.cpp, srcIt.cpp);
          AddStringItemsUniquely(dstIt.c_cpp, srcIt.c_cpp);
          AddStringItemsUniquely(dstIt.link, srcIt.link);
          AddStringItemsUniquely(dstIt.lib, srcIt.lib);
        }
        else {
          dst.push_back(srcIt);
        }
      }
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

bool ProjMgrWorker::GetAccessSequence(size_t& offset, string& src, string& sequence, const char start, const char end) {
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

void ProjMgrWorker::InsertVectorPointers(vector<string*>& dst, vector<string>& src) {
  for (auto& item : src) {
    dst.push_back(&item);
  }
}

void ProjMgrWorker::InsertFilesPointers(vector<string*>& dst, vector<GroupNode>& groups) {
    for (auto& groupNode : groups) {
      for (auto& fileNode : groupNode.files) {
        dst.push_back(&(fileNode.file));
      }
      InsertFilesPointers(dst, groupNode.groups);
    }
}

void ProjMgrWorker::PushBackUniquely(vector<string>& vec, const string& value) {
  if (find(vec.cbegin(), vec.cend(), value) == vec.cend()) {
    vec.push_back(value);
  }
}

bool ProjMgrWorker::CopyContextFiles(ContextItem& context, const string& outputDir, bool outputEmpty) {
  if (!outputEmpty) {
    // Copy RTE folder content
    error_code ec;
    const string& cprojectDir = fs::absolute(context.directories.cproject, ec).generic_string();
    vector<string> rteDirs;
    static constexpr const char* RTE_FOLDER = "/RTE";
    rteDirs.push_back(cprojectDir + RTE_FOLDER);
    for (auto const& clayer : context.clayers) {
      const string& clayerDir = fs::path(clayer.first).parent_path().generic_string();
      rteDirs.push_back(clayerDir + RTE_FOLDER);
    }
    for (auto const& rteDir : rteDirs) {
      if (RteFsUtils::Exists(rteDir)) {
        RteFsUtils::CreateDirectories(outputDir + RTE_FOLDER);
        fs::copy(rteDir, outputDir + RTE_FOLDER, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
      }
    }
    // Copy files described in cproject and clayers
    if (!context.filePaths.empty()) {
      for (auto const& filePath : context.filePaths) {
        const string& output = fs::path(outputDir + "/" + filePath.first).parent_path().generic_string();
        if (RteFsUtils::Exists(filePath.second)) {
          if (!RteFsUtils::Exists(output)) {
            RteFsUtils::CreateDirectories(output);
          }
          fs::copy(filePath.second, output, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
        }
      }
    }
  } else if (!context.clayers.empty()) {
    ProjMgrLogger::Warn("output option was not specified, files won't be copied for context '" + context.name + "'");
  }
  return true;
}

bool ProjMgrWorker::ExecuteGenerator(const std::string& context, std::string& generatorId) {
  if (m_contexts.find(context) == m_contexts.end()) {
    ProjMgrLogger::Error("context '" + context + "' was not found");
    return false;
  }
  if (!ProcessContext(m_contexts.at(context))) {
    return false;
  }
  const auto& generators = m_contexts.at(context).generators;
  if (generators.find(generatorId) == generators.end()) {
    ProjMgrLogger::Error("generator '" + generatorId + "' was not found");
    return false;
  }
  RteGenerator* generator = generators.at(generatorId);
  // TODO: review RteGenerator::GetExpandedCommandLine and variables
  //const string generatorCommand = m_kernel->GetCmsisPackRoot() + "/" + generator->GetPackagePath() + generator->GetCommand();
  const string generatorCommand = generator->GetExpandedCommandLine(m_contexts.at(context).rteActiveTarget);
  if (generatorCommand.empty()) {
    ProjMgrLogger::Error("generator command for'" + generatorId + "' was not found");
    return false;
  }
  const string generatorWorkingDir = generator->GetExpandedWorkingDir(m_contexts.at(context).rteActiveTarget);

  // Create generate.yml file with context info and destination
  ProjMgrYamlEmitter::EmitContextInfo(m_contexts.at(context), generatorWorkingDir);

  error_code ec;
  const auto& workingDir = fs::current_path(ec);
  fs::current_path(generatorWorkingDir, ec);
  ProjMgrUtils::Result result = ProjMgrUtils::ExecCommand(generatorCommand);
  fs::current_path(workingDir, ec);

  ProjMgrLogger::Info("generator '" + generatorId + "' reported:\n" + result.first);

  if (result.second) {
    ProjMgrLogger::Error("executing generator '" + generatorId + "' for context '" + context + "' failed");
    return false;
  }

  return true;
}

std::string ProjMgrWorker::GetDeviceInfoString(const std::string& vendor,
  const std::string& name, const std::string& processor) const {
  return vendor + (vendor.empty() ? "" : "::") +
    name + (processor.empty() ? "" : ":" + processor);
}
