/*
 * Copyright (c) 2020-2025 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PROJMGR_H
#define PROJMGR_H

#include "ProjMgrParser.h"
#include "ProjMgrWorker.h"
#include "ProjMgrGenerator.h"
#include "ProjMgrYamlEmitter.h"
#include "ProjMgrRunDebug.h"
#include "ProjMgrRpcServer.h"

#include <cxxopts.hpp>

/**
  * @brief Error return codes
*/
enum ErrorCode
{
  SUCCESS = 0,
  ERROR = 1,
  VARIABLE_NOT_DEFINED = 2,
  COMPILER_NOT_DEFINED = 3,
};

 /**
  * @brief projmgr class
 */
class ProjMgr {
public:
  /**
   * @brief class constructor
  */
  ProjMgr();

  /**
   * @brief class destructor
  */
  ~ProjMgr();

  /**
   * @brief entry point for running projmgr
   * @param argc command line argument count
   * @param argv command line argument vector
   * @param envp environment variables
   * @return program exit code as an integer, 0 for success
  */
  static int RunProjMgr(int argc, char **argv, char** envp);

  /**
   * @brief get worker object
   * @return reference to m_worker
  */
  ProjMgrWorker& GetWorker() { return m_worker; };

  /**
   * @brief clear internal structures and worker object
  */
  void Clear();

  /**
   * @brief load solution
   * @param path to <solution>.csolution.yml file
   * @return processing status
  */
  bool LoadSolution(const std::string& csolution);

protected:
  /**
   * @brief parse command line options
   * @param argc command line argument count
   * @param argv command line argument vector
   * @return parsing result as an integer: 0 success, 1 error, -1 version or help requested
  */
  int ParseCommandLine(int argc, char **argv);

  /**
   * @brief process requested commands specified in command line
   * @return program exit code as an integer, 0 for success
  */
  int ProcessCommands();

  /**
   * @brief print usage
   * @param cmdOptionsDict map of command and options
   * @param cmd command for which usage is to be generated
   * @param subCmd subcommand for which usage is to be generated
   * @return true if successful, otherwise false
  */
  bool PrintUsage(const std::map<std::string,
    std::pair<bool, std::vector<cxxopts::Option>>>& cmdOptionsDict,
    const std::string& cmd, const std::string & subCmd);

  /**
   * @brief show version
  */
  void ShowVersion();

  /**
   * @brief get parser object
   * @return reference to m_parser
  */
  ProjMgrParser& GetParser() { return m_parser; };

  /**
   * @brief get generator object
   * @return reference to m_generator
  */
  ProjMgrGenerator& GetGenerator() { return m_generator; };

  /**
   * @brief get emitter object
   * @return reference to m_emitter
  */
  ProjMgrYamlEmitter& GetEmitter() { return m_emitter; };

  /**
   * @brief get run debug manager object
   * @return reference to m_runDebug
  */
  ProjMgrRunDebug& GetRunDebug() { return m_runDebug; };

  /**
   * @brief get cdefault file in solution/project or in installation directory
   * @return true if file is found successfully, false otherwise
  */
  bool GetCdefaultFile();

  /**
   * @brief get cmsis-toolbox version from manifest file
   * @return version as string
  */
  const std::string GetToolboxVersion(const std::string& manifestFilePath);

  /**
   * @brief get debug adapters file path
   * @return file path or empty string if it does not exist
  */
  const std::string GetDebugAdaptersFile(void);

protected:
  ProjMgrParser m_parser;
  ProjMgrExtGenerator m_extGenerator;
  ProjMgrWorker m_worker;
  ProjMgrGenerator m_generator;
  ProjMgrYamlEmitter m_emitter;
  ProjMgrRunDebug m_runDebug;
  ProjMgrRpcServer m_rpcServer;

  std::string m_csolutionFile;
  std::string m_cdefaultFile;
  std::vector<std::string> m_context;
  std::string m_filter;
  std::string m_codeGenerator;
  std::string m_command;
  std::string m_args;
  std::string m_rootDir;
  std::string m_outputDir;
  std::string m_outputType;
  std::string m_loadPacksPolicy;
  std::string m_clayerSearchPath;
  std::string m_export;
  std::string m_selectedToolchain;
  std::string m_activeTargetSet;
  bool m_checkSchema;
  bool m_missingPacks;
  bool m_updateRteFiles;
  bool m_verbose;
  bool m_debug;
  bool m_dryRun;
  bool m_ymlOrder;
  bool m_contextSet;
  bool m_relativePaths;
  bool m_frozenPacks;
  bool m_cbuildgen;
  bool m_updateIdx;
  GroupNode m_files;
  std::vector<ContextItem*> m_processedContexts;
  std::vector<ContextItem*> m_allContexts;
  std::set<std::string> m_failedContext;

  bool RunConfigure();
  bool RunConvert();
  bool RunCodeGenerator();
  bool RunListPacks();
  bool RunListBoards();
  bool RunListDevices();
  bool RunListComponents();
  bool RunListConfigs();
  bool RunListDependencies();
  bool RunListExamples();
  bool RunListContexts();
  bool RunListTargetSets();
  bool RunListGenerators();
  bool RunListLayers();
  bool RunListToolchains();
  bool RunListEnvironment();
  bool PopulateContexts();
  bool SetLoadPacksPolicy();
  bool ValidateCreatedFor(const std::string& createdFor);

  bool Configure();
  bool GenerateYMLConfigurationFiles(bool previousResult);
  bool UpdateRte();
  bool ParseAndValidateContexts();
  bool ProcessContexts();
  bool IsSolutionImageOnly();
};

#endif  // PROJMGR_H
