/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgrMlops.h"

#include "ProjMgrLogger.h"
#include "ProjMgrParser.h"
#include "ProjMgrUtils.h"

using namespace std;

ProjMgrMlops::ProjMgrMlops(ProjMgrWorker* worker) : m_worker(worker) {
  // Reserved
}

ProjMgrMlops::~ProjMgrMlops(void) {
  // Reserved
}

bool ProjMgrMlops::FindTargetType(const CsolutionItem& csolution, const string& typeName, TargetType& targetType) const {
  for (const auto& [name, type] : csolution.targetTypes) {
    if (name == typeName) {
      targetType = type;
      return true;
    }
  }
  return false;
}

bool ProjMgrMlops::GetTargetSetItemRef(const TargetType& targetType, optional<const string> targetSetName,
  bool simulator, TargetSetItem& targetSet) const {
  if (!targetSetName.has_value()) {
    for (const auto& set : targetType.targetSet) {
      if ((simulator && set.debugger.name == "Arm-FVP") ||
        (!simulator && set.debugger.name != "Arm-FVP")) {
        // default simulator or hardware target set
        targetSet = set;
        return true;
      }
    }
  } else {
    for (const auto& set : targetType.targetSet) {
      if (set.set == targetSetName.value()) {
        // specified target set
        targetSet = set;
        return true;
      }
    }
  }
  // target set could not be determined
  return false;
}

bool ProjMgrMlops::ResolveTargetSet(const CsolutionItem& csolution, const MlopsTargetItem target,
  bool simulatorDefault, string& typeName, TargetSetItem& targetSetItem) const {
  TargetType targetType;
  if (target.targetType.empty()) {
    if (!csolution.targetTypes.empty()) {
      // default simulator: last target-type, default hardware: first target-type
      typeName = simulatorDefault ? csolution.targetTypes.back().first : csolution.targetTypes.front().first;
      targetType = simulatorDefault ? csolution.targetTypes.back().second : csolution.targetTypes.front().second;
    }
  } else {
    typeName = target.targetType; 
    if (!FindTargetType(csolution, target.targetType, targetType)) {
      // print error if specified target type was not found
      ProjMgrLogger::Get().Error("mlops: target type '" + target.targetType + "' not found");
      return false;
    }
  }
  // target set name = nullopt if target type is not specified, to differentiate from default (unnamed) target set
  auto targetSetName = target.targetType.empty() ? nullopt : make_optional(target.targetSet);
  if (!GetTargetSetItemRef(targetType, targetSetName, simulatorDefault, targetSetItem)) {
    if (targetSetName.has_value()) {
      // print error if specified target set was not found
      ProjMgrLogger::Get().Error("mlops: " + (target.targetSet.empty() ?
        ("no default unnamed target set for '" + target.targetType + "'") :
        ("target set '" + target.targetType + '@' + target.targetSet + "' not found")));
      return false;
    }
  }
  return true;
}

string ProjMgrMlops::BuildActive(const string& targetType, const string& targetSet) const {
  return targetSet.empty() ? targetType : targetType + "@" + targetSet;
}

string ProjMgrMlops::GetCustomScalar(const CustomItem& custom, const string& key) const {
  for (const auto& [customKey, value] : custom.map) {
    if (customKey == key) {
      return value.scalar;
    }
  }
  return RteUtils::EMPTY_STRING;
}

string ProjMgrMlops::BuildVelaOptions(const MlopsNpuType& npu, const MlopsVelaItem& vela) const {
  string options;
  if (!npu.type.empty() && !npu.macs.empty()) {
    options += "--accelerator-config " + RteUtils::ToLower(npu.type) + "-" + npu.macs;
  }
  if (!vela.system.empty()) {
    options += (options.empty() ? "" : " ") + string("--system-config ") + vela.system;
  }
  if (!vela.memory.empty()) {
    options += (options.empty() ? "" : " ") + string("--memory-mode ") + vela.memory;
  }
  if (!vela.misc.empty()) {
    options += (options.empty() ? "" : " ") + vela.misc;
  }
  return options;
}

bool ProjMgrMlops::SetMlopsRunType(MlopsRunType& run, const string& targetType, const TargetSetItem& targetSet,
  const vector<ContextItem>& contexts, const string& outBaseDir, const string& solutionName) const {
  run.active = BuildActive(targetType, targetSet.set);
  run.cbuildRun = outBaseDir + '/' + solutionName + '+' + targetType + ".cbuild-run.yml";
  for (auto context : contexts) {
    if (context.outputTypes.elf.on) {
      MlopsOutputType output;
      output.file = context.directories.cprj + '/' + context.directories.outdir + '/' + context.outputTypes.elf.filename;
      output.type = RteConstants::OUTPUT_TYPE_ELF;
      run.output.push_back(output);
    }
    if (context.imageOnly) {
      for (auto item : targetSet.images) {
        if (!item.image.empty()) {
          if (!m_worker->ProcessSequenceRelative(context, item.image, context.csolution->directory, false)) {
            return false;
          }
          if (RteFsUtils::IsRelative(item.image)) {
            RteFsUtils::NormalizePath(item.image, context.directories.cprj);
          }
          if (ProjMgrUtils::FileTypeFromExtension(item.image) == RteConstants::OUTPUT_TYPE_ELF) {
            MlopsOutputType output;
            output.file = item.image;
            output.type = RteConstants::OUTPUT_TYPE_ELF;
            run.output.push_back(output);
          }
        }
      }
    }
  }
  return true;
}

bool ProjMgrMlops::CollectSettings(const CsolutionItem& csolution, MlopsType& mlops) {
  mlops = {};

  const auto& solutionMlops = csolution.mlops;

  // resolve hardware and simulator targets and get references for their items
  string hardwareType, simulatorType;
  TargetSetItem hardwareTargetSet, simulatorTargetSet;
  if (!ResolveTargetSet(csolution, solutionMlops.hardware, false, hardwareType, hardwareTargetSet) ||
      !ResolveTargetSet(csolution, solutionMlops.simulator, true, simulatorType, simulatorTargetSet)) {
    // throw error if target is explicit specified but target set cannot be determined
    return false;
  }

  // get all context items
  map<string, ContextItem>* contexts = nullptr;
  m_worker->GetContexts(contexts);
  if (contexts->empty()) {
    return false;
  }

  // find hardware and simulator contexts
  vector<ContextItem> hardwareContexts, simulatorContexts;
  StrSet pnames;
  vector<tuple<const TargetSetItem&, const string&, vector<ContextItem>&>> refs = {
    {hardwareTargetSet, hardwareType, hardwareContexts}, {simulatorTargetSet, simulatorType, simulatorContexts}};
  for (auto& [targetSet, targetType, ref] : refs) {
    for (const auto& entry : targetSet.images) {
      const string contextName = (entry.context.empty() ? csolution.name : entry.context) + "+" + targetType;
      if (contexts->find(contextName) != contexts->end()) {
        // process context precedences if needed
        auto& context = contexts->at(contextName);
        if (!context.precedences) {
          if (!m_worker->ParseContextLayers(context) || !m_worker->LoadPacks(context) ||
            !m_worker->ProcessPrecedences(context, BoardOrDevice::Both) ||
            !m_worker->SetTargetAttributes(context, context.targetAttributes)) {
            return false;
          }
          m_worker->CollectNpuInfo(context);
        }
        ref.push_back(context);
        pnames.insert(context.deviceItem.pname);
      }
    }
  }

  // check if hardware and simulator contexts were found
  vector<tuple<const vector<ContextItem>&, const MlopsTargetItem&>> contextRefs = {
    {hardwareContexts, solutionMlops.hardware},
    {simulatorContexts, solutionMlops.simulator}
  };
  for (const auto& [ref, t] : contextRefs) {
    if (!t.targetType.empty() && ref.empty()) {
      // print error if context for specified target type was not found
      ProjMgrLogger::Get().Error("mlops: no image or project-context specified for target '" +
        t.targetType + (t.targetSet.empty() ? "" : '@' + t.targetSet) + "'");
      return false;
    }
  }

  // get hardware and simulator context references
  bool hardwareFound = !hardwareContexts.empty();
  bool simulatorFound = !simulatorContexts.empty();
  ContextItem emptyContext;
  ContextItem& hardwareContext = hardwareFound ? hardwareContexts.front() : emptyContext;
  ContextItem& simulatorContext = simulatorFound ? simulatorContexts.front() : emptyContext;

  // if hardware is not determined use first selected context for processor type and access sequences
  ContextItem& context = hardwareFound ? hardwareContext : contexts->at(m_worker->GetSelectedContexts().front());

  // mlops description
  mlops.description = solutionMlops.description;

  // get hardware processor type ("Dcore")
  if (context.targetAttributes.find("Dcore") != context.targetAttributes.end()) {
    mlops.processor.type = context.targetAttributes.at("Dcore");
  }

  // npu type and macs
  mlops.npu.type = solutionMlops.npu.type;
  mlops.npu.macs = solutionMlops.npu.macs;

  // filter npu info items
  vector<NpuInfoItem> npuInfoItems;
  for (NpuInfoItem npu : hardwareContext.npuInfoItems) {
    if (pnames.find(npu.pname) != pnames.end()) {
      npu.macs = RteUtils::StripSuffix(npu.macs, "MACs");
      npuInfoItems.push_back(npu);
    }
  }

  if (!npuInfoItems.empty()) {
    if (mlops.npu.type.empty() && mlops.npu.macs.empty()) {
      // default npu type and macs
      mlops.npu.type = npuInfoItems.front().type;
      mlops.npu.macs = npuInfoItems.front().macs;
    } else if (!mlops.npu.type.empty() && mlops.npu.macs.empty()) {
      // explicit type, find matching macs
      for (const auto& npu : npuInfoItems) {
        if (npu.type == mlops.npu.type) {
          mlops.npu.macs = npu.macs;
          break;
        }
      }
    } else if (mlops.npu.type.empty() && !mlops.npu.macs.empty()) {
      // explicit macs, find matching type
      for (const auto& npu : npuInfoItems) {
        if (RteUtils::StringToULL(npu.macs) == RteUtils::StringToULL(mlops.npu.macs)) {
          mlops.npu.type = npu.type;
          break;
        }
      }
    }
  }

  // vela options
  mlops.vela.options = BuildVelaOptions(mlops.npu, solutionMlops.vela);

  // vela ini
  if (solutionMlops.vela.ini.empty()) {
    // default vela ini from DFP for matching NPU
    for (const auto& npu : npuInfoItems) {
      if (npu.type == mlops.npu.type &&
        RteUtils::StringToULL(npu.macs) == RteUtils::StringToULL(mlops.npu.macs)) {
        mlops.vela.ini = fs::path(npu.velaAbsolutePath).filename().generic_string();
        RteFsUtils::NormalizePath(mlops.vela.ini, csolution.directory + "/.cmsis/");
        // copy file to .cmsis if it does not exist yet
        if (!RteFsUtils::Exists(mlops.vela.ini)) {
          RteFsUtils::CopyCheckFile(npu.velaAbsolutePath, mlops.vela.ini, false);
        }
      }
    }
  } else {
    // explicit vela ini
    mlops.vela.ini = solutionMlops.vela.ini;
    if (!m_worker->ProcessSequenceRelative(context, mlops.vela.ini, csolution.directory, false)) {
      return false;
    }
    if (RteFsUtils::IsRelative(mlops.vela.ini)) {
      RteFsUtils::NormalizePath(mlops.vela.ini, context.directories.cprj);
    }
  }

  // model name and clayer
  if (!solutionMlops.model.clayer.empty()) {
    mlops.model.name = solutionMlops.model.name.empty() ? "Algorithm" : solutionMlops.model.name;
    mlops.model.clayer = solutionMlops.model.clayer;
    if (!m_worker->ProcessSequenceRelative(context, mlops.model.clayer, csolution.directory, false)) {
      return false;
    }
    if (RteFsUtils::IsRelative(mlops.model.clayer)) {
      RteFsUtils::NormalizePath(mlops.model.clayer, context.directories.cprj);
    }    
  }
  
  if (hardwareFound) {
    // set hardware run types
    const string outBaseDir = hardwareContext.directories.cprj + "/" + hardwareContext.directories.outBaseDir;
    if (!SetMlopsRunType(mlops.hardware, hardwareType, hardwareTargetSet, hardwareContexts, outBaseDir, csolution.name)) {
      return false;
    }
  }

  if (simulatorFound) {
    // set simulator run types
    const string outBaseDir = simulatorContext.directories.cprj + "/" + simulatorContext.directories.outBaseDir;
    if (!SetMlopsRunType(mlops.simulator, simulatorType, simulatorTargetSet, simulatorContexts, outBaseDir, csolution.name)) {
      return false;
    }

    // get debugger model and config-file
    mlops.simulator.model = GetCustomScalar(simulatorTargetSet.debugger.custom, "model");
    mlops.simulator.configFile = GetCustomScalar(simulatorTargetSet.debugger.custom, "config-file");
    if (!mlops.simulator.configFile.empty()) {
      if (!m_worker->ProcessSequenceRelative(simulatorContext, mlops.simulator.configFile, csolution.directory, false)) {
        return false;
      }
      if (RteFsUtils::IsRelative(mlops.simulator.configFile)) {
        RteFsUtils::NormalizePath(mlops.simulator.configFile, simulatorContext.directories.cprj);
      }
    }
  }

  return true;
}
