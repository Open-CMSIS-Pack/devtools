/*
 * Copyright (c) 2025 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgrRpcServerData.h"

#include "RteComponent.h"
#include "RteInstance.h"
#include "RtePackage.h"
#include "RteProject.h"
#include "RteTarget.h"
#include "RteModel.h"
#include "RteConstants.h"

#include "CollectionUtils.h"

#include <algorithm>
using namespace std;

using namespace RpcArgs;

template <typename T>
T FromRteItem(const string& id, RteItem* rteItem) {
  T e;
  e.id = id;
  const auto& description = rteItem->GetDescription();
  if(!description.empty()) {
    e.description = description;
  }
  const auto& doc = rteItem->GetDocFile();
  if(!doc.empty()) {
    e.doc = doc;
  }
  return e;
}

RpcDataCollector::RpcDataCollector(RteTarget* t, RteModel* m) :
  m_target(t),
  m_model(t? t->GetFilteredModel() : m)
{
}

RpcArgs::Component RpcDataCollector::FromRteComponent(const RteComponent* rteComponent) const{
    RpcArgs::Component c;
    c.id = rteComponent->GetComponentID(true);
    c.pack = rteComponent->GetPackageID(true);
    const auto& description = rteComponent->GetDescription();
    if (!description.empty()) {
      c.description = description;
    }
    const auto& doc = rteComponent->GetDocFile();
    if (!doc.empty()) {
      c.doc = doc;
    }
    const auto& api = rteComponent->GetApi(m_target, true);
    if (api) {
      c.implements = api->ConstructComponentID(true);
    }
    if (rteComponent->HasMaxInstances()) {
      c.maxInstances = rteComponent->GetMaxInstances();
    }
    return c;
}

std::optional<RpcArgs::Options> RpcDataCollector::OptionsFromRteItem(const RteItem* item) const {
    if (!item) {
        return std::nullopt;
    }
    Options opt;
    bool bHasOpt = false;

    if (item->HasAttribute("layer")) {
        opt.layer = item->GetAttribute("layer");
        bHasOpt = true;
    }
    if (item->HasAttribute("explicitVersion")) {
        opt.explicitVersion = item->GetAttribute("explicitVersion");
        bHasOpt = true;
    }
    if (item->GetAttributeAsBool("explicitVendor")) {
        opt.explicitVendor = true;
        bHasOpt = true;
    }
    if (bHasOpt) {
        return opt;
    }
    return std::nullopt;
}

std::string RpcDataCollector::ResultStringFromRteItem(const RteItem* item) const {
  string result;
  RteTarget* rteTarget = GetTarget();
  if(item && rteTarget) {
    RteItem::ConditionResult res = item->GetConditionResult(rteTarget->GetDependencySolver());
    if(res > RteItem::ConditionResult::UNDEFINED && res < RteItem::ConditionResult::FULFILLED) {
      result = RteItem::ConditionResultToString(res);
    }
  }
  return result;
}

RpcArgs::ComponentInstance RpcDataCollector::FromComponentInstance(const RteComponentInstance* rteCi) const {
  ComponentInstance ci;
  if(rteCi) {
    ci.id = rteCi->GetAttribute("ymlID");
    ci.selectedCount = rteCi->GetInstanceCount(m_target->GetName());
    auto rteComponent = rteCi->GetResolvedComponent(m_target->GetName());
    if(rteComponent) {
      ci.resolvedComponent = FromRteComponent(rteComponent);
    }

    auto& gen = rteCi->GetAttribute("generator");
    if(!gen.empty()) {
      ci.generator = gen;
    }
    if(rteCi->GetAttributeAsBool("generated") && !rteCi->GetAttributeAsBool("selectable")) {
      ci.fixed = true;
    }
    auto opt = OptionsFromRteItem(rteCi);
    if(opt) {
      ci.options = opt;
    }
  }
  return ci;
}


RteItem* RpcDataCollector::GetTaxonomyItem(const RteComponentGroup* rteGroup) const {
  if(m_target && rteGroup) {
    auto taxonimyId = rteGroup->GetTaxonomyDescriptionID();
    return m_target->GetFilteredModel()->GetTaxonomyItem(taxonimyId);
  }
  return nullptr;
}

void RpcDataCollector::CollectUsedComponents(vector< RpcArgs::ComponentInstance>& usedComponents) const {
  auto rteProject = m_target ? m_target->GetProject() : nullptr;
  if(!rteProject) {
    return;
  }
  for(auto [_id, rteCi] : rteProject->GetComponentInstances()) {
    if(!rteCi->IsApi()) {
      usedComponents.push_back(FromComponentInstance(rteCi));
    }
  }
}
std::set<std::string> RpcDataCollector::GetUsedPacks() const {
  RtePackageMap usedPacks;
  auto rteProject = m_target ? m_target->GetProject() : nullptr;
  if(rteProject) {
    rteProject->GetUsedPacks(usedPacks, m_target->GetName());
  }
  return key_set(usedPacks);
}

RpcArgs::Device RpcDataCollector::FromRteDevice( RteDeviceItem* rteDevice, bool bIncludeProperties) const {
  RpcArgs::Device d;
  d.id = rteDevice->GetVendorName() + RteConstants::SUFFIX_CVENDOR + rteDevice->GetName();
  d.family = rteDevice->GetEffectiveAttribute("Dfamily");
  d.subFamily = rteDevice->GetEffectiveAttribute("DsubFamily");
  d.pack = rteDevice->GetPackageID();
  if(bIncludeProperties) {
    string description;
    for(auto descr : rteDevice->GetAllEffectiveProperties("description")) {
      description += descr->GetText() + "\n";
    }
    if(!description.empty()) {
      d.description = description;
    }
    vector<RpcArgs::Processor> processors;
    for(auto [pname, rteProc] : rteDevice->GetProcessors()) {
      RpcArgs::Processor p;
      p.name = pname;
      p.core = rteProc->GetAttribute("Dcore");
      p.attributes = rteProc->GetAttributes();
      processors.push_back(p);
    }
    d.processors = processors;
    auto mems = rteDevice->GetAllEffectiveProperties("memory");
    vector<RpcArgs::Memory> memories;
    for(auto rteMem : rteDevice->GetAllEffectiveProperties("memory")) {
      RpcArgs::Memory m;
      m.name = rteMem->GetName();
      m.size = rteMem->GetAttribute("size");
      m.access = rteMem->GetAccessPermissions();
      memories.push_back(m);
    }
    d.memories = memories;
  }
  return d;
}

RpcArgs::Board RpcDataCollector::FromRteBoard( RteBoard* rteBoard, bool bIncludeProperties) const {
  RpcArgs::Board b;
  b.id = rteBoard->GetVendorString() + "::" + rteBoard->GetName();
  const string& rev = rteBoard->GetRevision();
  if(!rev.empty()) {
    b.id += ":" + rev;
  }
  b.pack = rteBoard->GetPackageID();
  if(bIncludeProperties) {
    b.description = rteBoard->GetDescription();
    auto imageItem = rteBoard->GetFirstChild("image");
    if(imageItem) {
      auto imageFile = imageItem->GetAttribute("small");
      if(imageFile.empty()) {
        imageFile = imageItem->GetAttribute("large");
      }
      if(!imageFile.empty()) {
        b.image = rteBoard->GetOriginalAbsolutePath(imageFile);
      }
    }
    Collection<RteItem*> mems;
    vector<RpcArgs::Memory> memories;
    for(auto rteMem : rteBoard->GetMemories(mems)) {
      RpcArgs::Memory m;
      m.name = rteMem->GetName();
      m.size = rteMem->GetAttribute("size");
      m.access = rteMem->GetAccessPermissions();
      memories.push_back(m);
    }
    if(!memories.empty()) {
      b.memories = memories;
    }
    auto debugProbe = rteBoard->GetItemByTag("debugProbe");
    if (debugProbe) {
      RpcArgs::Debugger d;
      d.name = debugProbe->GetName();
      d.protocol = debugProbe->GetAttribute("debugLink");
      d.clock = debugProbe->GetAttributeAsULL("debugClock");
      b.debugger = d;
    }
    vector<RpcArgs::Device> devices;
    list<RteDevice*> processedDevices;
    CollectBoardDevices(devices, rteBoard, true, processedDevices);
    if(!devices.empty()) {
      b.devices = devices;
    }
  }
  return b;
}

void RpcDataCollector::CollectBoardDevices(vector<RpcArgs::Device>& boardDevices, RteBoard* rteBoard,
  bool bMounted, list<RteDevice*>& processedDevices) const {
  list<RteItem*> refDevices;
  rteBoard->GetDevices(refDevices, !bMounted, bMounted);
  for(auto rteItem : refDevices) {
    RteDevice* rteDevice = m_model->GetDevice(rteItem->GetDeviceName(), rteItem->GetDeviceVendor());
    if(rteDevice && find(processedDevices.begin(), processedDevices.end(), rteDevice) == processedDevices.end()) {
      processedDevices.push_back(rteDevice);
      boardDevices.push_back(FromRteDevice(rteDevice, bMounted));
    }
  }
}

void RpcDataCollector::CollectDeviceList(RpcArgs::DeviceList& deviceList, const std::string& namePattern, const std::string& vendor) const {
  list<RteDevice*> devices;
  if(m_model) {
    m_model->GetDevices(devices, namePattern, vendor, RteDeviceItem::VARIANT);
  }
  for(auto rteDevice : devices) {
    if (!rteDevice->GetDeviceItems().empty()) {
      // skip not end-leaf item
      continue;
    }
    deviceList.devices.push_back(FromRteDevice(rteDevice, false));
  }
}

void RpcDataCollector::CollectDeviceInfo(RpcArgs::DeviceInfo& deviceInfo, const std::string& id) const {
  string name = RteUtils::StripPrefix(id, RteConstants::SUFFIX_CVENDOR);
  string vendor = RteUtils::ExtractPrefix(id, RteConstants::SUFFIX_CVENDOR);
  if(name.empty()) {
    deviceInfo.success = false;
    deviceInfo.message = "Invalid device ID: '" + id + "'";
    return;
  }

  RteDevice* rteDevice = m_model? m_model->GetDevice(name, vendor) : nullptr;
  if(rteDevice) {
    deviceInfo.device = FromRteDevice(rteDevice, true);
    deviceInfo.success = true;
  } else {
    deviceInfo.success = false;
    deviceInfo.message = "Device '" + id + "' not found";
    return;
  }
  deviceInfo.device = FromRteDevice(rteDevice, true);
}

void RpcDataCollector::CollectBoardList(RpcArgs::BoardList& boardList, const std::string& namePattern, const std::string& vendor) const {
  if(!m_model) {
    return;
  }
  auto& boards = m_model->GetBoards();
  for(auto [name, rteBoard] : boards) {
    if(vendor.empty() || rteBoard->GetVendorString() == vendor) {
      if(namePattern.empty() || WildCards::MatchToPattern(name, namePattern)) {
        boardList.boards.push_back(FromRteBoard(rteBoard, false));
      }
    }
  }
}

void RpcDataCollector::CollectBoardInfo(RpcArgs::BoardInfo& boardInfo, const std::string& id) const {
  string displayName = RteUtils::StripPrefix(id, RteConstants::SUFFIX_CVENDOR);
  string vendor = RteUtils::ExtractPrefix(id, RteConstants::SUFFIX_CVENDOR);
  string name = RteUtils::GetPrefix(displayName, ':');
  string revision = RteUtils::GetSuffix(displayName, ':');
  if(!revision.empty()) {
    displayName = name + " (" + revision + ")";
  }

  if(name.empty()) {
    boardInfo.success = false;
    boardInfo.message = "Invalid board ID: '" + id + "'";
    return;
  }

  RteBoard* rteBoard = m_model? m_model->FindBoard(displayName) : nullptr;
  if(rteBoard) {
    boardInfo.board = FromRteBoard(rteBoard, true);
    boardInfo.success = true;
  } else {
    boardInfo.success = false;
    boardInfo.message = "Board '" + id + "' not found";
    return;
  }
  boardInfo.board = FromRteBoard(rteBoard, true);
}


void RpcDataCollector::CollectCtClasses(RpcArgs::CtRoot& root) const {

  auto classContainer = m_target ? m_target->GetClasses() : nullptr;
  if(!classContainer) {
    return; // can happen if no solution is loaded
  }

  for(auto [name, rteClass] : classContainer->GetGroups()) {
    RpcArgs::CtClass ctClass;
    ctClass.name = name;
    auto activeBundle = rteClass->GetSelectedBundleName();
    ctClass.activeBundle = activeBundle;
    auto taxonomyItem = GetTaxonomyItem(rteClass);
    if(taxonomyItem) {
      ctClass.taxonomy = FromRteItem<Taxonomy>(taxonomyItem->GetTaxonomyDescriptionID(), taxonomyItem);
    }
    auto res = ResultStringFromRteItem(rteClass);
    if(!res.empty()) {
      ctClass.result = res;
    }
    CollectCtBundles(ctClass, rteClass);
    root.classes.push_back(ctClass);
  }
}

void RpcDataCollector::CollectCtBundles(RpcArgs::CtClass& ctClass, RteComponentGroup* rteClass) const {
  for(auto [bundleName, bundleId] : rteClass->GetBundleNames()) {
    m_target->GetFilteredBundles();
    CtBundle ctBundle;
    ctBundle.name = bundleName;
    RteBundle* rteBundle = get_or_null(GetTarget()->GetFilteredBundles(), bundleId);
    if(rteBundle) {
      ctBundle.bundle = FromRteItem<Bundle>(bundleName, rteBundle);
    }
    CollectCtChildren(ctBundle, rteClass, bundleName);
    // add to parent collection
    ctClass.bundles.push_back(ctBundle);
  }
}

void RpcDataCollector::CollectCtChildren(RpcArgs::CtTreeItem& parent, RteComponentGroup* parentRteGroup, const string& bundleName) const
{
  // collect aggregates to this level
  CollectCtAggregates(parent, parentRteGroup, bundleName);
  auto& rteGroups = parentRteGroup->GetGroups();
  if(rteGroups.empty()) {
    return;
  }
  vector<CtGroup> cgroups;
  for(auto [name, rteGroup] : rteGroups) {
    if(!rteGroup->HasBundleName(bundleName)) {
      continue;
    }
    CtGroup g;
    g.name = name;
    auto rteApi = rteGroup->GetApi();
    if(rteApi) {
      g.api = FromRteItem<Api>(rteApi->GetID(), rteApi);
    }
    auto taxonomyItem = GetTaxonomyItem(rteGroup);
    if(taxonomyItem) {
      g.taxonomy = FromRteItem<Taxonomy>(taxonomyItem->GetTaxonomyDescriptionID(), taxonomyItem);
    }
    auto res = ResultStringFromRteItem(rteGroup);
    if(!res.empty()) {
      g.result = res;
    }
    // subgroups
    CollectCtChildren(g, rteGroup, bundleName);
    CollectCtAggregates(g, rteGroup,bundleName);
    // add to parent collection
    cgroups.push_back(g);
  }
  parent.cgroups = cgroups;
}

void RpcDataCollector::CollectCtAggregates(RpcArgs::CtTreeItem& parent, RteComponentGroup* parentRteGroup, const string& bundleName) const
{
  // aggregates
  vector<CtAggregate> aggregates;
  for(auto child : parentRteGroup->GetChildren()) {
    RteComponentAggregate* rteAggregate = dynamic_cast<RteComponentAggregate*>(child);
    if(!rteAggregate || rteAggregate->GetCbundleName() != bundleName ) {
      continue;
    }
    CtAggregate a;
    a.name = rteAggregate->GetDisplayName();
    a.id = rteAggregate->GetID();
    a.activeVersion = rteAggregate->GetEffectiveVersion();
    auto& activeVariant = rteAggregate->GetSelectedVariant();
    if(!activeVariant.empty()) {
      a.activeVariant = rteAggregate->GetSelectedVariant();
    }
    auto selectedCount = rteAggregate->IsSelected();
    if(selectedCount) {
      a.selectedCount = selectedCount;
      auto res = ResultStringFromRteItem(rteAggregate);
      if(!res.empty()) {
        a.result = res;
      }
    }

    auto& gen = rteAggregate->GetAttribute("generator");
    if(!gen.empty()) {
      a.generator = gen;
    }
    if(rteAggregate->GetAttributeAsBool("generated") && !rteAggregate->GetAttributeAsBool("selectable")) {
      a.fixed = true;
    }
    auto opt = OptionsFromRteItem(rteAggregate);
    if(opt) {
      a.options = opt;
    }

    CollectCtVariants(a, rteAggregate);
    aggregates.push_back(a);
  }
  if(!aggregates.empty()) {
    parent.aggregates = aggregates;
  }
}

void RpcDataCollector::CollectCtVariants(RpcArgs::CtAggregate& ctAggregate, RteComponentAggregate* rteAggregate) const
{
  for(auto& variantName : rteAggregate->GetVariants()) {
    CtVariant v;
    v.name = variantName;
    auto components = rteAggregate->GetComponentVersions(variantName);
    if(components) {
      for(auto [version, rteComponent] : *components) {
        v.components.push_back(FromRteComponent(rteComponent));
      }
    }
    ctAggregate.variants.push_back(v);
  }
}

// end of ProjMgrRpcServerData.cpp
