/*
 * Copyright (c) 2024-2025 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PROJMGRRUNDEBUG_H
#define PROJMGRRUNDEBUG_H

#include "ProjMgrWorker.h"

/**
  * @brief programming algorithm types
 */
struct AlgorithmType {
  std::string algorithm;
  unsigned long long start = 0;
  unsigned long long size = 0;
  unsigned long long ramStart = 0;
  unsigned long long ramSize = 0;
  bool bDefault = false;
  std::string pname;
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
  bool bDefault = false;
  bool bStartup = false;
  bool bUninit = false;
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
  std::string run;
  std::string debug;
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
  std::string timeout;
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
 * @brief debugger type
*/
struct DebuggerType {
  std::string name;
  std::string info;
  std::string port;
  unsigned long long clock = 0;
  std::string dbgconf;
};

 /**
  * @brief debug run manager types
 */
struct RunDebugType {
  std::string solution;
  std::string solutionName;
  std::string targetType;
  std::string compiler;
  std::string board;
  std::string boardPack;
  std::string device;
  std::string devicePack;
  std::vector<AlgorithmType> algorithms;
  std::vector<FilesType> outputs;
  std::vector<FilesType> systemDescriptions;
  SystemResourcesType systemResources;
  std::vector<DebuggerType> debuggers;
  DebugVarsType debugVars;
  std::vector<DebugSequencesType> debugSequences;
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
  bool CollectSettings(const std::vector<ContextItem*>& contexts);

protected:
  RunDebugType m_runDebug;
  void GetDebugSequenceBlock(const RteItem* item, DebugSequencesBlockType& block);
  void PushBackUniquely(std::vector<std::pair<const RteItem*, std::vector<std::string>>>& vec,
    const RteItem* item, const std::string pname);
  FilesType SetLoadFromOutput(const ContextItem* context, OutputType output, const std::string type);
};

#endif  // PROJMGRRUNDEBUG_H
