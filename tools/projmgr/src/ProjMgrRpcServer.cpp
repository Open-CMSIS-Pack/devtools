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
  RpcArgs::SuccessResult LoadSolution(const string& solution, const string& activeTarget) override;
  RpcArgs::UsedItems GetUsedItems(const string& context) override;
  RpcArgs::PacksInfo GetPacksInfo(const string& context) override;
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
  result.success = m_solutionLoaded = m_manager.LoadSolution(csolutionFile, activeTarget);
  if (!m_solutionLoaded) {
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

RpcArgs::UsedItems RpcHandler::GetUsedItems(const string& context) {
  RpcArgs::UsedItems usedItems;
  usedItems.success = true;
  RpcDataCollector dc(GetActiveTarget(context));
  dc.CollectUsedItems(usedItems);
  return usedItems;
}

RpcArgs::DeviceList RpcHandler::GetDeviceList(const string& context, const string& namePattern, const string& vendor)
{
  RpcArgs::DeviceList deviceList{{false}};
  if (!m_packsLoaded) {
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
  if (!m_packsLoaded) {
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
  if (!m_packsLoaded) {
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
  if (!m_packsLoaded) {
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
  if(rteClass->GetSelectedBundleName() == bundleName) {
    return result; // no change => false
  }
  if(!contains_key(rteClass->GetBundleNames(), bundleName)) {
    result.message = "Bundle '" + bundleName + "' is not found for component class '" + className +"'";
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

RpcArgs::DraftProjectsInfo RpcHandler::GetDraftProjects(const RpcArgs::DraftProjectsFilter& filter) {
  RpcArgs::DraftProjectsInfo applications;
  applications.success = false;

  if (!m_packsLoaded) {
    applications.message = "Packs must be loaded before retrieving draft projects";
    return applications;
  }

  // initialize context and target attributes with board and device
  ContextItem context;
  m_worker.InitializeTarget(context);
  if (filter.board.has_value() || filter.device.has_value()) {
    context.board = filter.board.has_value() ? filter.board.value() : "";
    context.device = filter.device.has_value() ? filter.device.value() : "";
    if (!m_worker.ProcessDevice(context, BoardOrDevice::SkipProcessor)) {
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
  for (const auto& example : collectedExamples) {
    RpcArgs::ExampleProject e;
    e.name = example.name;
    e.pack = example.pack;
    e.doc = example.doc;
    e.description = example.description;
    if (!example.version.empty()) {
      e.version = example.version;
    }
    if (!example.archive.empty()) {
      e.archive = example.archive;
    }
    for (const auto& [name, environment] : example.environments) {
      RpcArgs::ExampleEnvironment env;
      env.name = name;
      env.file = environment.load;
      env.folder = environment.folder;
      e.environments.push_back(env);
    }
    if (!example.components.empty()) {
      e.components = example.components;
    }
    if (!example.categories.empty()) {
      e.categories = example.categories;
    }
    if (!example.keywords.empty()) {
      e.keywords = example.keywords;
    }
    // classify the example as ref-app if it does not specify boards
    auto& ref = example.boards.empty() ? refApps : examples;
    ref.push_back(e);
  }
  if (!examples.empty()) {
    applications.examples = examples;
  }
  if (!refApps.empty()) {
    applications.refApps = refApps;
  }

  // collect templates
  vector<RpcArgs::SolutionTemplate> templates;
  const auto& csolutionTemplates = m_worker.CollectTemplates(context);
  for (const auto& csolutionTemplate : csolutionTemplates) {
    RpcArgs::SolutionTemplate t;
    t.name = csolutionTemplate.name;
    t.pack = csolutionTemplate.pack;
    t.description = csolutionTemplate.description;
    t.file = csolutionTemplate.file;
    t.folder = csolutionTemplate.path;
    if (!csolutionTemplate.copyTo.empty()) {
      t.copyTo = csolutionTemplate.copyTo;
    }
    templates.push_back(t);
  }
  if (!templates.empty()) {
    applications.templates = templates;
  }

  applications.success = true;
  return applications;
}

RpcArgs::ConvertSolutionResult RpcHandler::ConvertSolution(const string& solution, const string& activeTarget, const bool& updateRte) {
  RpcArgs::ConvertSolutionResult result = {{ false }};
  const auto csolutionFile = RteFsUtils::MakePathCanonical(solution);
  if (!regex_match(csolutionFile, regex(".*\\.csolution\\.(yml|yaml)"))) {
    result.message = solution + " is not a *.csolution.yml file";
    return result;
  }
  if (!m_manager.RunConvert(csolutionFile, activeTarget, updateRte) || !ProjMgrLogger::Get().GetErrors().empty()) {
    if (m_worker.HasVarDefineError()) {
      const auto& vars = m_worker.GetUndefLayerVars();
      result.undefinedLayers = StrVec(vars.begin(), vars.end());
      result.message = "Layer variables undefined, names can be found under 'undefinedLayers'";
    } else if (m_worker.HasCompilerDefineError()) {
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

// end of ProkMgrRpcServer.cpp
