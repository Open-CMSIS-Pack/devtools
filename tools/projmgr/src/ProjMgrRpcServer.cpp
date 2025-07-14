/*
 * Copyright (c) 2025 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgrRpcServer.h"
#include "ProjMgrRpcServerData.h"
#include "ProjMgrLogger.h"
#include "ProjMgr.h"
#include "ProductInfo.h"

#include "CrossPlatformUtils.h"
#include <RteFsUtils.h>

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
  while (getline(cin, line) && !cin.fail()) {
    if (line.find(header) == 0) {
      contentLength = RteUtils::StringToInt(line.substr(header.length()), 0);
    }
    if (line.empty() || line.front() == '\r' || line.front() == '\n') {
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
  while (cin.get(c) && !cin.fail()) {
    if (c == '{') {
      braces++;
      inJson = true;
    }
    if (c == '}') {
      braces--;
    }
    if (inJson) {
      jsonData += c;
    }
    if (inJson && braces == 0) {
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
  RpcArgs::SuccessResult LoadSolution(const string& solution) override;
  RpcArgs::UsedItems GetUsedItems(const string& context) override;
  RpcArgs::PacksInfo GetPacksInfo(const string& context) override;
  RpcArgs::CtRoot GetComponentsTree(const string& context, const bool& all) override;
  RpcArgs::SuccessResult SelectComponent(const string& context, const string& id, const int& count, const RpcArgs::Options& options) override;
  RpcArgs::SuccessResult SelectVariant(const string& context, const string& aggregateId, const string& variantName) override;
  RpcArgs::SuccessResult SelectBundle(const string& context, const string& className, const string& bundleName) override;
  RpcArgs::Results ValidateComponents(const string& context) override;
  RpcArgs::LogMessages GetLogMessages(void) override;

protected:
  enum Exception
  {
    SOLUTION_NOT_FOUND     = -1,
    SOLUTION_NOT_VALID     = -2,
    SOLUTION_NOT_LOADED    = -3,
    CONTEXT_NOT_FOUND      = -4,
    CONTEXT_NOT_VALID      = -5,
    COMPONENT_NOT_FOUND    = -6,
    COMPONENT_NOT_RESOLVED = -7,
    PACKS_NOT_LOADED       = -8,
    PACKS_LOADING_FAIL     = -9,
    RTE_MODEL_ERROR        = -10,
  };

  ProjMgrRpcServer& m_server;
  ProjMgr& m_manager;
  ProjMgrWorker& m_worker;
  ContextItem m_globalContext;
  bool m_packsLoaded = false;
  bool m_solutionLoaded = false;
  bool m_bUseAllPacks = false;

  void StoreSelectedComponents(RteTarget* rteTarget, map<RteComponent*, int>& selectedComponents);
  void UpdateFilter(const string& context, RteTarget* rteTarget, bool bAll); // returns true if changed
  const ContextItem& GetContext(const string& context) const;
  RteTarget* GetActiveTarget(const string& context) const;
  RteComponentAggregate* GetComponentAggregate(const string& context, const string& id) const;
  bool SelectVariantOrVersion(const string& context, const string& id, const string& value, bool bVariant);
};

bool ProjMgrRpcServer::Run(void) {
  JsonRpc2Server jsonServer;
  RpcHandler handler(*this, jsonServer);

  while (!m_shutdown && !cin.fail()) {
    // Get request
    const auto request = m_contextLength ?
      GetRequestFromStdinWithLength() :
      GetRequestFromStdin();

    if (request.empty()) {
      continue;
    }

    ofstream log;
    if (m_debug) {
      log.open(RteFsUtils::GetCurrentFolder(true) + "csolution-rpc-log.txt", fstream::app);
      log << request << std::endl;
    }

    // Handle request
    const auto response = jsonServer.HandleRequest(request);

    // Send response
    if (m_contextLength) {
      // compliant to https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#baseProtocol
      cout << CONTENT_LENGTH_HEADER << response.size() <<
        CrossPlatformUtils::Crlf() << CrossPlatformUtils::Crlf() << response << std::flush;
    } else {
      cout << response << std::endl;
    }

    if (m_debug) {
      log << response << std::endl;
      log.close();
    }

  }
  return true;
}

const ContextItem& RpcHandler::GetContext(const string& context) const {
  if (!m_solutionLoaded) {
    throw JsonRpcException(SOLUTION_NOT_LOADED, "a valid solution must be loaded before proceeding");
  }
  if (context.empty()) {
    throw JsonRpcException(CONTEXT_NOT_VALID, "'context' argument cannot be empty");
  }
  const auto selected = m_worker.GetSelectedContexts();
  if (find(selected.begin(), selected.end(), context) == selected.end()) {
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
  RpcArgs::GetVersionResult res = {true};
  res.message = string("Running ") + INTERNAL_NAME + " " + VERSION_STRING;
  res.version = VERSION_STRING;
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
    // Apply returns if list of gpdc files needs to be updated: irrelevant for csolution
    result.success = true;
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

RpcArgs::UsedItems RpcHandler::GetUsedItems(const string& context) {
  RpcArgs::UsedItems usedItems;
  usedItems.success = true;
  RpcDataCollector dc(GetActiveTarget(context));
  dc.CollectUsedItems(usedItems);
  return usedItems;
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

RpcArgs::SuccessResult RpcHandler::LoadSolution(const string& solution) {
  RpcArgs::SuccessResult result = {false};
  const auto csolutionFile = RteFsUtils::MakePathCanonical(solution);
  if (!regex_match(csolutionFile, regex(".*\\.csolution\\.(yml|yaml)"))) {
    result.message = solution + " is not a *.csolution.yml file";
    return result;
  }
  if (!m_packsLoaded) {
    result.message = "Packs must be loaded before loading solution";
    return result;
  }
  result.success = m_solutionLoaded = m_manager.LoadSolution(csolutionFile);
  if (!m_solutionLoaded) {
    result.message = "failed to load and process solution " + csolutionFile;
  }
  return result;
}

RpcArgs::PacksInfo RpcHandler::GetPacksInfo(const string& context) {
  const auto contextItem = GetContext(context);

  map<string, vector<string>> packRefs;
  for (const auto& packItem : contextItem.packRequirements) {
    if (!packItem.origin.empty()) {
      const auto packId = RtePackage::ComposePackageID(packItem.pack.vendor, packItem.pack.name, packItem.pack.version);
      CollectionUtils::PushBackUniquely(packRefs[packId], packItem.origin);
    }
  }

  RpcArgs::PacksInfo packsInfo;
  for (auto& [pack, packItem] : contextItem.rteActiveTarget->GetFilteredModel()->GetPackages()) {
    RpcArgs::Pack p;
    p.id = packItem->GetPackageID(true);
    const auto& description = packItem->GetDescription();
    if (!description.empty()) {
      p.description = description;
    }
    string overview = packItem->GetChildAttribute("description", "overview");
    if (!overview.empty()) {
      RteFsUtils::NormalizePath(overview, packItem->GetAbsolutePackagePath());
      p.overview = overview;
    }
    if (contextItem.packages.find(p.id) != contextItem.packages.end()) {
      p.used = true;
      if (packRefs.find(p.id) != packRefs.end()) {
        p.references = packRefs.at(p.id);
      }
    }
    packsInfo.packs.push_back(p);
  }
  packsInfo.success = true;
  return packsInfo;
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
    auto& contextItem = GetContext(context);
    // use pack ID's from context
    set<string> packIds;
    for(const auto& [_, packs] : contextItem.userInputToResolvedPackIdMap) {
      for (const auto& id : packs) {
        packIds.insert(id);
      }
    }
    // add new packs from current selection otherwise we will loose the selection
    for(auto [c, _count] : selectedComponents) {
      auto id = c->GetPackageID();
      packIds.insert(id);
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
  if(options.layer.has_value()) {
    rteAggregate->AddAttribute("layer", options.layer.value(), false);
  }
  if(options.explicitVendor.has_value()) {
    rteAggregate->AddAttribute("explicitVendor", options.explicitVendor.value() ? "1" : "", false);
  }

  // TODO: check if version is plausible
  if(options.explicitVersion.has_value()) {
    rteAggregate->AddAttribute("explicitVersion", options.explicitVersion.value(), false);
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
  rteClass->SetSelectedBundleName(bundleName, true);
  GetActiveTarget(context)->EvaluateComponentDependencies();
  return result;
}


RpcArgs::Results RpcHandler::ValidateComponents(const string& context) {
  auto contextItem = GetContext(context);

  RpcArgs::Results results;
  auto validationRes = m_worker.ValidateContext(contextItem);
  results.result = RteItem::ConditionResultToString(validationRes);
  if (validationRes < RteItem::ConditionResult::FULFILLED) {
    results.validation = vector<RpcArgs::Result>{};
    for (const auto& validation : contextItem.validationResults) {
      RpcArgs::Result r;
      r.result = RteItem::ConditionResultToString(validation.result);
      r.id = validation.id;
      if (!validation.aggregates.empty()) {
        r.aggregates = vector<string>(validation.aggregates.begin(), validation.aggregates.end());
      }
      if (!validation.conditions.empty()) {
        RpcArgs::Condition c;
        r.conditions = vector<RpcArgs::Condition>{};
        for (const auto& condition : validation.conditions) {
          c.expression = condition.expression;
          if (!condition.aggregates.empty()) {
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
  for (const auto& [_, info] : ProjMgrLogger::Get().GetInfos()) {
    for (const auto& msg : info) {
      CollectionUtils::PushBackUniquely(infoVec, msg);
    }
  }
  StrVec errorsVec;
  for (const auto& [_, errors] : ProjMgrLogger::Get().GetErrors()) {
    for (const auto& msg : errors) {
      CollectionUtils::PushBackUniquely(errorsVec, msg);
    }
  }
  StrVec warningsVec;
  for (const auto& [_, warnings] : ProjMgrLogger::Get().GetWarns()) {
    for (const auto& msg : warnings) {
      CollectionUtils::PushBackUniquely(warningsVec, msg);
    }
  }
  RpcArgs::LogMessages messages;
  if (!infoVec.empty()) {
    messages.info = infoVec;
  }
  if (!errorsVec.empty()) {
    messages.errors = errorsVec;
  }
  if (!warningsVec.empty()) {
    messages.warnings = warningsVec;
  }
  messages.success = true;
  return messages;
}

// end of ProkMgrRpcServer.cpp
