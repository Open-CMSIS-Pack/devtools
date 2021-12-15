/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgrWorker.h"

#include "CrossPlatformUtils.h"
#include "RteFsUtils.h"
#include "RteItem.h"

#include <algorithm>
#include <iostream>

using namespace std;

ProjMgrWorker::ProjMgrWorker(void) {
  // Reserved
}

ProjMgrWorker::~ProjMgrWorker(void) {
  // Reserved
}

bool ProjMgrWorker::LoadPacks(void) {
  string packRoot = CrossPlatformUtils::GetEnv("CMSIS_PACK_ROOT");
  if (packRoot.empty()) {
    packRoot = CrossPlatformUtils::GetDefaultCMSISPackRootDir();
  }
  m_kernel = ProjMgrKernel::Get();
  m_kernel->SetCmsisPackRoot(packRoot);

  // TODO: Handle subset of package requirements

  // Get installed packs
  if (!m_kernel->GetInstalledPacks(m_installedPacks)) {
    cerr << "projmgr error: parsing installed packs failed!" << endl;
  }
  return CheckRteErrors();
}

bool ProjMgrWorker::CheckRteErrors(void) {
  const list<string>& rteErrorMessages = m_kernel->GetCallback()->GetErrorMessages();
  if (!rteErrorMessages.empty()) {
    for (const auto& rteErrorMessage : rteErrorMessages) {
      cerr << rteErrorMessage << endl;
    }
    return false;
  }
  return true;
}

bool ProjMgrWorker::SetTargetAttributes(ProjMgrProjectItem& project, map<string, string>& attributes) {
  if (project.rteActiveProject == nullptr) {
    m_model = m_kernel->GetGlobalModel();
    // RteGlobalModel has the RteProject pointer ownership
    RteProject* rteProject = make_unique<RteProject>().release();
    m_model->AddProject(0, rteProject);
    m_model->SetActiveProjectId(rteProject->GetProjectId());
    project.rteActiveProject = m_model->GetActiveProject();
    project.rteActiveProject->AddTarget("CMSIS", attributes, true, true);
    project.rteActiveProject->SetActiveTarget("CMSIS");
    project.rteActiveTarget = project.rteActiveProject->GetActiveTarget();
  }
  else {
    project.rteActiveTarget->SetAttributes(attributes);
    project.rteActiveTarget->UpdateFilterModel();
  }
  return CheckRteErrors();
}

bool ProjMgrWorker::ProcessDevice(ProjMgrProjectItem& project) {
  if (project.cproject.target.device.empty()) {
    cerr << "projmgr error: missing device requirement" << endl;
    return false;
  }

  size_t deviceDelimiter = project.cproject.target.device.find("::");
  size_t processorDelimiter = project.cproject.target.device.find(':');
  string deviceVendor, deviceName, processorName;
  if (deviceDelimiter != string::npos) {
    deviceVendor = project.cproject.target.device.substr(0, deviceDelimiter);
    deviceName = project.cproject.target.device.substr(deviceDelimiter + 2, processorDelimiter);
  }
  else {
    deviceName = project.cproject.target.device.substr(0, processorDelimiter);
  }
  if (processorDelimiter != string::npos) {
    processorName = project.cproject.target.device.substr(processorDelimiter + 1, string::npos);
  }

  list<RteDeviceItem*> devices;
  for (const auto& pack : m_installedPacks) {
    list<RteDeviceItem*> deviceItems;
    pack->GetEffectiveDeviceItems(deviceItems);
    devices.insert(devices.end(), deviceItems.begin(), deviceItems.end());
  }
  list<RteDeviceItem*> matchedDevices;
  for (const auto& device : devices) {
    if (device->GetDeviceName() == deviceName) {
      if (deviceVendor.empty() || (deviceVendor == DeviceVendor::GetCanonicalVendorName(device->GetEffectiveAttribute("Dvendor")))) {
        matchedDevices.push_back(device);
      }
    }
  }
  RteDeviceItem* matchedDevice = nullptr;
  for (const auto& item : matchedDevices) {
    if ((!matchedDevice) || (VersionCmp::Compare(matchedDevice->GetPackage()->GetVersionString(), item->GetPackage()->GetVersionString()) < 0)) {
      matchedDevice = item;
    }
  }
  if (!matchedDevice) {
    cerr << "projmgr error: device '" << deviceName << "' was not found" << endl;
    return false;
  }

  const auto& processor = matchedDevice->GetProcessor(processorName);
  if (!processor) {
    cerr << "projmgr error: processor name '" << processorName << "' was not found" << endl;
    return false;
  }

  const auto& processorAttributes = processor->GetAttributes();
  project.targetAttributes.insert(processorAttributes.begin(), processorAttributes.end());
  project.targetAttributes["Dvendor"] = matchedDevice->GetEffectiveAttribute("Dvendor");
  project.targetAttributes["Dname"] = deviceName;
  if (!project.cproject.target.processor.endian.empty()) {
    project.targetAttributes["Dendian"] = project.cproject.target.processor.endian;
  }
  if (!project.cproject.target.processor.fpu.empty()) {
    project.targetAttributes["Dfpu"] = project.cproject.target.processor.fpu;
  }
  if (!project.cproject.target.processor.mpu.empty()) {
    project.targetAttributes["Dmpu"] = project.cproject.target.processor.mpu;
  }
  if (!project.cproject.target.processor.dsp.empty()) {
    project.targetAttributes["Ddsp"] = project.cproject.target.processor.dsp;
  }
  if (!project.cproject.target.processor.mve.empty()) {
    project.targetAttributes["Dmve"] = project.cproject.target.processor.mve;
  }
  if (!project.cproject.target.processor.trustzone.empty()) {
    project.targetAttributes["Dtz"] = project.cproject.target.processor.trustzone;
  }
  if (!project.cproject.target.processor.secure.empty()) {
    project.targetAttributes["Dsecure"] = project.cproject.target.processor.secure;
  }

  project.packages.insert({ matchedDevice->GetPackage()->GetPackageID(), matchedDevice->GetPackage() });

  return true;
}

bool ProjMgrWorker::ProcessPackages(ProjMgrProjectItem& project) {
  for (const auto& packageEntry : project.cproject.packages) {
    const size_t nameDelimiter = packageEntry.find("::");
    const size_t versionDelimiter = packageEntry.find('@');
    if (nameDelimiter == string::npos) {
      cerr << "projmgr error: missing package delimiter" << endl;
      return false;
    }
    PackageItem package;
    package.vendor = packageEntry.substr(0, nameDelimiter);
    if (versionDelimiter != string::npos) {
      package.name = packageEntry.substr(nameDelimiter + 2, versionDelimiter - nameDelimiter -2);
    } else {
      package.name = packageEntry.substr(nameDelimiter + 2, string::npos);
    }
    package.version = versionDelimiter == string::npos ? "0.0.0" : packageEntry.substr(versionDelimiter + 1, string::npos);
    project.packRequirements.push_back(package);
  }
  return true;
}

bool ProjMgrWorker::ProcessToolchain(ProjMgrProjectItem& project) {
  if (project.cproject.toolchain.empty()) {
    cerr << "projmgr error: missing toolchain requirement" << endl;
    return false;
  }
  const size_t delimiter = project.cproject.toolchain.find('@');
  project.toolchain.name = project.cproject.toolchain.substr(0, delimiter);
  project.toolchain.version = delimiter == string::npos ? "0.0.0" : project.cproject.toolchain.substr(delimiter + 1, string::npos);

  if (project.toolchain.name == "AC6" || project.toolchain.name == "AC5") {
    project.targetAttributes["Tcompiler"] = "ARMCC";
    project.targetAttributes["Toptions"] = project.toolchain.name;
  } else {
    project.targetAttributes["Tcompiler"] = project.toolchain.name;
  }
  return true;
}

bool ProjMgrWorker::ProcessComponents(ProjMgrProjectItem& project) {
  if (!project.rteActiveTarget) {
    cerr << "projmgr error: missing RTE target" << endl;
    return false;
  }
  RteComponentMap componentMap = project.rteActiveTarget->GetFilteredComponents();
  set<string> componentIds, filteredIds;
  for (const auto& component : componentMap) {
    componentIds.insert(component.first);
  }
  for (const auto& item : project.cproject.components) {
    // Filter components by filter words
    RteComponentMap filteredComponents;
    if (!item.component.empty()) {
      set<string> filteredIds;
      ApplyFilter(componentIds, SplitArgs(item.component), filteredIds);
      for (const auto& filteredId : filteredIds) {
        filteredComponents[filteredId] = componentMap[filteredId];
      }
    }
    // Evaluate filtered components
    if (filteredComponents.size() == 1) {
      // Single match
      const auto& matchedComponent = filteredComponents.begin()->second;
      project.components.insert({ matchedComponent->GetComponentUniqueID(true), matchedComponent });
      project.packages.insert({ matchedComponent->GetPackage()->GetPackageID(), matchedComponent->GetPackage() });
    } else if (filteredComponents.empty()) {
      // No match
      cerr << "projmgr error: no component was found with identifier '" << item.component << "'" << endl;
      return false;
    } else {
      // Multiple matches
      cerr << "projmgr error: multiple components were found for identifier'" << item.component << "'" << endl;
      for (const auto& component : filteredComponents) {
        cerr << component.first << endl;
      }
      return false;
    }
  }

  // Add components into RTE
  set<RteComponentInstance*> unresolvedComponents;
  list<RteItem*> selItems;
  for (const auto& component : project.components) {
    selItems.push_back(component.second);
  }
  project.rteActiveProject->AddCprjComponents(selItems, project.rteActiveTarget, unresolvedComponents);
  if (!unresolvedComponents.empty()) {
    cerr << "projmgr error: unresolved components:" << endl;
    for (const auto& component : unresolvedComponents) {
      cerr << component->GetComponentUniqueID(true) << endl;
    }
    return false;
  }

  return CheckRteErrors();
}

bool ProjMgrWorker::ProcessDependencies(ProjMgrProjectItem & project) {
  project.rteActiveProject->ResolveDependencies(project.rteActiveTarget);
  const auto& selected = project.rteActiveTarget->GetSelectedComponentAggregates();
  for (const auto& component : selected) {
    RteComponentContainer* c = component.first;
    string componentAggregate = c->GetComponentAggregateID();
    const auto& match = find_if(project.components.begin(), project.components.end(),
      [componentAggregate](auto component) {
        return component.second->GetComponentAggregateID() == componentAggregate;
      });
    if (match == project.components.end()) {
      project.dependencies.insert({ c->GetComponentAggregateID(), c });
    }
  }
  if (selected.size() != (project.components.size() + project.dependencies.size())) {
    cerr << "projmgr error: resolving dependencies failed" << endl;
    return false;
  }
  return CheckRteErrors();
}

bool ProjMgrWorker::ProcessProject(ProjMgrProjectItem& project, bool resolveDependencies) {
  project.name = project.cproject.name;
  project.description = project.cproject.description;
  project.outputType = project.cproject.outputType.empty() ? "exe" : project.cproject.outputType;
  project.groups = project.cproject.groups;

  if (!ProcessPackages(project)) {
    return false;
  }
  if (!LoadPacks()) {
    return false;
  }
  if (!ProcessToolchain(project)) {
    return false;
  }
  if (!ProcessDevice(project)) {
    return false;
  }
  if (!SetTargetAttributes(project, project.targetAttributes)) {
    return false;
  }
  if (!ProcessComponents(project)) {
    return false;
  }
  if (!ProcessDependencies(project)) {
    return false;
  }
  if (resolveDependencies) {

    // TODO: Add uniquely identified missing dependencies to RTE Model

    if (!project.dependencies.empty()) {
      cerr << "projmgr error: missing dependencies:" << endl;
      for (const auto& dependency : project.dependencies) {
        cerr << dependency.first << endl;
      }
      return false;
    }
  }
  return true;
}

void ProjMgrWorker::ApplyFilter(const set<string>& origin, const set<string>& filter, set<string>& result) {
  result.clear();
  for (const auto& word : filter) {
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

set<string> ProjMgrWorker::SplitArgs(const string& args) {
  set<string> s;
  size_t end = 0, start = 0, len = args.length();
  while (end < len) {
    if ((end = args.find(" ", start)) == string::npos) end = len;
    s.insert(args.substr(start, end - start));
    start = end + 1;
  }
  return s;
}

bool ProjMgrWorker::ListPacks(const string & filter, set<string>&packs) {
  if (!LoadPacks()) {
    return false;
  }
  if (m_installedPacks.empty()) {
    cerr << "projmgr error: no installed pack was found" << endl;
    return false;
  }
  for (const auto& pack : m_installedPacks) {
    packs.insert(pack->GetPackageID());
  }
  if (!filter.empty()) {
    set<string> filteredPacks;
    ApplyFilter(packs, SplitArgs(filter), filteredPacks);
    if (filteredPacks.empty()) {
      cerr << "projmgr error: no pack was found with filter '" << filter << "'" << endl;
      return false;
    }
    packs = filteredPacks;
  }
  return true;
}

bool ProjMgrWorker::ListDevices(ProjMgrProjectItem & project, const string & filter, set<string>& devices) {
  if (!project.cproject.packages.empty()) {
    if (!ProcessPackages(project)) {
      return false;
    }
  }
  if (!LoadPacks()) {
    return false;
  }
  for (const auto& pack : m_installedPacks) {
    list<RteDeviceItem*> deviceItems;
    pack->GetEffectiveDeviceItems(deviceItems);
    for (const auto& deviceItem : deviceItems) {
      devices.insert(deviceItem->GetFullDeviceName());
    }
  }
  if (devices.empty()) {
    cerr << "projmgr error: no installed device was found" << endl;
    return false;
  }
  if (!filter.empty()) {
    set<string> matchedDevices;
    ApplyFilter(devices, SplitArgs(filter), matchedDevices);
    if (matchedDevices.empty()) {
      cerr << "projmgr error: no device was found with filter '" << filter << "'" << endl;
      return false;
    }
    devices = matchedDevices;
  }
  return true;
}

bool ProjMgrWorker::ListComponents(ProjMgrProjectItem & project, const string & filter, set<string>& components) {
  if (!project.cproject.packages.empty()) {
    if (!ProcessPackages(project)) {
      return false;
    }
  }
  if (!LoadPacks()) {
    return false;
  }
  if (!project.cproject.target.device.empty()) {
    if (!ProcessToolchain(project)) {
      return false;
    }
    if (!ProcessDevice(project)) {
      return false;
    }
    if (!SetTargetAttributes(project, project.targetAttributes)) {
      return false;
    }
    RteComponentMap componentMap = project.rteActiveTarget->GetFilteredComponents();
    for (const auto& component : componentMap) {
      components.insert(component.second->GetComponentUniqueID(true));
    }
    if (components.empty()) {
      cerr << "projmgr error: no filtered component was found for device '" << project.cproject.target.device << "'" << endl;
      return false;
    }
  } else {
    for (const auto& pack : m_installedPacks) {
      if (pack->GetComponents()) {
        const auto& packComponents = pack->GetComponents()->GetChildren();
        for (const auto& packComponent : packComponents) {
          components.insert(packComponent->GetComponentUniqueID(true));
        }
      }
    }
    if (components.empty()) {
      cerr << "projmgr error: no installed component was found" << endl;
      return false;
    }
  }
  if (!filter.empty()) {
    set<string> filteredComponents;
    ApplyFilter(components, SplitArgs(filter), filteredComponents);
    if (filteredComponents.empty()) {
      cerr << "projmgr error: no component was found with filter '" << filter << "'" << endl;
      return false;
    }
    components = filteredComponents;
  }
  return true;
}

bool ProjMgrWorker::ListDependencies(ProjMgrProjectItem& project, const string& filter, set<string>& dependencies) {
  if (!ProcessProject(project)) {
    return false;
  }
  for (const auto& dependency : project.dependencies) {
    dependencies.insert(dependency.first);
  }
  if (!filter.empty()) {
    set<string> filteredDependencies;
    ApplyFilter(dependencies, SplitArgs(filter), filteredDependencies);
    if (filteredDependencies.empty()) {
      cerr << "projmgr error: no unresolved dependency was found with filter '" << filter << "'" << endl;
      return false;
    }
    dependencies = filteredDependencies;
  }
  return true;
}
