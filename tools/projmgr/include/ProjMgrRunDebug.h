/*
 * Copyright (c) 2024-2025 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PROJMGRRUNDEBUG_H
#define PROJMGRRUNDEBUG_H

#include "ProjMgrWorker.h"

 /**
  * @brief ram type
 */
struct RamType {
  unsigned long long start = 0;
  unsigned long long size = 0;
  std::string pname;
};

/**
  * @brief programming algorithm types
 */
struct AlgorithmType {
  std::string algorithm;
  unsigned long long start = 0;
  unsigned long long size = 0;
  RamType ram;
};

/**
 * @brief memory type
*/
struct MemoryType {
  std::string name;
  std::string access;
  std::string alias;
  std::string fromPack;
  unsigned long long start = 0;
  unsigned long long size = 0;
  std::string pname;
};

/**
 * @brief system resources type
*/
struct SystemResourcesType {
  std::vector<MemoryType> memories;
};

/**
 * @brief files type
*/
struct FilesType {
  std::string file;
  std::string info;
  std::string type;
  std::string load;
  std::string offset;
  std::string pname;
};

/**
 * @brief debug sequences block type
*/
struct DebugSequencesBlockType {
  std::string info;
  std::string execute;
  std::string control_if;
  std::string control_while;
  std::optional<unsigned int>timeout;
  bool bAtomic = false;
  std::vector<DebugSequencesBlockType> blocks;
};

/**
 * @brief debug sequences type
*/
struct DebugSequencesType {
  std::string name;
  std::string info;
  std::vector<DebugSequencesBlockType> blocks;
  std::string pname;
};

/**
 * @brief debug vars type
*/
struct DebugVarsType {
  std::string vars;
};

/**
 * @brief punit type
*/
struct PunitType {
  std::optional<unsigned int> punit;
  std::optional<unsigned long long> address;
};

/**
 * @brief processor type
*/
struct ProcessorType {
  std::string pname;
  std::vector<PunitType> punits;
  std::optional<unsigned int> apid;
  std::string resetSequence;
};

/**
 * @brief datapatch type
*/
struct DatapatchType {
  unsigned int apid;
  unsigned long long address;
  unsigned long long value;
  std::optional<unsigned long long> mask;
  std::string type;
  std::string info;
};

/**
 * @brief access port type
*/
struct AccessPortType {
  unsigned int apid;
  std::optional<unsigned int> index;
  std::optional<unsigned long long> address;
  std::optional<unsigned int> hprot;
  std::optional<unsigned int> sprot;
  std::vector<DatapatchType> datapatch;
  std::vector<AccessPortType> accessPorts;
};

/**
 * @brief debug port type
*/
struct DebugPortType {
  unsigned int dpid;
  std::optional<unsigned int> jtagTapIndex;
  std::optional<unsigned int> swdTargetSel;
  std::vector<AccessPortType> accessPorts;
};

/**
 * @brief debug topology type
*/
struct DebugTopologyType {
  std::vector<DebugPortType> debugPorts;
  std::vector<ProcessorType> processors;
  std::optional<bool> swj;
  std::optional<bool> dormant;
  std::string sdf;
};

 /**
  * @brief debug run manager types
 */
struct RunDebugType {
  std::string solution;
  std::string solutionName;
  std::string targetType;
  std::string targetSet;
  std::string cbuildRun;
  std::string compiler;
  std::string board;
  std::string boardPack;
  std::string device;
  std::string devicePack;
  std::vector<AlgorithmType> algorithms;
  std::vector<FilesType> outputs;
  std::vector<FilesType> systemDescriptions;
  SystemResourcesType systemResources;
  DebuggerType debugger;
  DebugVarsType debugVars;
  std::vector<DebugSequencesType> debugSequences;
  DebugTopologyType debugTopology;
};

/**
  * @brief projmgr run debug management class
*/
class ProjMgrRunDebug {
public:
  /**
   * @brief class constructor
  */
  ProjMgrRunDebug(void);

  /**
   * @brief class destructor
  */
  ~ProjMgrRunDebug(void);

  /**
   * @brief get run debug info
   * @return reference to m_runDebug
  */
  RunDebugType& Get() { return m_runDebug; };

  /**
   * @brief collect run/debug info for selected contexts
   * @param vector of selected contexts
   * @return true if executed successfully
  */
  bool CollectSettings(const std::vector<ContextItem*>& contexts, const DebugAdaptersItem& adapters);

  void Clear() {
    m_runDebug = {};
  }

protected:
  RunDebugType m_runDebug;
  void GetDebugSequenceBlock(const RteItem* item, DebugSequencesBlockType& block);
  void PushBackUniquely(std::vector<std::pair<const RteItem*, std::vector<std::string>>>& vec,
    const RteItem* item, const std::string pname);
  void AddGeneratedImage(const ContextItem* context, const std::string& filename, const std::string& type, const std::string& load);
  void AddGeneratedImages(const ContextItem* context);
  void SetAccessPorts(std::vector<AccessPortType>& parent, const std::map<unsigned int,
    std::vector<AccessPortType>>& childrenMap);
  void SetProtNodes(const RteDeviceProperty* item, AccessPortType& ap);
  bool GetDebugAdapter(const std::string& name, const DebugAdaptersItem& adapters, DebugAdapterItem& match);
  void CollectDebuggerSettings(const ContextItem& context, const DebugAdaptersItem& adapters,
    const std::map<std::string, RteDeviceProperty*>& pnames);
  void CollectDebugTopology(const ContextItem& context, std::vector<std::pair<const RteItem*, std::vector<std::string>>> debugs,
    const std::map<std::string, RteDeviceProperty*>& pnames);
  void CollectTelnetOptions(const ContextItem& context, DebugAdapterItem& adapter,
    const std::map<std::string, RteDeviceProperty*>& pnames);
  void SetTelnetPort(TelnetOptionsItem& item, unsigned long long& port, std::set<unsigned long long>& usedPorts);
  CustomItem& CustomMapFind(std::vector<std::pair<std::string, CustomItem>>& customMap, const std::string& key);
  void MergeCustomItems(const CustomItem& src, CustomItem& dst);
};

#endif  // PROJMGRRUNDEBUG_H
