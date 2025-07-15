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

#include "CollectionUtils.h"

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

RpcDataCollector::RpcDataCollector(RteTarget* t) :
  m_target(t)
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

void RpcDataCollector::CollectUsedItems(RpcArgs::UsedItems& usedItems) const {
  auto rteProject = m_target ? m_target->GetProject() : nullptr;
  if(!rteProject) {
    return;
  }

  for(auto [_id, rteCi] : rteProject->GetComponentInstances()) {
    if(!rteCi->IsApi()) {
      usedItems.components.push_back(FromComponentInstance(rteCi));
    }
  }
  RtePackageMap packs;
  rteProject->GetUsedPacks(packs, m_target->GetName());
  for(auto [id, rtePack] : packs) {
    Pack p;
    p.id = id;
    usedItems.packs.push_back(p);
  }
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
