/*
 * Copyright (c) 2020-2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PROJMGRMLOPS_H
#define PROJMGRMLOPS_H

#include "ProjMgrWorker.h"

struct CsolutionItem;
struct ContextItem;
struct CustomItem;
struct MlopsItem;
struct TargetSetItem;
struct TargetType;

/**
 * @brief mlops processor type containing
 *        processor type
*/
struct MlopsProcessorType {
  std::string type;
};

/**
 * @brief mlops NPU type containing
 *        NPU type,
 *        number of MACs
*/
struct MlopsNpuType {
  std::string type;
  std::string macs;
};

/**
 * @brief mlops Vela type containing
 *        path to INI file,
 *        option string
*/
struct MlopsVelaType {
  std::string ini;
  std::string options;
};

/**
 * @brief mlops model type containing
 *        path to AI clayer,
 *        model name
*/
struct MlopsModelType {
  std::string clayer;
  std::string name;
};

/**
 * @brief Mlops output type
*/
struct MlopsOutputType {
  std::string file;
  std::string type;
};

/**
 * @brief mlops run type containing
 *        active target-set,
 *        cbuild-run file
 *        output artifacts
*/
struct MlopsRunType {
  std::string active;
  std::string cbuildRun;
  std::vector<MlopsOutputType> output;
};

/**
 * @brief mlops simulator type containing
 *        active target-set,
 *        cbuild-run file
 *        output artifacts
 *        simulator model,
 *        simulator configuration file
*/
struct MlopsSimulatorType : MlopsRunType {
  std::string model;
  std::string configFile;
};

/**
 * @brief mlops type
*/
struct MlopsType {
  std::string description;
  MlopsProcessorType processor;
  MlopsNpuType npu;
  MlopsVelaType vela;
  MlopsModelType model;
  MlopsRunType hardware;
  MlopsSimulatorType simulator;
};

/**
 * @brief projmgr mlops management class
*/
class ProjMgrMlops {
public:
  /**
   * @brief class constructor
  */
  ProjMgrMlops(ProjMgrWorker* worker);

  /**
   * @brief class destructor
  */
  ~ProjMgrMlops(void);

  /**
   * @brief collect mlops information
   * @param csolution csolution settings
   * @param mlops output settings
   * @return true if executed successfully
  */
  bool CollectSettings(const CsolutionItem& csolution, MlopsType& mlops);

private:
  ProjMgrWorker* m_worker = nullptr;
  bool FindTargetType(const CsolutionItem& csolution, const std::string& typeName, TargetType& targetType) const;
  bool GetTargetSetItemRef(const TargetType& targetType, const std::string& targetTypeName,
    const std::string& targetSetName, bool simulatorDefault, TargetSetItem& targetSet) const;
  std::string BuildActive(const std::string& targetType, const std::string& targetSet) const;
  std::string GetCustomScalar(const CustomItem& custom, const std::string& key) const;
  std::string BuildVelaOptions(const MlopsNpuType& npu, const MlopsVelaItem& vela) const;
  void SetMlopsRunType(MlopsRunType& run, const std::string& targetType, const std::string& targetSet,
    const std::vector<ContextItem>& contexts, const std::string& outBaseDir, const std::string& solutionName) const;
};

#endif  // PROJMGRMLOPS_H
