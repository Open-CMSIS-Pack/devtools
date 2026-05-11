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
  ProjMgrLogger::Get().Error("mlops: target-type '" + typeName + "' not found");
  return false;
}

bool ProjMgrMlops::GetTargetSetItemRef(const TargetType& targetType, const string& targetTypeName,
  const string& targetSetName, bool simulatorDefault, TargetSetItem& targetSet) const {
  if (targetSetName.empty()) {
    if (simulatorDefault) {
      for (const auto& set : targetType.targetSet) {
        if (set.debugger.name == "Arm-FVP") {
          targetSet = set;
          return true;
        }
      }
      ProjMgrLogger::Get().Error("mlops: simulator target with debugger 'Arm-FVP' not found");
      return false;
    }
    if (!targetType.targetSet.empty()) {
      targetSet = targetType.targetSet.front();
      return true;
    }
  } else {
    for (const auto& set : targetType.targetSet) {
      if (set.set == targetSetName) {
        targetSet = set;
        return true;
      }
    }
  }
  ProjMgrLogger::Get().Error("mlops: target-type '" + targetTypeName + "' target-set '" +
    (targetSetName.empty() ? "<default>" : targetSetName) + "' not found");
  return false;
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

void ProjMgrMlops::SetMlopsRunType(MlopsRunType& run, const string& targetType, const string& targetSet,
  const vector<ContextItem>& contexts, const string& outBaseDir, const string& solutionName) const {
  run.active = BuildActive(targetType, targetSet);
  run.cbuildRun = outBaseDir + '/' + solutionName + '+' + targetType + ".cbuild-run.yml";
  for (const auto& context : contexts) {
    if (context.outputTypes.elf.on) {
      MlopsOutputType output;
      output.file = context.directories.cprj + '/' + context.directories.outdir + '/' + context.outputTypes.elf.filename;
      output.type = RteConstants::OUTPUT_TYPE_ELF;
      run.output.push_back(output);
    }
  }
}

bool ProjMgrMlops::CollectSettings(const CsolutionItem& csolution, MlopsType& mlops) {
  mlops = {};

  const auto& solutionMlops = csolution.mlops;

  // hardware and simulator target types
  const string hardwareType = !solutionMlops.hardware.targetType.empty() ? solutionMlops.hardware.targetType :
    (!csolution.targetTypes.empty() ? csolution.targetTypes.front().first : RteUtils::EMPTY_STRING);
  const string simulatorType = !solutionMlops.simulator.targetType.empty() ? solutionMlops.simulator.targetType :
    (!csolution.targetTypes.empty() ? csolution.targetTypes.back().first : RteUtils::EMPTY_STRING);

  // get hardware set
  TargetType hardwareTargetType;
  TargetSetItem hardwareTargetSet;
  if (!FindTargetType(csolution, hardwareType, hardwareTargetType) ||
    !GetTargetSetItemRef(hardwareTargetType, hardwareType, solutionMlops.hardware.targetSet, false, hardwareTargetSet)) {
    return false;
  }
  
  // get simulator set
  TargetType simulatorTargetType;
  TargetSetItem simulatorTargetSet;
  if (!FindTargetType(csolution, simulatorType, simulatorTargetType) ||
    !GetTargetSetItemRef(simulatorTargetType, simulatorType, solutionMlops.simulator.targetSet, true, simulatorTargetSet)) {
    return false;
  }

  // get all context items
  map<string, ContextItem>* contexts = nullptr;
  m_worker->GetContexts(contexts);

  // find hardware and simulator contexts
  vector<ContextItem> hardwareContexts, simulatorContexts;
  StrSet pnames;
  vector<tuple<const TargetSetItem&, const string&, vector<ContextItem>&>> refs = {
    {hardwareTargetSet, hardwareType, hardwareContexts}, {simulatorTargetSet, simulatorType, simulatorContexts}};
  for (auto& [targetSet, targetType, ref] : refs) {
    for (const auto& image : targetSet.images) {
      if (!image.context.empty()) {
        const string contextName = image.context + "+" + targetType;
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
  }

  // check if hardware and simulator contexts were found
  vector<tuple<const vector<ContextItem>&, const string&, const string&>> contextRefs = {
    {hardwareContexts, hardwareType, hardwareTargetSet.set},
    {simulatorContexts, simulatorType, simulatorTargetSet.set}
  };
  for (const auto& [ref, targetType, targetSet] : contextRefs) {
    if (ref.empty()) {
      ProjMgrLogger::Get().Error("mlops: target-type '" + targetType + "' target-set '" +
        (targetSet.empty() ? "<default>" : targetSet) + "' project-contexts not found");
      return false;
    }
  }

  auto& hardwareContext = hardwareContexts.front();
  auto& simulatorContext = simulatorContexts.front();

  // mlops description
  mlops.description = solutionMlops.description;

  // get hardware processor type ("Dcore")
  if (hardwareContext.targetAttributes.find("Dcore") != hardwareContext.targetAttributes.end()) {
    mlops.processor.type = hardwareContext.targetAttributes.at("Dcore");
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
    if (!m_worker->ProcessSequenceRelative(hardwareContext, mlops.vela.ini, csolution.directory, false)) {
      return false;
    }
    if (RteFsUtils::IsRelative(mlops.vela.ini)) {
      RteFsUtils::NormalizePath(mlops.vela.ini, hardwareContext.directories.cprj);
    }
  }
  
  // model name and clayer
  if (!solutionMlops.model.clayer.empty()) {
    mlops.model.name = solutionMlops.model.name.empty() ? "Algorithm" : solutionMlops.model.name;
    mlops.model.clayer = solutionMlops.model.clayer;
    if (!m_worker->ProcessSequenceRelative(hardwareContext, mlops.model.clayer, csolution.directory, false)) {
      return false;
    }
    if (RteFsUtils::IsRelative(mlops.model.clayer)) {
      RteFsUtils::NormalizePath(mlops.model.clayer, hardwareContext.directories.cprj);
    }    
  }
  
  // set hardware and simulator run types
  const string outBaseDir = hardwareContext.directories.cprj + "/" + hardwareContext.directories.outBaseDir;
  SetMlopsRunType(mlops.hardware, hardwareType, hardwareTargetSet.set, hardwareContexts, outBaseDir, csolution.name);
  SetMlopsRunType(mlops.simulator, simulatorType, simulatorTargetSet.set, simulatorContexts, outBaseDir, csolution.name);

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

  return true;
}
