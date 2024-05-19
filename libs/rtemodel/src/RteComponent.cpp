/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteComponent.cpp
* @brief CMSIS RTE Data model
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/
#include "RteComponent.h"

#include "RteCondition.h"
#include "RtePackage.h"
#include "RteFile.h"
#include "RteModel.h"
#include "RteInstance.h"
#include "RteProject.h"
#include "RteGenerator.h"

#include "XMLTree.h"

using namespace std;


RteComponent::RteComponent(RteItem* parent) :
  RteItem(parent),
  m_files(0)
{
}

RteComponent::~RteComponent()
{
  RteComponent::Clear();
}


void RteComponent::Clear()
{
  m_files = 0;
  RteItem::Clear();
}

bool RteComponent::Validate()
{
  m_bValid = RteItem::Validate();
  if (!m_bValid) {
    string msg = CreateErrorString("error", "501", "error(s) in component definition:");
    m_errors.push_front(msg);
  }
  RteModel* model = GetModel();
  if (model && !IsApi()) {
    RteComponent* c = model->GetComponent(GetID());
    if (c && c != this && c->GetPackage() == GetPackage()) {
      m_bValid = false;
      string msg = CreateErrorString("warning", "520", "multiple component definition");
      m_errors.push_back(msg);
    }
  }

  return m_bValid;
}

bool RteComponent::Dominates(RteComponent *that) const {
  RtePackage *thisPackage = GetPackage();
  RtePackage *thatPackage = that ? that->GetPackage() : nullptr;

  bool thisDominating = thisPackage ? thisPackage->IsDominating() : false;
  bool thatDominating = thatPackage ? thatPackage->IsDominating() : false;

  if (thisDominating && !thatDominating) // only this component dominates
    return true;

  // both dominate: return true if this component version newer than that one
  if (thisDominating && thatDominating) {
    return (VersionCmp::Compare(GetVersionString(), that->GetVersionString()) > 0);
  }

  return false;
}

string RteComponent::ConstructComponentPreIncludeFileName() const
{
  string fileName = "Pre_Include_";
  return fileName + WildCards::ToX(ConcatenateCclassCgroupCsub('_')) + ".h";
}

bool RteComponent::IsDeviceDependent() const
{
  if (GetCclassName() == "Device")
    return true;
  return RteItem::IsDeviceDependent();
}

bool RteComponent::IsDeviceStartup() const
{
  return GetCclassName() == "Device" &&
    GetCgroupName() == "Startup" &&
    GetCsubName().empty();
}


const string& RteComponent::GetName() const
{
  return GetAttribute("Cname");
}

RteBundle* RteComponent::GetParentBundle() const
{
  return dynamic_cast<RteBundle*>(GetParent());
}


size_t RteComponent::GetFileCount() const
{
  return m_files ? m_files->GetChildCount() : 0;
}

RteItem::ConditionResult RteComponent::GetConditionResult(RteConditionContext* context) const
{
  ConditionResult result = RteItem::GetConditionResult(context);
  if (result < INSTALLED)
    return result;
  RteTarget* target = context->GetTarget();
  RteItem::ConditionResult apiRes = IsApiAvailable(target);
  if (apiRes < FULFILLED) {
    return apiRes;
  } else if(apiRes == FULFILLED) { // api exists
    RteApi* thisApi = GetApi(target, true); // no need to check for NULL
    set<RteComponent*> components;
    apiRes = target->GetComponentsForApi(thisApi, components, true);
    if (apiRes == CONFLICT) {
      return apiRes;
    }
  }
  return result;
}



const string& RteComponent::GetVendorString() const
{
  if (IsApi())
    return EMPTY_STRING;
  const string& vendor = GetAttribute("Cvendor");
  if (vendor.empty()) {
    if (m_parent)
      return m_parent->GetVendorString();
  }
  return vendor;
}

const string& RteComponent::GetVersionString() const
{
  const string& s = GetAttribute("Cversion");
  if (s.empty() && m_parent)
    return m_parent->GetVersionString();
  return s;
}

const string& RteComponent::GetCclassName() const
{
  const string& s = GetAttribute("Cclass");
  if (s.empty() && m_parent)
    return m_parent->GetCclassName();
  return s;
}

const string& RteComponent::GetCbundleName() const
{
  const string& s = GetAttribute("Cbundle");
  if (s.empty() && m_parent)
    return m_parent->GetCbundleName();
  return s;
}


string RteComponent::ConstructID()
{
  return GetComponentUniqueID();
};


string RteComponent::GetDisplayName() const
{
  string id = GetName();
  if (id.empty()) {
    if (!GetCsubName().empty()) {
      id = GetCsubName();
    } else {
      id = GetCgroupName();
    }
  }
  return id;
}

string RteComponent::GetFullDisplayName() const
{
  return GetComponentID(true);
}

string RteComponent::GetDocFile() const
{
  string doc;
  if (!m_files)
    return doc;
  RteFile* fDoc = nullptr;
  for (auto child : m_files->GetChildren()) {
    RteFile* f = dynamic_cast<RteFile*>(child);
    if (f && !f->IsConfig() && f->GetCategory() == RteFile::Category::DOC) {
      fDoc = f;
      break;
    }
  }
  if (fDoc) {
    doc = fDoc->GetDocFile();
  }
  return doc;
}

RteItem* RteComponent::CreateItem(const std::string& tag)
{
  if (tag == "files") {
    if (!m_files) {
      m_files = new RteFileContainer(this);
    }
    return m_files;
  }
  return RteItem::CreateItem(tag);
}

void RteComponent::Construct()
{
  RtePackage* pack = GetPackage();
  if (pack && pack->IsGenerated()) {
    SetAttribute("generated", "1");
  }

  // inherit attributes from bundle
  if (GetAttribute("Cclass").empty()) {
    AddAttribute("Cclass", GetCclassName());
  }
  // ensure Cvendor Cversion and Cbundle for components, not for apis
  if (!IsApi()) {
    if (m_parent && GetAttribute("Cbundle").empty()) {
      AddAttribute("Cbundle", m_parent->GetCbundleName(), false); // insert bundle ID if not empty
    }
    if (GetAttribute("Cvendor").empty()) {
      AddAttribute("Cvendor", GetVendorString());
    }
    if (GetAttribute("Cversion").empty()) {
      AddAttribute("Cversion", GetVersionString(), false);
    }
  }

  RteItem::Construct();
}


bool RteComponent::HasApi(RteTarget* target) const
{
  if (HasAttribute("Capiversion"))
    return true;
  RteApi* a = nullptr;
  string apiId = GetApiID(false);
  if (target)
    a = target->GetApi(apiId);
  if (!a)
    GetModel()->GetApi(apiId);
  return a != nullptr;
}

RteItem::ConditionResult RteComponent::IsApiAvailable(RteTarget* target) const
{
  if (!target) {
    return UNAVAILABLE;
  }
  if (!HasApi(target)) {
    return IGNORED;
  }
  RteApi* api = GetApi(target, true);
  if (api) {
    return FULFILLED;
  }
  api = GetApi(target, false);
  if (!api) {
    return MISSING_API;
  }
  if (api->GetApiVersionString().empty()) {
    return FULFILLED; // API without version - deprecated case that we still need to support
  }
  return MISSING_API_VERSION;
}


RteApi* RteComponent::GetApi(RteTarget* target, bool matchVersion) const
{
  if (!target) {
    return nullptr;
  }
  if (matchVersion) {
    return target->GetApi(GetAttributes());
  }
  return target->GetApi(GetApiID(false));
}

RteGenerator* RteComponent::GetGenerator() const
{
  RteGenerator* generator = nullptr;
  const string& generatorName = GetGeneratorName();
  if (!generatorName.empty()) {
    RtePackage* pack = GetPackage();
    if (pack) {
      generator = pack->GetGenerator(generatorName);
    }
    if (!generator && GetCallback()) {
      generator = GetCallback()->GetExternalGenerator(generatorName);
    }
  }
  return generator;
}

string RteComponent::GetGpdscFile(RteTarget* target) const
{
  RtePackage* pack = GetPackage();
  if (pack) {
    if (IsGenerated()) {
      return pack->GetPackageFileName();
    } else {
      RteGenerator* gen = GetGenerator();
      if (gen) {
        const auto& ci = target->GetProject()->GetComponentInstance(this->GetID());
        const auto& genDir = ci ? ci->GetAttribute("gendir") : RteUtils::EMPTY_STRING;
        return gen->GetExpandedGpdsc(target, genDir);
      }
    }
  }
  return EMPTY_STRING;
}

void RteComponent::FilterFiles(RteTarget* target)
{
  if (!m_files)
    return;
  if (!target->IsTargetSupported())
    return;
  // iterate through full list selecting files that pass target.
  set<RteFile*> s;
  for (auto child : m_files->GetChildren()) {
    RteFile* f = dynamic_cast<RteFile*>(child);
    if (!f)
      continue;

    if (f->GetCondition() != nullptr) {
      if (f->Evaluate(target->GetFilterContext()) < FULFILLED)
        continue;
      if (f->Evaluate(target->GetDependencySolver()) < FULFILLED)
        continue;
    }
    s.insert(f);
  }
  if (!s.empty()) {
    target->AddFilteredFiles(this, s);
  }
}


void RteComponent::InsertInModel(RteModel* model)
{
  if (model) {
    model->InsertComponent(this);
  }
}

RteApi::RteApi(RteItem* parent) :
  RteComponent(parent)
{
}

bool RteApi::IsExclusive() const
{
  string s = GetAttribute("exclusive");
  if (s == "0" || s == "false")
    return false;
  return true; // true by default
}


string RteApi::GetFullDisplayName() const
{
  return GetApiID(true);
}

string RteApi::GetComponentUniqueID() const
{
  return GetApiID(true);
}

string RteApi::GetComponentID(bool withVersion) const
{
  withVersion = false; // api ID is always without version (the latest is used)
  return GetApiID(withVersion);
}



bool RteApi::Validate()
{
  m_bValid = RteComponent::Validate();
  return m_bValid;
}



RteComponentAggregate::RteComponentAggregate(RteItem* parent) :
  RteComponentContainer(parent),
  m_instance(0),
  m_maxInstances(0),
  m_bHasMaxInstances(false),
  m_nSelected(0)
{

}
RteComponentAggregate::~RteComponentAggregate()
{
  m_instance = 0;
  RteComponentAggregate::Clear();
}


void RteComponentAggregate::Clear()
{
  // NOTE! : implementation should not call base, as it does not allocate RteItem childeren
  m_components.clear();
  ClearAttributes();
}

bool RteComponentAggregate::IsFiltered() const
{
  if (GetSelectedBundleShortID() != GetBundleShortID())
    return false;
  return true;
}


bool RteComponentAggregate::IsUsed() const
{
  return m_instance != nullptr;
}


bool RteComponentAggregate::SetSelected(int nSel)
{
  if (nSel > m_maxInstances)
    nSel = m_maxInstances;
  if (nSel != m_nSelected) {
    m_nSelected = nSel;
    return true;
  }
  return false;
}

bool RteComponentAggregate::MatchComponentAttributes(const map<string, string>& attributes, bool bRespectVersion) const
{
  if (!m_components.empty()) {
    for (auto [_, versionMap] : m_components) {
      for (auto [_, c] : versionMap) {
        if (c && c->MatchComponentAttributes(attributes, bRespectVersion))
          return true;
      }
    }
    return false;
  } else if (m_instance)
    return m_instance->MatchComponentAttributes(attributes, bRespectVersion);
  return false;
}

bool RteComponentAggregate::MatchComponentInstance(RteComponentInstance* ci) const
{
  if (!ci)
    return false;
  const map<string, string>& attributes = ci->GetAttributes();

  for (auto [a, v] : m_attributes) {
    if (!a.empty() && a[0] == 'C') {
      auto ita = attributes.find(a);
      if (ita == attributes.end()) { // no attribute in supplied map
        if (a == "Cbundle" || v.empty())
          continue;
        return false;
      }
      if (!WildCards::Match(v, ita->second))
        return false;
    }
  }
  return true; // no other checks
}


void RteComponentAggregate::AddComponent(RteComponent* c)
{
  const string& version = c->GetVersionString();
  const string& variant = c->GetCvariantName();
  const string& generator = c->GetGeneratorName();
  RtePackage* pack = c->GetPackage();
  bool bDominating = pack && pack->IsDominating();

  if (!generator.empty() && GetGeneratorName() != generator)
    m_components.clear(); // for generated components only one can be added to aggregate
  else if (generator.empty() && IsGenerated())
    return; // aggregate with generator can only contain generated components
  if (m_components.empty()) {
    m_ID = c->GetComponentAggregateID();
    m_fullDisplayName = c->GetComponentAggregateID();
    m_maxInstances = c->GetMaxInstances();
    m_bHasMaxInstances = c->HasMaxInstances();
    ClearAttributes();
    AddAttribute("Cclass", c->GetCclassName());
    AddAttribute("Cgroup", c->GetCgroupName());
    AddAttribute("Csub", c->GetCsubName());
    AddAttribute("Cvendor", c->GetVendorString());
    AddAttribute("Cbundle", c->GetCbundleName());
    AddAttribute("generator", generator, false);
    AddAttribute("generated", c->GetAttribute("generated"), false);
    AddAttribute("selectable", c->GetAttribute("selectable"), false);
    m_selectedVariant = variant; // set default variant
  }

  if (c->IsDefaultVariant()) {
    // set default variant
    m_selectedVariant = variant;
    m_defaultVariant = variant;
  }

  map<string, RteComponentVersionMap>::iterator itvar = m_components.find(variant);
  if (itvar == m_components.end()) {
    m_components[variant] = RteComponentVersionMap();
  }

  RteComponentVersionMap& versionMap = m_components[variant];
  // check for dominate and version between dominating components.
  // Keep all dominating component versions.
  // versionMap keeps only dominating components in case of mixed ones
  if (versionMap.begin() != versionMap.end()) {
    RteComponent* inserted = versionMap.begin()->second;
    RtePackage* insertedPack = inserted->GetPackage();
    if (!insertedPack || !insertedPack->IsDominating()) {
      // already inserted pack is NOT dominating
      if (bDominating) {
        // new pack is dominating
        versionMap.clear(); // only dominating component is available
      }
    } else {
      if (!bDominating) {
        return; // already inserted pack is dominating, new one is not dominating
      }
    }
  }

  RteComponentVersionMap::iterator it = versionMap.find(version);
  if (it != versionMap.end()) {
    // check if it is the same component
    RteComponent* inserted = it->second;
    if (inserted == c)
      return; // nothing to do
    RtePackage* insertedPack = inserted->GetPackage();
    RtePackage* devicePack = GetTarget()->GetEffectiveDevicePackage();
    if (insertedPack == devicePack)
      return; // component from device pack is already installed
    if (insertedPack && devicePack && pack != devicePack) {
      const string& packVersion = pack->GetVersionString();
      const string& insertedPackVersion = insertedPack->GetVersionString();
      if (VersionCmp::Compare(packVersion, insertedPackVersion) < 0)
        return; // the inserted component comes from newer package
    }
  }
  versionMap[version] = c;
  if (c->HasMaxInstances()) {
    m_bHasMaxInstances = true;
    int maxInstances = c->GetMaxInstances();
    if (m_maxInstances < maxInstances)
      m_maxInstances = maxInstances;
  }
}

void RteComponentAggregate::SetComponentInstance(RteComponentInstance* ci, int count)
{
  if (count >= 0) {
    m_nSelected = count;
    m_instance = ci;
  } else {
    m_instance = nullptr;
  }

  if (!m_instance) {
    m_nSelected = 0;
    m_text.clear();
    Purge();
    return;
  }

  RteTarget* t = GetTarget();
  const string& targetName = t->GetName();
  RteComponent* c = ci->GetResolvedComponent(targetName);
  RteItem* ei = ci->GetEffectiveItem(targetName);
  m_selectedVariant = ei->GetCvariantName();
  m_selectedVersion = ei->GetVersionString();

  if (m_components.empty()) {
    if (c) {
      AddComponent(c);
      m_fullDisplayName = c->GetComponentAggregateID();
      m_maxInstances = c->GetMaxInstances();
      m_bHasMaxInstances = c->HasMaxInstances();
    } else {
      m_fullDisplayName = ci->GetComponentAggregateID();
      m_maxInstances = ci->GetMaxInstances();
      m_bHasMaxInstances = ci->HasMaxInstances();
    }
    m_ID = ei->GetComponentAggregateID();
    ClearAttributes();
    AddAttribute("Cclass", ei->GetCclassName());
    AddAttribute("Cgroup", ei->GetCgroupName());
    AddAttribute("Csub", ei->GetCsubName());
    AddAttribute("Cvendor", ei->GetVendorString());
    AddAttribute("Cbundle", ei->GetCbundleName());
  }

  if (!c) {
    string packId = ci->GetPackageID(true);
    const RtePackageFilter& filter = t->GetPackageFilter();
    if (!filter.IsPackageSelected(packId)) {
      packId = RtePackage::ReleaseIdFromId(packId);
    }
    ConditionResult res = ci->GetResolveResult(targetName);
    if (res == MISSING) {
      m_text = "Component is missing (previously found in pack '";
      m_text += packId;
      m_text += "\')";
    } else {
      m_text = "Component is not available for target '";
      m_text += targetName;
      m_text += "'";
      RteModel* model = GetModel();
      RtePackage* pack = model->GetPackage(packId);
      if (pack && !filter.IsPackageFiltered(packId)) {
        m_text += ", pack \'";
        m_text += packId;
        m_text += "\' is not selected";
      }
    }
  } else {
    m_text.clear();
  }

  if (ci->HasMaxInstances()) {
    m_bHasMaxInstances = true;
    int maxInstances = ci->GetMaxInstances();
    if (m_maxInstances < maxInstances)
      m_maxInstances = maxInstances;
  }
}


void RteComponentAggregate::Purge()
{
  map<string, RteComponentVersionMap>::iterator itvar;
  for (itvar = m_components.begin(); itvar != m_components.end(); ) {
    auto itvarcur = itvar++;
    RteComponentVersionMap& versionMap = itvarcur->second;
    RteComponentVersionMap::iterator it;
    for (it = versionMap.begin(); it != versionMap.end(); ) {
      auto itcur = it++;
      if (!itcur->second) {
        versionMap.erase(itcur);
      }
    }
    if (versionMap.empty()) {
      m_components.erase(itvarcur);
    }
  }
  if (!m_instance && !GetComponent()) {
    ResetToDefaultVariant();
  }
}



bool RteComponentAggregate::HasComponent(RteComponent* c) const
{
  for (auto [_, versionMap] : m_components) {
    for (auto [_, component] : versionMap) {
      if (c == component)
        return true;
    }
  }
  return false;
}

bool RteComponentAggregate::IsEmpty() const
{
  return m_components.empty() && m_instance == nullptr;
}

string RteComponentAggregate::GetEffectiveVersion() const
{
  if (!m_selectedVersion.empty()) {
    return m_selectedVersion;
  }
  return GetLatestVersion(m_selectedVariant);
}

string RteComponentAggregate::GetLatestVersion(const string& variant) const
{
  map<string, RteComponentVersionMap>::const_iterator itvar = m_components.find(variant);
  if (itvar != m_components.end()) {
    auto it = itvar->second.begin();
    if (it != itvar->second.end())
      return it->first;
  }
  if (m_instance)
    return m_instance->GetVersionString();
  return EMPTY_STRING;
}

void RteComponentAggregate::ResetToDefaultVariant()
{
  if (!GetComponentVersions(m_defaultVariant)) {
    m_defaultVariant.clear();
    m_selectedVersion.clear();
    auto itvar = m_components.begin();
    if (itvar != m_components.end())
      m_defaultVariant = itvar->first;
  }
  SetSelectedVariant(m_defaultVariant);
}


void RteComponentAggregate::SetSelectedVariant(const string& variant)
{
  m_selectedVariant = variant;
  // adjust version
  if (!GetComponent()) {
    // no component with such version exist
    if (m_instance && m_instance->GetCvariantName() == m_selectedVariant) {
      m_selectedVersion = m_instance->GetVersionString();
    } else {
      m_selectedVersion.clear();
    }
  }
}


RteComponent* RteComponentAggregate::GetComponent() const
{
  return GetComponent(m_selectedVariant, GetEffectiveVersion());
}

RtePackage* RteComponentAggregate::GetPackage() const // returns package of the active component
{
  RteComponent* c = GetComponent();
  if (c)
    return c->GetPackage();
  return nullptr;
}

string RteComponentAggregate::GetPackageID(bool withVersion) const
{
  RteComponent* c = GetComponent();
  if(c) {
    RteItem* packInfo = c->HasAttribute("selectable") ? c->GetFirstChild("package") : nullptr;
    if(packInfo) {
      return RtePackage::GetPackageIDfromAttributes(*packInfo, withVersion);
    }
    return c->GetPackageID(withVersion);
  }
  if (m_instance)
    return m_instance->GetPackageID(withVersion);
  return EMPTY_STRING;
}

string RteComponentAggregate::GetToolTip() const
{
  string toolTip;
  RteComponent* c = GetComponent();
  if (c)
    toolTip = c->GetFullDisplayName();
  else if (m_instance)
    toolTip = m_instance->GetFullDisplayName();

  toolTip += "\nPack: ";
  toolTip += GetPackageID(true);
  return toolTip;
}


const RteComponentVersionMap* RteComponentAggregate::GetComponentVersions(const string& variant) const
{
  map<string, RteComponentVersionMap>::const_iterator itvar = m_components.find(variant);
  if (itvar != m_components.end())
    return &(itvar->second);
  return nullptr;
}

int RteComponentAggregate::GetComponentCount() const
{
  int count = 0;
  for (auto [_, versionMap] : m_components) {
    count += (int)versionMap.size();
  }
  return count;
}

RteComponent* RteComponentAggregate::GetComponent(const string& variant, const string& version) const
{
  map<string, RteComponentVersionMap>::const_iterator itvar = m_components.find(variant);
  if (itvar == m_components.end())
    return nullptr;

  RteComponentVersionMap::const_iterator it = itvar->second.find(version);
  if (it != itvar->second.end())
    return it->second;
  return nullptr;
}

RteComponent* RteComponentAggregate::GetLatestComponent(const string& variant) const
{
  const RteComponentVersionMap* versions = GetComponentVersions(variant);
  if (!versions && variant.empty()) {
    // RTE migration policy when adding variants to components
    // use default (selected) variant if argument is empty
    versions = GetComponentVersions(m_selectedVariant);
  }

  if (versions) {
    RteComponentVersionMap::const_iterator it = versions->begin();
    if (it != versions->end())
      return it->second;
  }
  return nullptr;
}


RteComponent* RteComponentAggregate::FindComponentMatch(const string& variant, const string& pattern) const
{
  auto itvar = m_components.begin();
  for (; itvar != m_components.end(); ++itvar) {
    if (WildCards::Match(variant, itvar->first))
      break;
  }

  if (itvar == m_components.end())
    return nullptr;

  const RteComponentVersionMap& versionMap = itvar->second;
  for (auto [ver, component] : versionMap) {
    if (WildCards::Match(pattern, ver))
      return component;
  }
  return nullptr;
}

RteComponent* RteComponentAggregate::FindComponent(const map<string, string>& attributes) const
{
  {
    RteComponent* c = GetComponent();
    if(c && c->MatchComponentAttributes(attributes))
      return c;
  }

  for (auto [_, versionMap] : m_components) {
    for (auto [_, c] : versionMap) {
      if (c && c->MatchComponentAttributes(attributes))
        return c;
    }
  }
  return nullptr;
}

list<string> RteComponentAggregate::GetVersions(const string& variant) const
{
  list<string> versions;
  auto itvar = m_components.find(variant);
  if (itvar != m_components.end()) {
    const RteComponentVersionMap& versionMap = itvar->second;
    for (auto [ver, c] : versionMap) {
      versions.push_back(ver);
    }
  }
  return versions;
}

list<string> RteComponentAggregate::GetVariants() const
{
  list<string> variants;
  if (m_instance) {
    // always add instance variant if not resolved
    RteTarget* t = GetTarget();
    const string& targetName = t->GetName();
    RteComponent* c = m_instance->GetResolvedComponent(targetName);
    if (!c) {
      const string& var = m_instance->GetCvariantName();
      if (!c && m_components.find(var) == m_components.end())
        variants.push_back(var);
    }
  }

  for (auto [var, versionMap] : m_components) {
    variants.push_back(var);
  }
  return variants;
}

bool RteComponentAggregate::HasVariant(const string& variant) const {
  if (m_instance && m_instance->GetCvariantName() == variant) {
    return true;
  }
  return m_components.find(variant) != m_components.end();
}


string RteComponentAggregate::GetDisplayName() const
{
  string id;
  if (!GetCsubName().empty()) {
    id = GetCsubName();
  } else {
    id = GetCgroupName();
  };
  return id;
}

const string& RteComponentAggregate::GetDescription() const
{
  RteComponent* c = GetComponent();
  if (c)
    return c->GetDescription();
  return m_text;
}

string RteComponentAggregate::GetDocFile() const
{
  RteComponent* c = GetComponent();
  if (c) {
    string doc = c->GetDocFile();
    if (doc.empty())
      doc = c->GetItemValue("doc"); // for generator components without own description
    return doc;
  }
  return EMPTY_STRING;
}

RteItem::ConditionResult RteComponentAggregate::GetConditionResult(RteConditionContext* context) const
{
  RteComponent* c = GetComponent();
  RteTarget* target = context->GetTarget();
  if (c) {
    ConditionResult result = c->GetConditionResult(context);
    if (IsSelected() && result > MISSING) {
      ConditionResult apiResult = c->IsApiAvailable(target);
      if (apiResult < FULFILLED) {
        return apiResult;
      }
    }
    return result;
  } else if (m_instance && !m_instance->IsExcluded(target->GetName())) {
    return m_instance->GetResolveResult(target->GetName());
  }
  return UNDEFINED;
}

RteItem::ConditionResult RteComponentAggregate::GetDepsResult(map<const RteItem*, RteDependencyResult>& results, RteTarget* target) const
{
  ConditionResult r = RteDependencyResult::GetResult(this, results);
  if (r != UNDEFINED)
    return r;
  RteComponent* c = GetComponent();
  if (c) {
    ConditionResult result = c->GetDepsResult(results, target);
    if (IsSelected()) {
      ConditionResult apiResult = c->IsApiAvailable(target);
      if (apiResult < FULFILLED) {
        RteDependencyResult depRes(this, apiResult);
        results[this] = depRes;
        return apiResult;
      }
    }
    return result;
  } else if (m_instance && !m_instance->IsExcluded(target->GetName())) {
    ConditionResult result = m_instance->GetResolveResult(target->GetName());
    if (result < SELECTABLE) {
      RteDependencyResult depRes(this, result);
      results[this] = depRes;
    }
    return result;
  }
  return UNDEFINED;
}


RteItem::ConditionResult RteComponentAggregate::Evaluate(RteConditionContext* context)
{
  RteComponent* c = GetComponent();
  if (c)
    return c->Evaluate(context);
  return UNDEFINED;
}



RteComponentContainer::RteComponentContainer(RteItem* parent) :
  RteItem(parent)
{
}

RteItem* RteComponentContainer::CreateItem(const std::string& tag)
{
  if (tag == "component") {
    return new RteComponent(this);
  } else if (tag == "bundle") {
    return new RteBundle(this);
  }
  return RteItem::CreateItem(tag);
}

const string& RteComponentContainer::GetVendorString() const
{
  const string& vendor = GetAttribute("Cvendor");
  if (vendor.empty()) {
    RtePackage* package = GetPackage();
    if (package)
      return package->GetVendorString();
  }
  return vendor;
}

RteComponentClass* RteComponentContainer::GetComponentClass() const
{
  for (RteItem* item = GetParent(); item; item = item->GetParent()) {
    RteComponentClass* compClass = dynamic_cast<RteComponentClass*>(item);
    if (compClass)
      return compClass;
  }
  return nullptr;
}

const string& RteComponentContainer::GetSelectedBundleName() const
{
  RteComponentClass* compClass = GetComponentClass();
  if (compClass)
    return compClass->GetSelectedBundleName();
  return EMPTY_STRING;
}

const string& RteComponentContainer::GetSelectedBundleShortID() const
{
  RteComponentClass* compClass = GetComponentClass();
  if (compClass)
    return compClass->GetSelectedBundleShortID();
  return EMPTY_STRING;
}


RteTarget* RteComponentContainer::GetTarget() const
{
  for (RteItem* parent = GetParent(); parent; parent = parent->GetParent()) {
    RteTarget* t = dynamic_cast<RteTarget*>(parent);
    if (t)
      return t;
  }
  return nullptr;
}


RteBundle::RteBundle(RteItem* parent) :
  RteComponentContainer(parent)
{
}

void RteBundle::InsertInModel(RteModel* model)
{
  if (model)
    model->InsertBundle(this);
  RteComponentContainer::InsertInModel(model);
}

string RteBundle::ConstructID()
{
  return GetBundleID(true);
}

string RteBundle::GetDisplayName() const
{
  return GetBundleShortID();
}

string RteBundle::GetFullDisplayName() const
{
  return m_ID;
}


const string& RteBundle::GetVersionString() const
{
  return GetAttribute("Cversion");
}



RteApiContainer::RteApiContainer(RteItem* parent) :
  RteItem(parent)
{
}

RteItem* RteApiContainer::CreateItem(const string& tag)
{
  if (tag == "api") {
    return new RteApi(this);
  }
  return RteItem::CreateItem(tag);
}


RteComponentGroup::RteComponentGroup(RteItem* parent) :
  RteComponentContainer(parent),
  m_bHasApi(false),
  m_api(0),
  m_apiInstance(0)
{
}

RteComponentGroup::~RteComponentGroup()
{
  RteComponentGroup::Clear();
}

void RteComponentGroup::Clear()
{
  for ( auto [_, g] : m_groups) {
    delete g;
  }
  m_api = 0;
  m_apiInstance = 0;
  m_bHasApi = false;
  m_groups.clear();
  RteComponentContainer::Clear();
}

bool RteComponentGroup::HasSingleAggregate() const
{
  return GetSingleComponentAggregate() != nullptr;
}

bool RteComponentGroup::IsFiltered() const
{
  return HasBundleName(GetSelectedBundleName());
}


RteComponentAggregate* RteComponentGroup::GetSingleComponentAggregate() const
{
  if (m_groups.empty() && GetChildCount() == 1) {
    RteComponentAggregate* a = dynamic_cast<RteComponentAggregate*>(*m_children.begin());
    if (a && a->GetCsubName().empty())
      return a; // the single aggregate has the same name as the group
  }
  return nullptr;
}


RteComponentAggregate* RteComponentGroup::GetComponentAggregate(const string& id) const
{
  for (auto child : m_children) {
    RteComponentAggregate* a = dynamic_cast<RteComponentAggregate*>(child);
    if (a && a->GetID() == id) {
      return a;
    }
  }
  for (auto [_, g] : m_groups)   {
    RteComponentAggregate* a = g->GetComponentAggregate(id);
    if (a)
      return a;
  }
  return nullptr;
}

RteComponentAggregate* RteComponentGroup::FindComponentAggregate(RteComponentInstance* ci) const
{
  for (auto child : m_children) {
    RteComponentAggregate* a = dynamic_cast<RteComponentAggregate*>(child);
    if (a && a->MatchComponentInstance(ci)) {
      return a;
    }
  }
  for (auto [_, g] : m_groups) {
    RteComponentAggregate* a = g ? g->FindComponentAggregate(ci) : nullptr;
    if (a)
      return a;
  }
  return nullptr;
}


int RteComponentGroup::IsSelected() const
{
  for (auto child : m_children) {
    RteComponentAggregate* a = dynamic_cast<RteComponentAggregate*>(child);
    if (a && a->IsSelected()) {
      return 1;
    }
  }

  for (auto [_, g] : m_groups) {
    if (g->IsSelected())
      return 1;
  }
  return 0;
}


RteItem::ConditionResult RteComponentGroup::Evaluate(RteConditionContext* context)
{
  if (!IsFiltered())
    return UNDEFINED;

  if (!IsSelected())
    return UNDEFINED;

  ConditionResult res = FULFILLED;

  for (auto child : m_children) {
    RteComponentAggregate* a = dynamic_cast<RteComponentAggregate*>(child);
    if (a && a->IsFiltered() && a->IsSelected()) {
      ConditionResult r = a->Evaluate(context);
      if (r < res && r > UNDEFINED)
        res = r;
    }
  }
  for (auto [_, g] : m_groups) {
    ConditionResult r = g->Evaluate(context);
    if (r < res && r > UNDEFINED)
      res = r;
  }
  return res;
}

const string& RteComponentGroup::GetApiVersionString() const
{
  if (m_api)
    return m_api->GetApiVersionString();
  else if (m_apiInstance)
    return m_apiInstance->GetApiVersionString();
  return m_apiVersionString;
}

RteItem::ConditionResult RteComponentGroup::GetConditionResult(RteConditionContext* context) const
{
  if (!IsFiltered())
    return UNDEFINED;

  if (!IsSelected())
    return UNDEFINED;

  if (m_apiInstance) {
    if (!m_api)
      return MISSING;
  }

  ConditionResult res = FULFILLED;

  for (auto child : m_children) {
    RteComponentAggregate* a = dynamic_cast<RteComponentAggregate*>(child);
    if (a && a->IsFiltered() && a->IsSelected()) {
      ConditionResult r = a->GetConditionResult(context);
      if (r < res && r > UNDEFINED)
        res = r;
    }
  }
  for (auto [_, g] : m_groups) {
    ConditionResult r = g->GetConditionResult(context);
    if (r < res && r > UNDEFINED)
      res = r;
  }
  return res;
}

RteItem::ConditionResult RteComponentGroup::GetDepsResult(map<const RteItem*, RteDependencyResult>& results, RteTarget* target) const
{
  if (!IsFiltered())
    return UNDEFINED;

  if (!IsSelected())
    return UNDEFINED;

  ConditionResult r = RteDependencyResult::GetResult(this, results);
  if (r != UNDEFINED)
    return r;

  ConditionResult res = FULFILLED;
  if (m_apiInstance && !m_api) {
    res = MISSING;
    RteDependencyResult depRes(m_apiInstance, MISSING);
    results[this] = depRes;
  }

  for (auto child : m_children) {
    RteComponentAggregate* a = dynamic_cast<RteComponentAggregate*>(child);
    if (a && a->IsFiltered() && a->IsSelected()) {
      r = a->GetDepsResult(results, target);
      if (r < res && r > UNDEFINED)
        res = r;
    }
  }
  for (auto [_, g] : m_groups) {
    r = g->GetDepsResult(results, target);
    if (r < res && r > UNDEFINED)
      res = r;
  }
  return res;
}


RteComponentAggregate* RteComponentGroup::GetComponentAggregate(RteComponent* c) const
{
  for (auto child : m_children) {
    RteComponentAggregate* a = dynamic_cast<RteComponentAggregate*>(child);
    if (a && a->HasComponent(c)) {
      return a;
    }
  }
  for (auto [_, g] : m_groups) {
    RteComponentAggregate* a = g ? g->GetComponentAggregate(c) : nullptr;
    if (a)
      return a;
  }
  return nullptr;
}

void RteComponentGroup::CollectSelectedComponentAggregates(map<RteComponentAggregate*, int>& selectedAggregates) const
{
  for (auto child : m_children) {
    RteComponentAggregate* a = dynamic_cast<RteComponentAggregate*>(child);
    if (a && a->IsSelected() && a->IsFiltered()) {
      selectedAggregates[a] = a->IsSelected();
    }
  }
  for (auto [_, g] : m_groups) {
    if (g)
      g->CollectSelectedComponentAggregates(selectedAggregates);
  }
}


void RteComponentGroup::GetUnSelectedGpdscAggregates(set<RteComponentAggregate*>& unSelectedGpdscAggregates) const
{
  for (auto child : m_children) {
    RteComponentAggregate* a = dynamic_cast<RteComponentAggregate*>(child);
    if (a && !a->IsSelected() && a->IsFiltered() && !a->GetGeneratorName().empty()) {
      unSelectedGpdscAggregates.insert(a);
    }
  }
  for (auto [_, g] : m_groups) {
    if (g)
      g->GetUnSelectedGpdscAggregates(unSelectedGpdscAggregates);
  }
}

void RteComponentGroup::ClearSelectedComponents()
{
  for (auto child : m_children) {
    RteComponentAggregate* a = dynamic_cast<RteComponentAggregate*>(child);
    if (a && a->IsSelected()) {
      a->SetSelected(0);
    }
  }
  for (auto [_, g] : m_groups) {
    if (g)
      g->ClearSelectedComponents();
  }
}

void RteComponentGroup::ClearUsedComponents()
{
  m_apiInstance = 0;
  for (auto child : m_children) {
    RteComponentAggregate* a = dynamic_cast<RteComponentAggregate*>(child);
    if (a) {
      a->SetComponentInstance(0, -1);
    }
  }
  for (auto [_, g] : m_groups) {
    if (g)
      g->ClearUsedComponents();
  }
}

void RteComponentGroup::Purge()
{
  set<RteComponentAggregate*> toDelete;
  for (auto child : m_children) {
    RteComponentAggregate* a = dynamic_cast<RteComponentAggregate*>(child);
    if (a && a->IsEmpty()) {
      toDelete.insert(a);
    }
  }
  for (auto a : toDelete) {
    RemoveItem(a);
    delete a;
  }

  for (auto [_, g] :m_groups) {
    if (g)
      g->Purge();
  }

  for (auto it = m_groups.begin(); it != m_groups.end(); ) {
    map<string, RteComponentGroup*>::iterator itcurrent = it++;
    RteComponentGroup* g = itcurrent->second;
    if (g && g->IsEmpty()) {
      m_groups.erase(itcurrent);
      delete g;
    }
  }
}

bool RteComponentGroup::IsEmpty() const
{
  if (m_apiInstance)
    return false;
  for (auto child : m_children) {
    RteComponentAggregate* a = dynamic_cast<RteComponentAggregate*>(child);
    if (a && !a->IsEmpty()) {
      return false;
    }
  }
  for (auto [_, g] : m_groups) {
    if (g && !g->IsEmpty())
      return false;
  }
  return true;
}


RteComponentGroup* RteComponentGroup::GetComponentGroup(RteComponent* c) const
{
  if (GetApi() == c)
    return const_cast<RteComponentGroup*>(this);

  for (auto child : m_children) {
    RteComponentAggregate* a = dynamic_cast<RteComponentAggregate*>(child);
    if (a && a->HasComponent(c)) {
      return const_cast<RteComponentGroup*>(this);
    }
  }
  for (auto [_, g] :  m_groups) {
    RteComponentGroup* cg = g->GetComponentGroup(c);
    if (cg)
      return cg;
  }
  return nullptr;
}


string RteComponentGroup::GetDisplayName() const
{
  string id = GetName();
  if (HasApi())
    id += " (API)";
  return id;
}


RteComponentGroup* RteComponentGroup::GetGroup(const string& name) const
{
  map<string, RteComponentGroup*>::const_iterator it = m_groups.find(name);
  if (it != m_groups.end())
    return it->second;
  return nullptr;
}


RteComponentGroup* RteComponentGroup::EnsureGroup(const string& name)
{
  RteComponentGroup* group = GetGroup(name);
  if (!group) {
    group = CreateGroup(name);
    group->SetTag(name);
    m_groups[name] = group;
  }
  return group;
}

RteComponentGroup* RteComponentGroup::CreateGroup(const string& name)
{
  return new RteComponentGroup(this);
}

void RteComponentGroup::AddComponent(RteComponent* c)
{
  if (c->IsApi()) {
    m_api = dynamic_cast<RteApi*>(c);
    m_bHasApi = true;
    return;
  }
  if (c->HasApi(GetTarget())) {
    m_bHasApi = true;
    const string& apiVersion = c->GetApiVersionString();
    if (!apiVersion.empty() && m_apiVersionString.empty())
      m_apiVersionString = apiVersion;
  }

  string aggregateId = c->GetComponentAggregateID();
  RteComponentAggregate* a = GetComponentAggregate(aggregateId);
  if (!a) {
    a = new RteComponentAggregate(this);
    m_children.push_back(a);
  }
  a->AddComponent(c);
  const string& bundleName = c->GetCbundleName();
  AddBundleName(bundleName, c->GetBundleShortID());
  if (!bundleName.empty()) {
    RteItem* bundle = c->GetParent();
    if (bundle && bundle->IsDefaultVariant())
      GetComponentClass()->SetSelectedBundleName(bundleName, false); // do not update selection
  }
}

void RteComponentGroup::AddComponentInstance(RteComponentInstance* ci, int count)
{
  RteTarget* t = GetTarget();
  if (ci->IsApi()) {
    m_apiInstance = count > 0 ? ci : nullptr;
    m_bHasApi = true;
    if (!m_api && m_apiInstance) {
      m_api = dynamic_cast<RteApi*>(m_apiInstance->GetResolvedComponent(t->GetName()));
    }
    return;
  }
  RteItem* ei = ci->GetEffectiveItem(t->GetName());
  const string& apiVersion = ei->GetApiVersionString();
  if (!apiVersion.empty()) {
    m_bHasApi = true;
    if (m_apiVersionString.empty())
      m_apiVersionString = apiVersion;
  }

  string aggregateId = ei->GetComponentAggregateID();
  RteComponentAggregate* a = GetComponentAggregate(aggregateId);
  if (count > 0 && !a) {
    a = new RteComponentAggregate(this);
    AddItem(a);
  }
  if (a) {
    a->SetComponentInstance(ci, count);
    AddBundleName(ei->GetCbundleName(), ei->GetBundleShortID());
    GetComponentClass()->SetSelectedBundleName(ei->GetCbundleName(), false); // do not update selection
  }
}

bool RteComponentGroup::HasBundleName(const string& name) const
{
  return m_bundleNames.find(name) != m_bundleNames.end();
}

void RteComponentGroup::AddBundleName(const string& name, const string& id)
{
  InsertBundleName(name, id);
  RteComponentClass* cl = GetComponentClass();
  if (cl && cl != this) {
    cl->AddBundleName(name, id);
  }
}

void RteComponentGroup::InsertBundleName(const string& name, const string& id)
{
  m_bundleNames[name] = id;
}


bool RteComponentGroup::HasMissingComponents() const
{
  if (m_apiInstance && !m_api)
    return true;

  for (auto child : m_children) {
    RteComponentAggregate* a = dynamic_cast<RteComponentAggregate*>(child);
    if (!a || !a->IsSelected())
      continue;
    RteComponent* c = a->GetComponent();
    if (c == nullptr)
      return true;
    if (GetSelectedBundleName() == c->GetCbundleName()) {
      RteTarget* target = GetTarget();
      if (c->IsApiAvailable(target) < FULFILLED)
        return true;
    }
  }

  for (auto [_, g] : m_groups)   {
    if (g->HasMissingComponents())
      return true;
  }
  return false;
}

string RteComponentGroup::GetToolTip() const
{
  string toolTip;
  RteApi* api = GetApi();
  if (api) {
    toolTip = "API: ";
    toolTip += api->GetFullDisplayName();
    toolTip += "\nPack: ";
    toolTip += api->GetPackageID(true);
  }
  return toolTip;
}


string RteComponentGroup::GetTaxonomyDescriptionID() const
{
  string id;
  if (m_parent)
    id = m_parent->GetTaxonomyDescriptionID();
  if (!id.empty())
    id += ".";
  id += GetName();
  return id;
}

const string& RteComponentGroup::GetDescription() const
{
  RteApi* api = GetApi();
  if (api) {
    const string& descr = api->GetDescription();
    if (!descr.empty())
      return descr;
  }
  RteTarget* target = GetTarget();
  RteProject* project = GetProject();
  if (project && target) {
    const map<string, RteGpdscInfo*>& gpdscInfos = project->GetGpdscInfos();
    for (auto [ _, gi] : gpdscInfos) {
      if (!gi || !gi->IsUsedByTarget(target->GetName()))
        continue;
      RtePackage* gpdscPack = gi->GetGpdscPack();
      if (gpdscPack) {
        const string& descr = gpdscPack->GetTaxonomyDescription(GetTaxonomyDescriptionID());
        if (!descr.empty())
          return descr;
      }
    }
  }
  if (target) {
    RteModel* model = target->GetFilteredModel();
    if (model)
      return model->GetTaxonomyDescription(GetTaxonomyDescriptionID());
  }
  return EMPTY_STRING;
}


string RteComponentGroup::GetDocFile() const
{
  RteApi* api = GetApi();
  if (api) {
    string doc = api->GetDocFile();
    if (!doc.empty())
      return doc;
  }

  RteTarget* target = GetTarget();
  RteProject* project = GetProject();
  if (project && target) {
    const map<string, RteGpdscInfo*>& gpdscInfos = project->GetGpdscInfos();
    for (auto [_, gi] : gpdscInfos) {
      if (!gi->IsUsedByTarget(target->GetName()))
        continue;
      RtePackage* gpdscPack = gi->GetGpdscPack();
      if (gpdscPack) {
        string doc = gpdscPack->GetTaxonomyDoc(GetTaxonomyDescriptionID());
        if (!doc.empty())
          return doc;
      }
    }
  }
  if (target) {
    RteModel* model = target->GetFilteredModel();
    if (model)
      return model->GetTaxonomyDoc(GetTaxonomyDescriptionID());
  }
  return EMPTY_STRING;
}


RteItem::ConditionResult RteComponentGroup::GetComponentAggregates(const XmlItem& componentAttributes, set<RteComponentAggregate*>& aggregates) const
{
  ConditionResult res = MISSING;

  for (auto child : m_children) {
    RteComponentAggregate* a = dynamic_cast<RteComponentAggregate*>(child);
    if (a && a->MatchComponentAttributes(componentAttributes.GetAttributes())) {
      aggregates.insert(a);
      ConditionResult r = INSTALLED;
      if (a->IsFiltered()) {
        r = SELECTABLE;
        if (a->IsSelected()) {
          RteComponent* c = a->GetComponent();
          if (c) {
            if (c->MatchComponentAttributes(componentAttributes.GetAttributes())) {
              r = FULFILLED;
            } else if (!WildCards::Match(componentAttributes.GetAttribute("Cvariant"), c->GetCvariantName())) {
              r = INCOMPATIBLE_VARIANT;
            } else {
              r = INCOMPATIBLE;
            }
          } else if (a->GetComponentInstance()) {
            r = FULFILLED; // still mark as fulfilled
          }
        }
      } else {
        // TODO: implement different behavior for conflicting bundles ?
      }
      if (res < r)
        res = r;
    }
  }
  for (auto [_, g] : m_groups) {
    if (g) {
      ConditionResult r = g->GetComponentAggregates(componentAttributes, aggregates);
      if (res < r)
        res = r;
    }
  }
  return res;
}


void RteComponentGroup::AdjustComponentSelection(const string& newBundleName)
{
  for (auto child : m_children) {
    RteComponentAggregate* a = dynamic_cast<RteComponentAggregate*>(child);
    if (!a || !a->IsFiltered())
      continue;
    int nsel = a->IsSelected();
    // find similar aggregate with the same attributes and new BundleName
    const string& variant = a->GetSelectedVariant();
    const string id = a->GetID();
    XmlItem attr(a->GetAttributes());
    attr.AddAttribute("Cbundle", newBundleName);
    attr.AddAttribute("Cvendor", "*");
    set<RteComponentAggregate*> aggregates;
    GetComponentAggregates(attr, aggregates);
    for (auto aggr : aggregates) {
      aggr->SetSelected(nsel);
      if (nsel && aggr->HasVariant(variant))
        aggr->SetSelectedVariant(variant);
    }
  }
}


RteComponentClass::RteComponentClass(RteItem* parent) :
  RteComponentGroup(parent)
{
}

RteComponentClass::~RteComponentClass()
{
}

RteComponentClass* RteComponentClass::GetComponentClass() const
{
  return const_cast<RteComponentClass*>(this);
}


void RteComponentClass::SetSelectedBundleName(const string& name, bool updateSelection)
{
  // adjust selection if needed
  if (updateSelection) {
    AdjustComponentSelection(name);
  }
  if (m_selectedBundleName == name)
    return;
  m_selectedBundleName = name;
}

void RteComponentClass::AdjustComponentSelection(const string& newBundleName)
{
  RteComponentGroup::AdjustComponentSelection(newBundleName);
  for (auto [_, g] : m_groups)   {
    g->AdjustComponentSelection(newBundleName);
  }
}


const string& RteComponentClass::GetSelectedBundleShortID() const
{
  auto it = m_bundleNames.find(m_selectedBundleName);
  if (it != m_bundleNames.end())
    return it->second;
  return EMPTY_STRING;
}


void RteComponentClass::AddBundleName(const string& name, const string& id)
{
  InsertBundleName(name, id);
  if (!HasBundleName(m_selectedBundleName)) {
    m_selectedBundleName = name;
  }
}

const string& RteComponentClass::EnsureSelectedBundleName()
{
  if (m_bundleNames.empty()) {
    m_selectedBundleName.clear();
  } else if (!HasBundleName(m_selectedBundleName)) {
    m_selectedBundleName = m_bundleNames.begin()->first;
  }
  return m_selectedBundleName;
}

RteBundle* RteComponentClass::GetSelectedBundle() const
{
  if (!m_selectedBundleName.empty()) {
    const string& bundleId = GetSelectedBundleShortID();
    return get_or_null(GetTarget()->GetFilteredBundles(), bundleId);
  }
  return nullptr;
}

string RteComponentClass::GetTaxonomyDescriptionID() const
{
  return GetName();
}

string RteComponentClass::GetDocFile() const
{
  RteBundle* b = GetSelectedBundle();
  if (b) {
    string doc = b->GetDocFile();
    if (!doc.empty())
      return doc;
  }
  return RteComponentGroup::GetDocFile();
}

const string& RteComponentClass::GetDescription() const
{
  RteBundle* b = GetSelectedBundle();
  if (b) {
    const string& descr = b->GetDescription();
    if (!descr.empty())
      return descr;
  }
  return RteComponentGroup::GetDescription();
}

string RteComponentClass::GetToolTip() const
{
  string toolTip;
  RteBundle* b = GetSelectedBundle();
  if (b) {
    toolTip = "Bundle: ";
    toolTip += b->GetFullDisplayName();
    toolTip += "\nPack: ";
    toolTip += b->GetPackageID(true);
  }
  return toolTip;
}


RteComponentClassContainer::RteComponentClassContainer(RteItem* parent) :
  RteComponentGroup(parent)
{
}


RteComponentClass* RteComponentClassContainer::FindComponentClass(const string& name) const
{
  return dynamic_cast<RteComponentClass*>(GetGroup(name));
}

RteComponentGroup* RteComponentClassContainer::CreateGroup(const string& name)
{
  return new RteComponentClass(this);
}

RteItem::ConditionResult RteComponentClassContainer::GetComponentAggregates(const XmlItem& componentAttributes, set<RteComponentAggregate*>& aggregates) const
{
  RteComponentGroup* classGroup = GetGroup(componentAttributes.GetAttribute("Cclass"));
  if (classGroup)
    return classGroup->GetComponentAggregates(componentAttributes, aggregates);
  return MISSING;
}


RteComponentAggregate* RteComponentClassContainer::FindComponentAggregate(RteComponentInstance* ci) const
{
  if (!ci)
    return nullptr;
  const string& className = ci->GetCclassName();
  RteComponentGroup* classGroup = GetGroup(className);
  if (classGroup)
    return classGroup->FindComponentAggregate(ci);
  return nullptr;
}

// End of RteComponent.cpp
