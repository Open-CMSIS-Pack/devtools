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

using namespace Args;

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

Args::Component RpcDataCollector::FromRteComponent(const RteComponent* rteComponent) const{
    Args::Component c;
    c.id = rteComponent->GetComponentID(true);
    c.fromPack = rteComponent->GetPackageID(true);
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

Args::ComponentInstance RpcDataCollector::FromComponentInstance(const RteComponentInstance* rteCi) const {
  ComponentInstance ci;
  if(rteCi) {
    ci.id = rteCi->GetDisplayName();
    ci.selectedCount = rteCi->GetInstanceCount(m_target->GetName());
    auto& layer = rteCi->GetAttribute("layer");
    if(!layer.empty()) {
      ci.layer = layer;
    }
    auto rteComponent = rteCi->GetResolvedComponent(m_target->GetName());
    if(rteComponent) {
      ci.resolvedComponent = FromRteComponent(rteComponent);
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

void RpcDataCollector::CollectUsedItems(Args::UsedItems& usedItems) const {

  auto rteProject = m_target ? m_target->GetProject() : nullptr;
  if(!rteProject) {
    return;
  }

  for(auto [_id, rteCi] : rteProject->GetComponentInstances()) {
    usedItems.components.push_back(FromComponentInstance(rteCi));
  }
  RtePackageMap packs;
  rteProject->GetUsedPacks(packs, m_target->GetName());
  for(auto [id, rtePack] : packs) {
    Pack p;
    p.id = id;
    usedItems.packs.push_back(p);
  }
}


void RpcDataCollector::CollectCtClasses(Args::CtRoot& root) const {

  auto classContainer = m_target ? m_target->GetClasses() : nullptr;
  if(!classContainer) {
    return; // can happen if no solution is loaded
  }

  for(auto [name, rteClass] : classContainer->GetGroups()) {
    Args::CtClass ctClass;
    ctClass.name = name;
    auto activeBundle = rteClass->GetSelectedBundleName();
    ctClass.activeBundle = activeBundle;
    auto taxonomyItem = GetTaxonomyItem(rteClass);
    if(taxonomyItem) {
      ctClass.taxonomy = FromRteItem<Taxonomy>(taxonomyItem->GetTaxonomyDescriptionID(), taxonomyItem);
    }
    CollectCtBundles(ctClass, rteClass);
    root.classes.push_back(ctClass);
  }
}

void RpcDataCollector::CollectCtBundles(Args::CtClass& ctClass, RteComponentGroup* rteClass) const {
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

void RpcDataCollector::CollectCtChildren(Args::CtTreeItem& parent, RteComponentGroup* parentRteGroup, const string& bundleName) const
{
  // collect aggregates to this level
  CollectCtAggregates(parent, parentRteGroup, bundleName);
  auto& rteGroups = parentRteGroup->GetGroups();
  if(rteGroups.empty()) {
    return;
  }
  vector<CtGroup> groups;
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
    // subgroups
    CollectCtChildren(g, rteGroup, bundleName);
    CollectCtAggregates(g, rteGroup,bundleName);
    // add to parent collection
    groups.push_back(g);
  }
  parent.groups = groups;
}

void RpcDataCollector::CollectCtAggregates(Args::CtTreeItem& parent, RteComponentGroup* parentRteGroup, const string& bundleName) const
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
    }

    auto layer = rteAggregate->GetAttribute("layer");
    if(!layer.empty()) {
      a.layer = layer;
    }

    CollectCtVariants(a, rteAggregate);
    aggregates.push_back(a);
  }
  if(!aggregates.empty()) {
    parent.aggregates = aggregates;
  }
}

void RpcDataCollector::CollectCtVariants(Args::CtAggregate& ctAggregate, RteComponentAggregate* rteAggregate) const
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
