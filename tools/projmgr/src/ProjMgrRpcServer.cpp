/*
 * Copyright (c) 2025-2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgrRpcServer.h"
#include "ProjMgrRpcServerData.h"
#include "ProjMgrLogger.h"
#include "ProjMgr.h"
#include "ProductInfo.h"

#include "CrossPlatformUtils.h"
#include "RteFsUtils.h"
#include "CollectionUtils.h"

#include <fstream>
#include <iostream>
#include <regex>

using namespace std;

static constexpr const char* CONTENT_LENGTH_HEADER = "Content-Length:";

ProjMgrRpcServer::ProjMgrRpcServer(ProjMgr& manager) :
  m_manager(manager) {
}

ProjMgrRpcServer::~ProjMgrRpcServer(void) {
  // Reserved
}

const string ProjMgrRpcServer::GetRequestFromStdinWithLength(void) {
  string line;
  int contentLength = 0;
  const string& header = CONTENT_LENGTH_HEADER;
  while(getline(cin, line) && !cin.fail()) {
    if(line.find(header) == 0) {
      contentLength = RteUtils::StringToInt(line.substr(header.length()), 0);
    }
    if(line.empty() || line.front() == '\r' || line.front() == '\n') {
      break;
    }
  }
  string request(contentLength, '\0');
  cin.read(&request[0], contentLength);
  return request;
}

const string ProjMgrRpcServer::GetRequestFromStdin(void) {
  string jsonData;
  int braces = 0;
  bool inJson = false;
  char c;
  while(cin.get(c) && !cin.fail()) {
    if(c == '{') {
      braces++;
      inJson = true;
    }
    if(c == '}') {
      braces--;
    }
    if(inJson) {
      jsonData += c;
    }
    if(inJson && braces == 0) {
      break;
    }
  }
  return jsonData;
}

class RpcHandler : public RpcMethods {
public:
  RpcHandler(ProjMgrRpcServer& server, JsonRpc2Server& jsonServer) : RpcMethods(jsonServer),
    m_server(server),
    m_manager(server.GetManager()),
    m_worker(server.GetManager().GetWorker()) {}

  RpcArgs::GetVersionResult GetVersion(void) override;
  RpcArgs::SuccessResult Shutdown(void) override;
  RpcArgs::SuccessResult Apply(const string& context) override;
  RpcArgs::SuccessResult Resolve(const string& context) override;
  RpcArgs::SuccessResult LoadPacks(void) override;
  RpcArgs::SuccessResult LoadSolution(const string& solution, const string& activeTarget) override;
  RpcArgs::ContextInfo GetContextInfo(const string& context) override;
  RpcArgs::UsedItems GetUsedItems(const string& context) override;
  RpcArgs::PacksInfo GetPacksInfo(const string& context, const bool& all) override;
  RpcArgs::SuccessResult SelectPack(const string& context, const RpcArgs::PackReference& pack) override;
  RpcArgs::VariablesResult GetVariables(const string& context) override;
  RpcArgs::DeviceList GetDeviceList(const string& context, const string& namePattern, const string& vendor) override;
  RpcArgs::DeviceInfo GetDeviceInfo(const string& id) override;
  RpcArgs::BoardList GetBoardList(const string& context, const string& namePattern, const string& vendor) override;
  RpcArgs::BoardInfo GetBoardInfo(const string& id) override;
  RpcArgs::CtRoot GetComponentsTree(const string& context, const bool& all) override;
  RpcArgs::SuccessResult SelectComponent(const string& context, const string& id, const int& count, const RpcArgs::Options& options) override;
  RpcArgs::SuccessResult SelectVariant(const string& context, const string& aggregateId, const string& variantName) override;
  RpcArgs::SuccessResult SelectBundle(const string& context, const string& className, const string& bundleName) override;
  RpcArgs::Results ValidateComponents(const string& context) override;
  RpcArgs::LogMessages GetLogMessages(void) override;
  RpcArgs::DraftProjectsInfo GetDraftProjects(const RpcArgs::DraftProjectsFilter& filter) override;
  RpcArgs::ConvertSolutionResult ConvertSolution(const string& solution, const string& activeTarget, const bool& updateRte) override;
  RpcArgs::DiscoverLayersInfo DiscoverLayers(const string& solution, const string& activeTarget) override;
  RpcArgs::ListMissingPacksResult ListMissingPacks(const string& solution, const string& activeTarget) override;

protected:
  enum Exception
  {
    SOLUTION_NOT_FOUND = -1,
    SOLUTION_NOT_VALID = -2,
    SOLUTION_NOT_LOADED = -3,
    CONTEXT_NOT_FOUND = -4,
    CONTEXT_NOT_VALID = -5,
    COMPONENT_NOT_FOUND = -6,
    COMPONENT_NOT_RESOLVED = -7,
    PACKS_NOT_LOADED = -8,
    PACKS_LOADING_FAIL = -9,
    RTE_MODEL_ERROR = -10,
  };

  ProjMgrRpcServer& m_server;
  ProjMgr& m_manager;
  ProjMgrWorker& m_worker;
  bool m_packsLoaded = false;
  bool m_solutionLoaded = false;
  bool m_bUseAllPacks = false;

  map<std::string, PackReferenceVector> m_packReferences; // packsInfo is used to simplify creation and access to references

  void StoreSelectedComponents(RteTarget* rteTarget, map<RteComponent*, int>& selectedComponents);
  PackReferenceVector& GetPackReferences(const string& context);
  PackReferenceVector CollectPackReferences(const string& context);
  PackReferenceVector GetPackReferencesForPack(const string& context, const string& packId);
  RpcArgs::PackReference& EnsurePackReferenceForPack(const string& context, const string& packId, const string& origin, bool bVersion);
  RpcArgs::PackReference& EnsurePackReference(const string& context, const RpcArgs::PackReference& packRef);


  void UpdateFilter(const string& context, RteTarget* rteTarget, bool bAll); // returns true if changed
  const ContextItem& GetContext(const string& context) const;
  RteTarget* GetActiveTarget(const string& context) const;
  RteComponentAggregate* GetComponentAggregate(const string& context, const string& id) const;
  bool SelectVariantOrVersion(const string& context, const string& id, const string& value, bool bVariant);
  bool CheckSolutionArg(string& solution, optional<string>& message) const;
};

bool ProjMgrRpcServer::Run(void) {
  JsonRpc2Server jsonServer;
  RpcHandler handler(*this, jsonServer);

  while(!m_shutdown && !cin.fail()) {
    // Get request
    const auto request = m_contextLength ?
      GetRequestFromStdinWithLength() :
      GetRequestFromStdin();

    if(request.empty()) {
      continue;
    }

    ofstream log;
    if(m_debug) {
      log.open(RteFsUtils::GetCurrentFolder(true) + "csolution-rpc-log.txt", fstream::app);
      log << request << std::endl;
    }

    // Handle request
    const auto response = jsonServer.HandleRequest(request);

    // Send response
    if(m_contextLength) {
      // compliant to https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#baseProtocol
      cout << CONTENT_LENGTH_HEADER << response.size() <<
        CrossPlatformUtils::Crlf() << CrossPlatformUtils::Crlf() << response << std::flush;
    } else {
      cout << response << std::endl;
    }

    if(m_debug) {
      log << response << std::endl;
      log.close();
    }

  }
  return true;
}

bool RpcHandler::CheckSolutionArg(string& solution, optional<string>& message) const {
  if(!regex_match(solution, regex(".*\\.csolution\\.(yml|yaml)"))) {
    message = solution + " is not a *.csolution.yml file";
    return false;
  }
  solution = RteFsUtils::MakePathCanonical(solution);
  if(!RteFsUtils::Exists(solution)) {
    message = solution + " file does not exist";
    return false;
  }
  return true;
}

const ContextItem& RpcHandler::GetContext(const string& context) const {
  if(!m_solutionLoaded) {
    throw JsonRpcException(SOLUTION_NOT_LOADED, "a valid solution must be loaded before proceeding");
  }
  if(context.empty()) {
    throw JsonRpcException(CONTEXT_NOT_VALID, "'context' argument cannot be empty");
  }
  const auto selected = m_worker.GetSelectedContexts();
  if(find(selected.begin(), selected.end(), context) == selected.end()) {
    throw JsonRpcException(CONTEXT_NOT_FOUND, context + " was not found among selected contexts");
  }
  map<string, ContextItem>* contexts = nullptr;
  m_worker.GetContexts(contexts);
  return (*contexts)[context];
}

RteTarget* RpcHandler::GetActiveTarget(const string& context) const {
  const auto& contextItem = GetContext(context);
  return contextItem.rteActiveTarget;
}


RteComponentAggregate* RpcHandler::GetComponentAggregate(const string& context, const string& id) const {
  auto rteAggregate = GetContext(context).rteActiveTarget->GetComponentAggregate(id);
  if(!rteAggregate) {
    throw JsonRpcException(COMPONENT_NOT_FOUND, id + ": component not found");
  }
  return rteAggregate;
}

RpcArgs::GetVersionResult RpcHandler::GetVersion(void) {
  RpcArgs::GetVersionResult res = {{true}};
  res.message = string("Running ") + INTERNAL_NAME + " " + VERSION_STRING;
  res.version = VERSION_STRING;
  res.apiVersion = RPC_API_VERSION;
  return res;
}

RpcArgs::SuccessResult RpcHandler::Shutdown(void) {
  m_server.SetShutdown(true);
  return {true, "Shutdown initiated..."};
}

RpcArgs::SuccessResult RpcHandler::Apply(const string& context) {
  RpcArgs::SuccessResult result = {false};
  auto rteProject = GetActiveTarget(context)->GetProject();
  if(rteProject) {
    rteProject->Apply();
    // Apply returns true if list of gpdc files needs to be updated: irrelevant for csolution
    result.success = true;
    // purge unselected packs

    auto& packRefs = GetPackReferences(context);
    packRefs.erase(
      std::remove_if(packRefs.begin(), packRefs.end(),
        [](RpcArgs::PackReference& ref) { return !ref.selected; }),
      packRefs.end()
    );
  }
  return result;
}

RpcArgs::SuccessResult RpcHandler::Resolve(const string& context) {
  RpcArgs::SuccessResult result = {false};
  auto rteTarget = GetActiveTarget(context);
  auto rteProject = rteTarget->GetProject();
  if(rteProject) {
    result.success = rteProject->ResolveDependencies(rteTarget);
  }
  return result;
}


RpcArgs::SuccessResult RpcHandler::LoadPacks(void) {
  RpcArgs::SuccessResult result = {false};
  m_manager.Clear();
  m_solutionLoaded = false;
  m_worker.InitializeModel();
  m_worker.SetLoadPacksPolicy(LoadPacksPolicy::ALL);
  m_packsLoaded = m_worker.LoadAllRelevantPacks();
  m_worker.SetLoadPacksPolicy(LoadPacksPolicy::DEFAULT);
  result.success = m_packsLoaded;
  if(!m_packsLoaded) {
    result.message = "Packs failed to load";
  }
  return result;
}

RpcArgs::SuccessResult RpcHandler::LoadSolution(const string& solution, const string& activeTarget) {
  m_packReferences.clear();
  RpcArgs::SuccessResult result = {false};
  const auto csolutionFile = RteFsUtils::MakePathCanonical(solution);
  if(!regex_match(csolutionFile, regex(".*\\.csolution\\.(yml|yaml)"))) {
    result.message = solution + " is not a *.csolution.yml file";
    return result;
  }
  if(!m_packsLoaded) {
    result.message = "Packs must be loaded before loading solution";
    return result;
  }
  result.success = m_solutionLoaded = m_manager.LoadSolution(csolutionFile, activeTarget);
  if(!m_solutionLoaded) {
    result.message = "failed to load and process solution " + csolutionFile;
  }
  return result;
}

void RpcHandler::StoreSelectedComponents(RteTarget* rteTarget, map<RteComponent*, int>& selectedComponents)
{
  auto& selectedAggregates = rteTarget->CollectSelectedComponentAggregates();
  for(auto [aggregate, count] : selectedAggregates) {
    RteComponent* c = aggregate->GetComponent();
    if(c) {
      // consider only components, instances are added from project anyway
      selectedComponents[c] = count;
    }
  }
}

void RpcHandler::UpdateFilter(const string& context, RteTarget* rteTarget, bool all) {
  if(m_bUseAllPacks == all) {
    return;
  }
  m_bUseAllPacks = all;
  // store selected components, not aggregates: they will be destroyed
  map<RteComponent*, int> selectedComponents;
  StoreSelectedComponents(rteTarget, selectedComponents);

  RtePackageFilter packFilter;
  if(!all) {
    // construct and apply filter
    // use resolved pack ID's from selected references
    set<string> packIds;
    for(auto& ref : GetPackReferences(context)) {
      if(ref.selected && ref.resolvedPack.has_value()) {
        packIds.insert(ref.resolvedPack.value());
      }
    }
    packFilter.SetSelectedPackages(packIds);
    packFilter.SetUseAllPacks(false);
  }
  // only update filter if differs from current state
  if(!packFilter.IsEqual(rteTarget->GetPackageFilter())) {
    rteTarget->SetPackageFilter(packFilter);
    rteTarget->UpdateFilterModel();  // updates available components
    rteTarget->GetProject()->UpdateModel(); // inserts already instantiated components
    // restore selection
    for(auto [c, count] : selectedComponents) {
      rteTarget->SelectComponent(c, count, false, false);
    }
    rteTarget->EvaluateComponentDependencies();
  }
}

RpcArgs::SuccessResult RpcHandler::SelectPack(const string& context, const RpcArgs::PackReference& packRef) {
  RpcArgs::SuccessResult result = {true};
  // find reference if exists, otherwise add new one
  auto& ref = EnsurePackReference(context, packRef);
  ref.selected = packRef.selected;
  if(!ref.resolvedPack.has_value()) {
    // TODO: resolve pack
  }
  return result;
}

RpcArgs::ContextInfo RpcHandler::GetContextInfo(const string& context) {
  RpcArgs::ContextInfo contextInfo;
  RpcDataCollector dc(GetActiveTarget(context));
  contextInfo.success = true;
  dc.CollectUsedComponents(contextInfo.components);
  // get all references, even if they are not selected , because it is useful for client to remove them from files
  contextInfo.packs = GetPackReferences(context);

  auto& contextItem = GetContext(context);
  contextInfo.variables = contextItem.variables;
  contextInfo.attributes = contextItem.targetAttributes;
  contextInfo.pname = contextItem.deviceItem.pname;

  if(contextItem.rteDevice) {
    contextInfo.device = dc.FromRteDevice(contextItem.rteDevice, true);
  } else {
    contextInfo.device.id = contextItem.device;
    contextInfo.success = false;
    contextInfo.message = "No device is found";
  }
  if(contextItem.rteBoard) {
    contextInfo.board = dc.FromRteBoard(contextItem.rteBoard, true);
  }
  return contextInfo;
}

RpcArgs::UsedItems RpcHandler::GetUsedItems(const string& context) {
  RpcArgs::UsedItems usedItems;
  RpcDataCollector dc(GetActiveTarget(context));
  dc.CollectUsedComponents(usedItems.components);
  // get all references, even if they are not selected , because it is useful for client to remove them from files
  usedItems.packs = GetPackReferences(context);
  usedItems.success = true;
  return usedItems;
}


PackReferenceVector& RpcHandler::GetPackReferences(const string& context) {
  // emplace returns pair<iterator, bool>
  // return the value from the iterator
  return m_packReferences.emplace(context, RpcHandler::CollectPackReferences(context)).first->second;
}

PackReferenceVector RpcHandler::CollectPackReferences(const string& context) {
  PackReferenceVector packRefs;
  auto contextItem = GetContext(context);
  for(const auto& packItem : contextItem.packRequirements) {
    const auto packId = RtePackage::ComposePackageID(packItem.pack.vendor, packItem.pack.name, packItem.pack.version);

    RpcArgs::PackReference packRef;
    packRef.pack = packItem.selectedBy;
    packRef.resolvedPack = packId;
    packRef.origin = packItem.origin;
    packRef.path = packItem.path;
    packRef.selected = true; // initially pack is selected;
    packRefs.push_back(packRef);
  }
  return packRefs;
}

PackReferenceVector RpcHandler::GetPackReferencesForPack(const string& context, const string& packId) {
  PackReferenceVector packRefs;
  for(auto& ref : GetPackReferences(context)) {
    if(ref.resolvedPack.has_value() && ref.resolvedPack == packId) {
      packRefs.push_back(ref);
    }
  }
  return packRefs;
}

RpcArgs::PackReference& RpcHandler::EnsurePackReferenceForPack(const string& context, const string& packId, const string& origin, bool bExplicitVersion) {
  auto& packRefs = GetPackReferences(context);
  for(auto& ref : packRefs) {
    if(ref.resolvedPack.has_value() && ref.resolvedPack == packId &&
       (origin.empty() || RteFsUtils::Equivalent(ref.origin, origin))) {
      ref.selected = true; // ensure selected
      return ref;
    }
  }
  // no reference->add one to list of all references
  RpcArgs::PackReference newRef;
  newRef.pack = bExplicitVersion ? packId : RtePackage::CommonIdFromId(packId);
  newRef.resolvedPack = packId;
  if(!origin.empty()) {
    newRef.origin = origin;
  }
  newRef.selected = true;
  return packRefs.emplace_back(newRef);
}


RpcArgs::PackReference& RpcHandler::EnsurePackReference(const string& context, const RpcArgs::PackReference& packRef) {
  auto& packRefs = GetPackReferences(context);
  for(auto& ref : packRefs) {
    if(ref.pack == packRef.pack &&
      (packRef.origin.empty() || RteFsUtils::Equivalent(ref.origin, packRef.origin)) &&
      (RteFsUtils::Equivalent(ref.path.value_or(""), packRef.path.value_or("")))) {
      return ref;
    }
  }
  // no reference->add supplied
  return packRefs.emplace_back(packRef);
}


RpcArgs::PacksInfo RpcHandler::GetPacksInfo(const string& context, const bool& all) {

  RteTarget* rteTarget = GetActiveTarget(context);

  UpdateFilter(context, rteTarget, all);
  RpcDataCollector dc(GetActiveTarget(context));
  auto usedPacks = dc.GetUsedPacks();

  RpcArgs::PacksInfo packsInfo;
  for(auto& [packId, rtePackage] : rteTarget->GetFilteredModel()->GetPackages()) {
    RpcArgs::Pack p;
    p.id = rtePackage->GetPackageID(true);
    const auto& description = rtePackage->GetDescription();
    if(!description.empty()) {
      p.description = description;
    }
    auto overview = rtePackage->GetDocFile();
    if(!overview.empty()) {
      p.doc = overview;
    }
    if(contains_key(usedPacks, p.id)) {
      p.used = true;
    }
    auto packRefs = GetPackReferencesForPack(context, p.id);

    if(!packRefs.empty()) {
      p.references = packRefs;
    }
    packsInfo.packs.push_back(p);
  }
  // TODO: add unresolved packs from unresolved references

  packsInfo.success = true;
  return packsInfo;
}

RpcArgs::VariablesResult RpcHandler::GetVariables(const string& context) {
  RpcArgs::VariablesResult res;
  res.success = false;
  auto& contextItem = GetContext(context);
  res.variables = contextItem.variables;
  res.success = true;
  return res;
}

RpcArgs::DeviceList RpcHandler::GetDeviceList(const string& context, const string& namePattern, const string& vendor)
{
  RpcArgs::DeviceList deviceList{{false}};
  if(!m_packsLoaded) {
    deviceList.message = "Packs must be loaded before accessing device info";
  } else {
    RteTarget* rteTarget = context.empty() ? nullptr : GetActiveTarget(context);
    RteModel* rteModel = rteTarget ? rteTarget->GetFilteredModel() : ProjMgrKernel::Get()->GetGlobalModel();
    RpcDataCollector dc(rteTarget, rteModel);
    dc.CollectDeviceList(deviceList, namePattern, vendor);
    deviceList.success = true;
  }
  return deviceList;
}

RpcArgs::DeviceInfo RpcHandler::GetDeviceInfo(const string& id)
{
  RpcArgs::DeviceInfo deviceInfo{{false}};
  if(!m_packsLoaded) {
    deviceInfo.message = "Packs must be loaded before accessing device info";
  } else {
    RpcDataCollector dc(nullptr, ProjMgrKernel::Get()->GetGlobalModel());
    dc.CollectDeviceInfo(deviceInfo, id);
  }
  return deviceInfo;
}

RpcArgs::BoardList RpcHandler::GetBoardList(const string& context, const string& namePattern, const string& vendor)
{
  RpcArgs::BoardList boardList{{false}};
  if(!m_packsLoaded) {
    boardList.message = "Packs must be loaded before accessing board info";
  } else {
    RteTarget* rteTarget = context.empty() ? nullptr : GetActiveTarget(context);
    RteModel* rteModel = rteTarget ? rteTarget->GetFilteredModel() : ProjMgrKernel::Get()->GetGlobalModel();
    RpcDataCollector dc(rteTarget, rteModel);
    dc.CollectBoardList(boardList, namePattern, vendor);
    boardList.success = true;
  }
  return boardList;

}

RpcArgs::BoardInfo RpcHandler::GetBoardInfo(const string& id)
{
  RpcArgs::BoardInfo boardInfo{{false}};
  if(!m_packsLoaded) {
    boardInfo.message = "Packs must be loaded before accessing board info";
  } else {
    RpcDataCollector dc(nullptr, ProjMgrKernel::Get()->GetGlobalModel());
    dc.CollectBoardInfo(boardInfo, id);
  }
  return boardInfo;
}


RpcArgs::CtRoot RpcHandler::GetComponentsTree(const string& context, const bool& all) {
  RteTarget* rteTarget = GetActiveTarget(context);
  UpdateFilter(context, rteTarget, all);

  RpcDataCollector dc(rteTarget);
  RpcArgs::CtRoot ctRoot;
  dc.CollectCtClasses(ctRoot);
  ctRoot.success = true;
  return ctRoot;
}

RpcArgs::SuccessResult RpcHandler::SelectComponent(const string& context, const string& id, const int& count, const RpcArgs::Options& options) {
  // first try full component ID
  RteTarget* activeTarget = GetActiveTarget(context);
  RteComponent* rteComponent = activeTarget->GetComponent(id);
  RteComponentAggregate* rteAggregate = nullptr;
  RpcArgs::SuccessResult result = {false};
  if(rteComponent) {
    result.success = GetActiveTarget(context)->SelectComponent(rteComponent, count, true);
    rteAggregate = activeTarget->GetComponentAggregate(rteComponent);
  } else {
    rteAggregate = GetComponentAggregate(context, id);
    result.success = activeTarget->SelectComponent(rteAggregate, count, true);
    rteComponent = rteAggregate->GetComponent();
  }
  // set options
  auto& layer = options.layer.has_value() ? options.layer.value() : RteUtils::EMPTY_STRING;
  rteAggregate->AddAttribute("layer", layer, false);
  bool explicitVendor = options.explicitVendor.has_value() ? options.explicitVendor.value() : false;
  rteAggregate->AddAttribute("explicitVendor", explicitVendor ? "1" : "", false);

  auto& explicitVersion = options.explicitVersion.has_value() ? options.explicitVersion.value() : RteUtils::EMPTY_STRING;
  // TODO: check if version is plausible
  rteAggregate->AddAttribute("explicitVersion", explicitVersion, false);

  // ensure Pack reference is added when component is selected
  if(count > 0 && rteComponent) {
    auto& packId = rteComponent->GetPackage()->GetID();
    EnsurePackReferenceForPack(context, packId, layer, !explicitVersion.empty());
  }
  return result;
}

RpcArgs::SuccessResult RpcHandler::SelectVariant(const string& context, const string& id, const string& variant) {
  RpcArgs::SuccessResult result = {false};
  RteComponentAggregate* rteAggregate = GetComponentAggregate(context, id);
  if(rteAggregate->GetSelectedVariant() == variant) {
    return result;
  }

  auto availableVariants = rteAggregate->GetVariants();
  if(std::find(availableVariants.begin(), availableVariants.end(), variant) == availableVariants.end()) {
    result.message = "Variant '" + variant + "' is not found for component " + id;
    return result;
  }

  rteAggregate->SetSelectedVariant(variant);
  if(rteAggregate->IsSelected()) {
    GetActiveTarget(context)->EvaluateComponentDependencies();
  }
  result.success = true;
  return result;
}

bool RpcHandler::SelectVariantOrVersion(const string& context, const string& id, const string& value, bool bVariant) {
  RteComponentAggregate* rteAggregate = GetComponentAggregate(context, id);

  auto& selectedValue = bVariant ? rteAggregate->GetSelectedVariant() : rteAggregate->GetSelectedVersion();
  if(selectedValue == value) {
    return false;
  }

  if(bVariant || !value.empty()) {
    auto availableValues = bVariant ? rteAggregate->GetVariants() : rteAggregate->GetVersions(rteAggregate->GetSelectedVariant());
    if(std::find(availableValues.begin(), availableValues.end(), value) == availableValues.end()) {
      return false;
    }
  }
  if(bVariant) {
    rteAggregate->SetSelectedVariant(value);
  } else {
    rteAggregate->SetSelectedVersion(value);
  }
  if(rteAggregate->IsSelected()) {
    GetActiveTarget(context)->EvaluateComponentDependencies();
  }
  return true;
}


RpcArgs::SuccessResult RpcHandler::SelectBundle(const string& context, const string& className, const string& bundleName) {
  RpcArgs::SuccessResult result = {false};
  RteTarget* rteTarget = GetActiveTarget(context);
  RteComponentClass* rteClass = rteTarget->GetComponentClass(className);
  if(!rteClass) {
    throw JsonRpcException(COMPONENT_NOT_FOUND, className + ": component class not found");
  }
  if(rteClass->GetSelectedBundleName() == bundleName) {
    return result; // no change => false
  }
  if(!contains_key(rteClass->GetBundleNames(), bundleName)) {
    result.message = "Bundle '" + bundleName + "' is not found for component class '" + className + "'";
    return result; // error => false
  }
  rteClass->SetSelectedBundleName(bundleName, true);
  GetActiveTarget(context)->EvaluateComponentDependencies();
  result.success = true;
  return result;
}


RpcArgs::Results RpcHandler::ValidateComponents(const string& context) {
  auto contextItem = GetContext(context);

  RpcArgs::Results results;
  auto validationRes = m_worker.ValidateContext(contextItem);
  results.result = RteItem::ConditionResultToString(validationRes);
  if(validationRes < RteItem::ConditionResult::FULFILLED) {
    results.validation = vector<RpcArgs::Result>{};
    for(const auto& validation : contextItem.validationResults) {
      RpcArgs::Result r;
      r.result = RteItem::ConditionResultToString(validation.result);
      r.id = validation.id;
      if(!validation.aggregates.empty()) {
        r.aggregates = vector<string>(validation.aggregates.begin(), validation.aggregates.end());
      }
      if(!validation.conditions.empty()) {
        RpcArgs::Condition c;
        r.conditions = vector<RpcArgs::Condition>{};
        for(const auto& condition : validation.conditions) {
          c.expression = condition.expression;
          if(!condition.aggregates.empty()) {
            c.aggregates = vector<string>(condition.aggregates.begin(), condition.aggregates.end());
          }
          r.conditions->push_back(c);
        }
      }
      results.validation->push_back(r);
    }
  }
  results.success = true;
  return results;
}

RpcArgs::LogMessages RpcHandler::GetLogMessages(void) {
  StrVec infoVec;
  for(const auto& [_, info] : ProjMgrLogger::Get().GetInfos()) {
    for(const auto& msg : info) {
      CollectionUtils::PushBackUniquely(infoVec, msg);
    }
  }
  StrVec errorsVec;
  for(const auto& [_, errors] : ProjMgrLogger::Get().GetErrors()) {
    for(const auto& msg : errors) {
      CollectionUtils::PushBackUniquely(errorsVec, msg);
    }
  }
  StrVec warningsVec;
  for(const auto& [_, warnings] : ProjMgrLogger::Get().GetWarns()) {
    for(const auto& msg : warnings) {
      CollectionUtils::PushBackUniquely(warningsVec, msg);
    }
  }
  RpcArgs::LogMessages messages;
  if(!infoVec.empty()) {
    messages.info = infoVec;
  }
  if(!errorsVec.empty()) {
    messages.errors = errorsVec;
  }
  if(!warningsVec.empty()) {
    messages.warnings = warningsVec;
  }
  messages.success = true;
  return messages;
}

RpcArgs::DraftProjectsInfo RpcHandler::GetDraftProjects(const RpcArgs::DraftProjectsFilter& filter) {
  RpcArgs::DraftProjectsInfo applications;
  applications.success = false;

  if(!m_packsLoaded) {
    applications.message = "Packs must be loaded before retrieving draft projects";
    return applications;
  }

  // initialize context and target attributes with board and device
  ContextItem context;
  m_worker.InitializeTarget(context);
  if(filter.board.has_value() || filter.device.has_value()) {
    context.board = filter.board.has_value() ? filter.board.value() : "";
    context.device = filter.device.has_value() ? filter.device.value() : "";
    if(!m_worker.ProcessDevice(context, BoardOrDevice::SkipProcessor)) {
      applications.message = "Board or device processing failed";
      return applications;
    }

    if(contains_key(context.targetAttributes, "Dname") && !contains_key(context.targetAttributes, "Bname")) {
      context.targetAttributes["Bname"] = "";
    }
    m_worker.SetTargetAttributes(context, context.targetAttributes);
  }

  // collect examples, optionally filtered for 'environments'
  vector<RpcArgs::ExampleProject> examples, refApps;
  const auto& environments = filter.environments.has_value() ? filter.environments.value() : StrVec();
  const auto& collectedExamples = m_worker.CollectExamples(context, environments);
  for(const auto& example : collectedExamples) {
    RpcArgs::ExampleProject e;
    e.name = example.name;
    e.pack = example.pack;
    e.doc = example.doc;
    e.description = example.description;
    if(!example.version.empty()) {
      e.version = example.version;
    }
    if(!example.archive.empty()) {
      e.archive = example.archive;
    }
    for(const auto& [name, environment] : example.environments) {
      RpcArgs::ExampleEnvironment env;
      env.name = name;
      env.file = environment.load;
      env.folder = environment.folder;
      e.environments.push_back(env);
    }
    if(!example.components.empty()) {
      e.components = example.components;
    }
    if(!example.categories.empty()) {
      e.categories = example.categories;
    }
    if(!example.keywords.empty()) {
      e.keywords = example.keywords;
    }
    // classify the example as ref-app if it does not specify boards
    auto& ref = example.boards.empty() ? refApps : examples;
    ref.push_back(e);
  }
  if(!examples.empty()) {
    applications.examples = examples;
  }
  if(!refApps.empty()) {
    applications.refApps = refApps;
  }

  // collect templates
  vector<RpcArgs::SolutionTemplate> templates;
  const auto& csolutionTemplates = m_worker.CollectTemplates(context);
  for(const auto& csolutionTemplate : csolutionTemplates) {
    RpcArgs::SolutionTemplate t;
    t.name = csolutionTemplate.name;
    t.pack = csolutionTemplate.pack;
    t.description = csolutionTemplate.description;
    t.file = csolutionTemplate.file;
    t.folder = csolutionTemplate.path;
    if(!csolutionTemplate.copyTo.empty()) {
      t.copyTo = csolutionTemplate.copyTo;
    }
    templates.push_back(t);
  }
  if(!templates.empty()) {
    applications.templates = templates;
  }

  applications.success = true;
  return applications;
}

RpcArgs::ConvertSolutionResult RpcHandler::ConvertSolution(const string& solution, const string& activeTarget, const bool& updateRte) {
  RpcArgs::ConvertSolutionResult result = {{ false }};
  string csolutionFile = solution;
  if(!CheckSolutionArg(csolutionFile, result.message)) {
    return result;
  }
  if(!m_manager.RunConvert(csolutionFile, activeTarget, updateRte) || !ProjMgrLogger::Get().GetErrors().empty()) {
    if(m_worker.HasVarDefineError()) {
      const auto& vars = m_worker.GetUndefLayerVars();
      result.undefinedLayers = StrVec(vars.begin(), vars.end());
      const auto& selectCompiler = m_worker.GetSelectableCompilers();
      if(!selectCompiler.empty()) {
        result.selectCompiler = selectCompiler;
      }
      result.message = "Layer variables undefined, names can be found under 'undefinedLayers'";
    } else if(m_worker.HasCompilerDefineError()) {
      result.selectCompiler = m_worker.GetSelectableCompilers();
      result.message = "Compiler undefined, selectable values can be found under 'selectCompiler'";
    } else {
      result.message = "Convert solution failed, see log messages";
    }
    return result;
  }
  result.success = true;
  return result;
}

RpcArgs::DiscoverLayersInfo RpcHandler::DiscoverLayers(const string& solution, const string& activeTarget) {
  RpcArgs::DiscoverLayersInfo result = {{ false }};
  string csolutionFile = solution;
  if(!CheckSolutionArg(csolutionFile, result.message)) {
    return result;
  }
  if(!m_manager.SetupContexts(csolutionFile, activeTarget)) {
    result.message = "Setup of solution contexts failed";
    return result;
  }
  m_worker.SetUpCommand(true);
  StrVec layers;
  StrSet fails;
  if(!m_worker.ListLayers(layers, "", fails) || !m_worker.ElaborateVariablesConfigurations()) {
    result.message = "No compatible software layer found. Review required connections of the project";
    return result;
  } else {
    // retrieve valid configurations
    vector<RpcArgs::VariablesConfiguration> vcVec;
    const auto& processedContexts = m_worker.GetProcessedContexts();
    for(const auto& context : processedContexts) {
      if(!context->variablesConfigurations.empty()) {
        for(const auto& configuration : context->variablesConfigurations) {
          RpcArgs::VariablesConfiguration vc;
          vector<RpcArgs::LayerVariable> lvVec;
          for(const auto& variable : configuration.variables) {
            RpcArgs::LayerVariable lv = {variable.name, variable.clayer};
            vector<RpcArgs::SettingsType> settings;
            for(const auto& s : variable.settings) {
              if(!s.set.empty()) {
                settings.push_back(RpcArgs::SettingsType{s.set});
              }
            }
            if(!settings.empty()) {
              lv.settings = settings;
            }
            if(!variable.path.empty()) {
              lv.path = variable.path;
            }
            if(!variable.file.empty()) {
              lv.file = variable.file;
            }
            if(!variable.description.empty()) {
              lv.description = variable.description;
            }
            if(!variable.copyTo.empty()) {
              lv.copyTo = variable.copyTo;
            }
            lvVec.push_back(lv);
          }
          vc.variables = lvVec;
          vcVec.push_back(vc);
        }
        // break after first project context with possible variable configurations
        // solutions with undefined layer variables over multiple projects are currently not supported
        break;
      }
    }
    if(!vcVec.empty()) {
      result.configurations = vcVec;
    }
    result.success = true;
    return result;
  }
}

RpcArgs::ListMissingPacksResult RpcHandler::ListMissingPacks(const string& solution, const string& activeTarget) {
  RpcArgs::ListMissingPacksResult result = {{ false }};
  string csolutionFile = solution;
  if(!CheckSolutionArg(csolutionFile, result.message)) {
    return result;
  }
  if(!m_manager.SetupContexts(csolutionFile, activeTarget)) {
    result.message = "Setup of solution contexts failed";
    return result;
  }
  StrVec missingPacks;
  m_worker.ListPacks(missingPacks, true);
  if(!missingPacks.empty()) {
    result.packs = missingPacks;
  }
  result.success = true;
  return result;
}

// end of ProkMgrRpcServer.cpp
