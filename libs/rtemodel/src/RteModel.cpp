/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteModel.cpp
* @brief CMSIS RTE Data Model
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "RteModel.h"

#include "RteGenerator.h"
#include "RteProject.h"
#include "CprjFile.h"

#include "RteFsUtils.h"
#include "RteConstants.h"

#include "XMLTree.h"

using namespace std;

//////////////////////////////////////////////////////////
RteModel::RteModel(RteItem* parent, PackageState packageState) :
  RteItem(parent),
  m_packageState(packageState),
  m_callback(NULL),
  m_apiList(VersionCmp::Greater(RteConstants::PREFIX_CVERSION_CHAR)),
  m_bUseDeviceTree(true),
  m_filterContext(NULL)
{
  m_deviceTree = new RteDeviceItemAggregate("DeviceList", RteDeviceItem::VENDOR_LIST, NULL);
}


//////////////////////////////////////////////////////////
RteModel::RteModel(PackageState packageState) :
  RteItem(NULL),
  m_packageState(packageState),
  m_callback(NULL),
  m_bUseDeviceTree(false),
  m_filterContext(NULL)
{
  m_deviceTree = new RteDeviceItemAggregate("DeviceList", RteDeviceItem::VENDOR_LIST, NULL);
}


RteModel::~RteModel()
{
  RteModel::Clear();
  m_filterContext = NULL;
  delete m_deviceTree;
}

void RteModel::Clear()
{
  ClearModel();
}

void RteModel::ClearModel()
{
  ClearDevices();

  m_componentList.clear();
  m_apiList.clear();
  m_taxonomy.clear();
  m_bundles.clear();

  m_layerDescriptors.clear();
  m_templateDescriptors.clear();

  m_packageDuplicates.clear();
  m_packages.clear();
  m_latestPackages.clear();

  m_children.clear(); // clear children here, the packs are deleted by RtePackRegistry
  RteItem::Clear();
}

void RteModel::ClearDevices()
{
  for (auto [_, dv] : m_deviceVendors) {
    delete dv;
  }
  m_deviceVendors.clear();
  m_deviceTree->Clear();
  m_boards.clear();
}


 RteCallback* RteModel::GetCallback() const
 {
   return m_callback ? m_callback : RteCallback::GetGlobal();
 }

void RteModel::SetCallback(RteCallback* callback)
{
  m_callback = callback;
}

RtePackage* RteModel::GetPackage(const string& id) const
{
  auto it = m_packages.find(id);
  if (it != m_packages.end())
    return it->second;
  return NULL;
}


RtePackage* RteModel::GetLatestPackage(const string& id) const
{
  auto it = m_latestPackages.find(RtePackage::CommonIdFromId(id));
  if (it != m_latestPackages.end())
    return it->second;
  return NULL;
}

RtePackage* RteModel::GetAvailablePackage(const string& id) const
{
  RtePackage* pack = GetPackage(id);
  if (pack)
    return pack;
  pack = GetLatestPackage(id);
  if (!pack)
    return nullptr;
  string packVer = RtePackage::VersionFromId(id);
  if (VersionCmp::Compare(pack->GetVersionString(), packVer) >= 0) {
    return pack;
  }
  return nullptr;
}


RtePackage* RteModel::GetPackage(const XmlItem& attr) const
{
  string commonId = RtePackage::GetPackageIDfromAttributes(attr, false);
  RtePackage* pack = GetLatestPackage(commonId);
  if (!pack)
    return NULL; // latest not found => none is found
  const string& versionRange = attr.GetAttribute("version");
  if (versionRange.empty()) {
    return pack; // version is not provided => the latest
  }
  if (VersionCmp::RangeCompare(pack->GetVersionString(), versionRange) == 0)
    return pack; // the latest matches the range

  for (auto itp = m_packages.begin(); itp != m_packages.end(); ++itp) {
    pack = itp->second;
    if (pack->GetPackageID(false) != commonId)
      continue;
    if (VersionCmp::RangeCompare(pack->GetVersionString(), versionRange) == 0)
      return pack; // the latest matches the range
  }
  return NULL;
}

RteBoard* RteModel::FindBoard(const string& displayName) const
{
  for (auto [_, b] : GetBoards()) {
    if (b->GetDisplayName() == displayName) {
      return b;
    }
  }
  return nullptr;
}

void RteModel::GetCompatibleBoards(vector<RteBoard*>& boards, RteDeviceItem* device, bool bOnlyMounted) const
{
  if (!device) {
    return;
  }
  XmlItem ea;
  device->GetEffectiveAttributes(ea);
  for (auto [_, b] : GetBoards()) {
    if (b->HasCompatibleDevice(ea, bOnlyMounted)) {
      boards.push_back(b);
    }
  }
}

RteBoard* RteModel::FindCompatibleBoard(const string& displayName, RteDeviceItem* device, bool bOnlyMounted) const
{
  vector<RteBoard*> boards;
  GetCompatibleBoards(boards, device, bOnlyMounted);
  for (auto b : boards) {
    if (b->GetDisplayName() == displayName) {
      return b;
    }
  }
  return nullptr;
}

RteComponent* RteModel::FindComponents(const RteItem& item, std::list<RteComponent*>& components) const
{
  const string packId = item.GetPackageID(true);
  if (!packId.empty()) {
    RtePackage* pack = GetAvailablePackage(packId);
    return pack ? pack->FindComponents(item, components) : nullptr;
  }
  for (auto [key, pack] : m_packages) {
    pack->FindComponents(item, components);
  }
  return components.empty()? nullptr : *(components.begin());
}

RteComponent* RteModel::FindFirstComponent(const RteItem& item) const
{
  std::list<RteComponent*> components;
  return FindComponents(item, components);
}

RteComponent* RteModel::GetComponent(const string& uniqueID) const
{
  // look in the APIs
  map<string, RteApi* >::const_iterator ita = m_apiList.find(uniqueID);
  if (ita != m_apiList.end()) {
    return ita->second;
  }

  // look in the components
  RteComponentMap::const_iterator itc = m_componentList.find(uniqueID);
  if (itc != m_componentList.end()) {
    return itc->second;
  }
  return NULL;
}

RteComponent* RteModel::FindComponent(const std::string& id) const
{
  bool withVersion = id.find(RteConstants::PREFIX_CVERSION_CHAR) != string::npos;
  for (auto [_, c] : m_componentList) {
    if (c->GetComponentID(withVersion) == id) {
      return c;
    }
  }
  return nullptr;
}

RteComponent* RteModel::GetComponent(RteComponentInstance* ci, bool matchVersion) const
{
  if (ci->IsApi() || matchVersion) {
    return GetComponent(ci->GetID());
  }
  return FindComponent(ci->GetComponentID(false));
}


RteApi* RteModel::GetApi(const map<string, string>& componentAttributes) const
{
  RteApi* api = nullptr;
  for (auto [_, a] : m_apiList) {
    if(a && a->MatchApiAttributes(componentAttributes)) {
      if(a->GetPackage()->IsDominating()) {
        return a; // first dominating API wins
      } else if(!api) {
        api = a; // keep latest
      }
    }
  }
  return api;
}

RteApi* RteModel::GetApi(const string& id) const
{
  if (id.find(RteConstants::PREFIX_CVERSION_CHAR) != string::npos) {
    auto it = m_apiList.find(id);
    if (it != m_apiList.end()) {
      RteApi* api = it->second;
      return api;
    }
    return nullptr;
  }
  return GetLatestApi(id);
}

RteApi* RteModel::GetLatestApi(const string& id) const
{
  string commonId = RteUtils::GetPrefix(id, RteConstants::PREFIX_CVERSION_CHAR);
  RteApi* api = nullptr;
  // get highest API version
  for (auto [key, a] : m_apiList) {
    if (RteUtils::GetPrefix(key, RteConstants::PREFIX_CVERSION_CHAR) == commonId) {
      if(a->GetPackage()->IsDominating()) {
        return a; // first dominating API wins
      } else if(!api) {
        api = a; // keep latest
      }
    }
  }
  return api;
}

std::list<RteApi*> RteModel::GetAvailableApis(const std::string& id) const
{
  std::list<RteApi*> apis;
  string commonId = RteUtils::GetPrefix(id, RteConstants::PREFIX_CVERSION_CHAR);
  for (auto [key, api] : m_apiList) {
    if (RteUtils::GetPrefix(key, RteConstants::PREFIX_CVERSION_CHAR) == commonId) {
      apis.push_back(api);
    }
  }
  return apis;
}

RteBundle* RteModel::GetBundle(const string& id) const
{
  auto it = m_bundles.find(id);
  if (it != m_bundles.end())
    return it->second;
  return nullptr;
}

RteBundle* RteModel::GetLatestBundle(const string& id) const
{
  for (auto [_, b] : m_bundles) {
    if (b->GetBundleID(false) == id)
      return b;
  }
  return NULL;
}


RteItem* RteModel::GetTaxonomyItem(const string& id) const
{
  map<string, RteItem*>::const_iterator it = m_taxonomy.find(id);
  if (it != m_taxonomy.end()) {
    return it->second;
  }
  return NULL;
}

const string& RteModel::GetTaxonomyDescription(const string& id) const
{
  RteItem* t = GetTaxonomyItem(id);
  if (t) {
    return t->GetText();
  }
  return EMPTY_STRING;
}


string RteModel::GetTaxonomyDoc(const string& id) const
{
  RteItem* t = GetTaxonomyItem(id);
  if (t) {
    return t->GetDocFile();
  }
  return EMPTY_STRING;
}


RteCondition* RteModel::GetCondition(const string& packageId, const string& conditionId) const
{
  RtePackage* package = GetPackage(packageId);
  if (package)
    return package->GetCondition(conditionId);
  return NULL;
}

void RteModel::Construct()
{
}

void RteModel::InsertPacks(const list<RtePackage*>& packs)
{
  for (auto pack : packs) {
    InsertPack(dynamic_cast<RtePackage*>(pack));
  }
  FillComponentList(nullptr); // no device package yet
  FillDeviceTree();
}

void RteModel::InsertPack(RtePackage* package)
{
  if (!package) {
    return;
  }
  if (package->GetPackageState() == PackageState::PS_UNKNOWN) {
    package->SetPackageState(GetPackageState());
  }
  // check for duplicates
  const string& id = package->GetID();
  RtePackage* insertedPack = GetPackage(id);
  if (insertedPack) {
    if(insertedPack == package) {
      return; // already inserted
    }
    string pdscPath = RteFsUtils::MakePathCanonical(package->GetAbsolutePackagePath());
    if (pdscPath.find(m_rtePath) == 0) { // regular installed pack => error
    // duplicate, kept it in a temporary collection till validate;
      m_packageDuplicates.push_back(package);
      return;
    }
    string insertedPdscPath = RteFsUtils::MakePathCanonical(insertedPack->GetAbsolutePackagePath());
    if (insertedPdscPath.find(m_rtePath) == string::npos) { // inserted pack is also from outside => error
      m_packageDuplicates.push_back(package);
      return;
    }
  }

  // OK, add to normal children, but do not forget to remove before deletion!
  AddItem(package);
  // add to general sorted package map
  m_packages[id] = package;
  // add to latest package map
  const string& commonId = package->GetCommonID();
  RtePackage* p = GetLatestPackage(commonId);
  if (!p || p == insertedPack || VersionCmp::Compare(package->GetVersionString(), p->GetVersionString()) > 0) {
    m_latestPackages[commonId] = package;
  }
  if (insertedPack) {
    // remove inserted pack as replaced
    RemoveItem(insertedPack);
  }
}


bool RteModel::Validate()
{
  m_bValid = RteItem::Validate();
  if (!m_errors.empty())
    m_bValid = false;
  if (!m_packageDuplicates.empty()) {
    m_bValid = false;
    for (auto duplicate : m_packageDuplicates) {
      RtePackage* orig = GetPackage(duplicate->GetID());

      string msg = duplicate->GetPackageFileName();
      msg += ": warning #500: pack '" + duplicate->GetID() + "' is already defined in file " + orig->GetPackageFileName();
      msg += " - duplicate is ignored";
      m_errors.push_back(msg);
    }

    m_packageDuplicates.clear();
  }
  return m_bValid;
}

RtePackage* RteModel::FilterModel(RteModel* globalModel, RtePackage* devicePackage)
{
  Clear();

  // first add all latest packs
  set<string> latestPackIds;
  const RtePackageMap& latestPackages = globalModel->GetLatestPackages();
  for ( auto& [key, pack] : latestPackages) {
    if (pack) {
      latestPackIds.insert(pack->GetID());
    }
  }
  m_packageFilter.SetLatestInstalledPacks(latestPackIds); // filter requires global latests

  const RtePackageMap& allPacks = globalModel->GetPackages();
  for (auto [id, pack] : allPacks) {
    if (!m_packageFilter.IsPackageFiltered(pack))
      continue;

    if (m_packageFilter.IsPackageSelected(pack)) {
      m_packages[id] = pack; //directly add to map
    }
    const string& commonId = pack->GetCommonID();
    RtePackage* p = GetLatestPackage(commonId);
    if (!p || VersionCmp::Compare(pack->GetVersionString(), p->GetVersionString()) > 0) {
      m_latestPackages[commonId] = pack;
    }
  }
  if (devicePackage && !m_packageFilter.IsPackageFiltered(devicePackage)) {
    const string& commonId = devicePackage->GetCommonID();
    devicePackage = GetLatestPackage(commonId);
  }

  // add latest packs that pass filter, but not added yet
  for (auto [id, pack] : m_latestPackages) {
    const string& packId = pack->GetID();
    if (m_packages.find(packId) == m_packages.end())
      m_packages[id] = pack;
  }

  FillComponentList(devicePackage);
  FillDeviceTree();
  return devicePackage; // now effective
}

void RteModel::AddItemsFromPack(RtePackage* pack)
{
  RteItem* taxonomy = pack->GetTaxonomy();
  if (taxonomy) {
    // fill in taxonomy
    for (auto t : taxonomy->GetChildren()) {
      string id = t->GetTaxonomyDescriptionID();
      if (GetTaxonomyItem(id) == NULL && IsFiltered(t)) {// do not overwrite the newest
        m_taxonomy[id] = t;
      }
    }
  }
  // collect layers and templates
  auto& csolutionChildren = pack->GetGrandChildren("csolution");
  AddPackItemsToList(csolutionChildren, m_layerDescriptors, "clayer");
  AddPackItemsToList(csolutionChildren, m_templateDescriptors, "template");

  // fill api and component list
  pack->InsertInModel(this);
}

void RteModel::AddPackItemsToList(const Collection<RteItem*>& srcCollection, Collection<RteItem*>& dstCollection, const std::string& tag)
{
  for (auto item : srcCollection) {
    if ((tag.empty() || tag == item->GetTag()) && IsFiltered(item)) {
      dstCollection.push_back(item);
    }
  }
}

bool RteModel::IsFiltered(RteItem* item) {
  if (!item)
    return false;
  if (!GetFilterContext())
    return true;
  if (item->Evaluate(GetFilterContext()) < FULFILLED)
    return false;
  return true;
}


void RteModel::FillComponentList(RtePackage* devicePackage)
{
  m_componentList.clear();
  m_taxonomy.clear();
  m_apiList.clear();

  // first process DFP - it has precedence
  if (devicePackage) {
    AddItemsFromPack(devicePackage);
  }

  // evaluate dominate packages first
  for (auto [id, package] : m_packages) {
    if (package == devicePackage)
      continue;
    if (package->IsDeprecated())
      continue;
    if (package->IsDominating()) {
      AddItemsFromPack(package);
    }
  }

  // evaluated sorted collection, deprecated packs in the second run
  bool bHasDeprecated = false;
  for (auto [id, package] : m_packages) {
    if (package->IsDeprecated()) {
      bHasDeprecated = true;
      continue;
    }
    if (package->IsDominating())
      continue;
    if (package == devicePackage)
      continue;
    AddItemsFromPack(package);
  }
  if (!bHasDeprecated)
    return;
  for (auto [id, package] : m_packages) {
    if (!package->IsDeprecated())
      continue;
    if (package == devicePackage)
      continue;
    AddItemsFromPack(package);
  }

}

void RteModel::InsertComponent(RteComponent* c)
{
  if (!c)
    return;
  const string& id = c->GetID();
  if (c->IsApi()) {
    RteApi* a = dynamic_cast<RteApi*>(c);
    if (IsFiltered(a) && IsApiDominatingOrNewer(a)) {
      m_apiList[id] = a;
    }
  } else {
    // do not allow duplicates (filtering is performed later)
    if (GetComponent(id))
      return;
    m_componentList[id] = c;
  }
}

bool RteModel::IsApiDominatingOrNewer(RteApi* a) {
  const string& id = a->GetID();
  RteApi* aExisting = GetApi(id);
  if (!aExisting)
    return true;
  bool existingDominating = aExisting->GetPackage()->IsDominating();
  bool packageDominating = a->GetPackage()->IsDominating();
  if (packageDominating && !existingDominating)
    return true; // use new api anyway since it is dominating
  if (!packageDominating && existingDominating)
    return false;
  return VersionCmp::Compare(aExisting->GetVersionString(), a->GetVersionString()) < 0;
}


void RteModel::InsertBundle(RteBundle* b)
{
  if (!b)
    return;
  const string& id = b->GetID();
  RteBundle* inserted = GetBundle(id);
  if (inserted) {

    // insert bundle with most information (description and doc)
    if (b->GetDocFile().empty() && b->GetDescription().empty())
      return;

    if (!inserted->GetDocFile().empty()) {
      // ensure bundle has description
      if (inserted->GetDescription().empty())
        inserted->SetText(b->GetDescription());
      return;
    }
    if (b->GetDescription().empty())
      b->SetText(inserted->GetDescription());
  }
  m_bundles[id] = b;
}

RteDeviceVendor* RteModel::FindDeviceVendor(const string& vendor) const
{
  auto it = m_deviceVendors.find(vendor);
  if (it != m_deviceVendors.end())
    return it->second;
  return NULL;

}

RteDeviceVendor* RteModel::EnsureDeviceVendor(const string& vendor)
{
  if (vendor.empty())
    return NULL;
  RteDeviceVendor* dv = FindDeviceVendor(vendor);
  if (dv)
    return dv;
  dv = new RteDeviceVendor(vendor);
  m_deviceVendors[vendor] = dv;
  return dv;
}

bool RteModel::AddDeviceItem(RteDeviceItem* item)
{
  if (!item)
    return false;
  string vendor = item->GetVendorName();
  RteDeviceVendor* dv = EnsureDeviceVendor(vendor);
  return dv->AddDeviceItem(item);
}


void RteModel::GetDevices(list<RteDevice*>& devices, const string& namePattern, const string& vendor, RteDeviceItem::TYPE depth) const
{
  if (namePattern.empty() || namePattern.find_first_of("*?[") != string::npos) {
    if (IsUseDeviceTree()) {
      m_deviceTree->GetDevices(devices, namePattern, vendor, depth); // pattern match
      return;
    }
    for (auto [_, dv] : m_deviceVendors) {
      dv->GetDevices(devices, namePattern);
    }
    return;
  }
  RteDevice* d = GetDevice(namePattern, vendor);
  if (d)
    devices.push_back(d);
}

RteDevice* RteModel::GetDevice(const string& deviceName, const string& vendor) const
{
  if (!vendor.empty()) {
    string vendorName = DeviceVendor::GetCanonicalVendorName(vendor);
    RteDeviceVendor* dv = FindDeviceVendor(vendorName);
    if (dv) {
      RteDevice* d = dv->GetDevice(deviceName);
      if (d)
        return d;

    }
  } else {
    for (auto [_, dv] : m_deviceVendors) {
      RteDevice* d = dv->GetDevice(deviceName);
      if (d)
        return d;
    }
  }
  if (IsUseDeviceTree())
    return dynamic_cast<RteDevice*>(m_deviceTree->GetDeviceItem(deviceName, vendor));
  return NULL;
}

int RteModel::GetDeviceCount() const
{
  int count = 0;
  for (auto [_, dv] : m_deviceVendors) {
    count += dv->GetCount();
  }
  return count;

}
int RteModel::GetDeviceCount(const string& vendor) const
{
  RteDeviceVendor* dv = FindDeviceVendor(vendor);
  if (dv)
    return dv->GetCount();
  return 0;
}


RteDeviceItemAggregate* RteModel::GetDeviceAggregate(const string& deviceName, const string& vendor) const {
  return (m_deviceTree->GetDeviceAggregate(deviceName, vendor));
}

RteDeviceItemAggregate* RteModel::GetDeviceItemAggregate(const string& name, const string& vendor) const {
  return (m_deviceTree->GetDeviceItemAggregate(name, vendor));
}

// fill device and board information
void RteModel::FillDeviceTree()
{
  ClearDevices();

  bool bHasDeprecated = false;
  // use only latest packages
  for (auto [id, package] : m_latestPackages) {
    if (!package)
      continue;
    if (package->IsDeprecated()) {
      bHasDeprecated = true;
      continue;
    }
    FillDeviceTree(package);
  }

  if (!bHasDeprecated)
    return;
  for (auto [id, package] :  m_latestPackages) {
    if (!package)
      continue;
    if (!package->IsDeprecated()) {
      continue;
    }
    FillDeviceTree(package);
  }
}

void RteModel::FillDeviceTree(RtePackage* package)
{
  RteDeviceFamilyContainer* families = package->GetDeviceFamiles();
  if (families) {
    for (auto child : families->GetChildren()) {
      RteDeviceFamily* fam = dynamic_cast<RteDeviceFamily*>(child);
      if (fam) {
        bool bInserted = AddDeviceItem(fam);
        if (!bInserted)
          continue;
        if (!IsUseDeviceTree()) // additionally add device info as a tree
          continue;
        m_deviceTree->AddDeviceItem(fam);
      }
    }
  }
  RteItem* boards = package->GetBoards();
  if (boards) {
    for (auto child : boards->GetChildren()) {
      RteBoard* b = dynamic_cast<RteBoard*>(child);
      if (b == 0)
        continue;
      const string& id = b->GetID();
      if (m_boards.find(id) == m_boards.end()) {
        m_boards[id] = b;
      }
    }
  }
}


void RteModel::GetBoardBooks(map<string, string>& books, const string& device, const string& vendor) const
{
  if (m_boards.empty())
    return;
  RteDevice* d = GetDevice(device, vendor);
  if (!d)
    return;
  XmlItem ea;
  d->GetEffectiveAttributes(ea);
  GetBoardBooks(books, ea.GetAttributes());
}

void RteModel::GetBoardBooks(map<string, string>& books, const map<string, string>& deviceAttributes) const
{
  if (m_boards.empty())
    return;
  for (auto [_, b] : m_boards) {
    if (b->HasCompatibleDevice(deviceAttributes)) {
      b->GetBooks(books);
    }
  }
}

RteGlobalModel::RteGlobalModel() :
  RteModel(NULL, PackageState::PS_INSTALLED),
  m_packRegistry(new RtePackRegistry()),
  m_nActiveProjectId(-1)
{
}
RteGlobalModel::~RteGlobalModel()
{
  RteGlobalModel::Clear();
  delete m_packRegistry;
}

void RteGlobalModel::Clear()
{
  ClearProjects();
  ClearModel();
}

void RteGlobalModel::ClearModel()
{
  ClearProjectTargets();
  RteModel::ClearModel();
  m_packRegistry->Clear();
}


void RteGlobalModel::SetCallback(RteCallback* callback)
{
  RteModel::SetCallback(callback);
  for (auto [n, project] : m_projects) {
    project->SetCallback(callback);
  }
}

// projects
////////////////////////////////////////////////

int RteGlobalModel::GenerateProjectId()
{
  int id = 1;
  int n = (int)(m_projects.size() + 2);
  for (; id < n ; id++) {
    if (!GetProject(id))
      break;
  }
  return id;
}

RteProject* RteGlobalModel::GetProject(int id) const
{
  auto it = m_projects.find(id);
  if (it != m_projects.end())
    return it->second;
  return nullptr;
}

RteProject* RteGlobalModel::AddProject(int id, RteProject* project)
{
  if (id <= 0) {
    id = GenerateProjectId();
  }
  if (!project) {
    project = GetProject(id);
    if (!project) {
      project = new RteProject();
    }
  }
  project->SetProjectId(id);
  project->SetModel(this);
  project->SetCallback(m_callback);
  m_projects[id] = project;
  return project;
}

void RteGlobalModel::DeleteProject(int id)
{
  auto it = m_projects.find(id);
  if (it != m_projects.end()) {
    delete it->second;
    m_projects.erase(it);
    if (id == m_nActiveProjectId)
      m_nActiveProjectId = -1;
  }
}

void RteGlobalModel::ClearProjects()
{
  for (auto [n, project] : m_projects) {
    delete project;
  }
  m_projects.clear();
  m_nActiveProjectId = -1;
}


void RteGlobalModel::ClearProjectTargets(int id)
{
  for (auto [n, project] : m_projects) {
    if (id <= 0 || id == n) {
      project->ClearTargets();
    }
  }
}

// End of RteModel.cpp
