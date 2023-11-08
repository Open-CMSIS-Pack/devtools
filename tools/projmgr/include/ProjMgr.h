/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PROJMGR_H
#define PROJMGR_H

#include "ProjMgrParser.h"
#include "ProjMgrWorker.h"
#include "ProjMgrGenerator.h"
#include "ProjMgrYamlEmitter.h"

#include <cxxopts.hpp>

 /**
  * @brief projmgr class
 */
class ProjMgr {
public:
  /**
   * @brief class constructor
  */
  ProjMgr(void);

  /**
   * @brief class destructor
  */
  ~ProjMgr(void);

  /**
   * @brief entry point for running projmgr
   * @param argc command line argument count
   * @param argv command line argument vector
   * @param envp environment variables
  */
  static int RunProjMgr(int argc, char **argv, char** envp);

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
  void ShowVersion(void);

  /**
   * @brief get parser object
   * @return reference to m_parser
  */
  ProjMgrParser& GetParser(void) { return m_parser; };

  /**
   * @brief get worker object
   * @return reference to m_worker
  */
  ProjMgrWorker& GetWorker(void) { return m_worker; };

  /**
   * @brief get generator object
   * @return reference to m_generator
  */
  ProjMgrGenerator& GetGenerator(void) { return m_generator; };

  /**
   * @brief get emitter object
   * @return reference to m_emitter
  */
  ProjMgrYamlEmitter& GetEmitter(void) { return m_emitter; };

  /**
   * @brief get cdefault file in solution/project or in installation directory
   * @return true if file is found successfully, false otherwise
  */
  bool GetCdefaultFile(void);

protected:
  ProjMgrParser m_parser;
  ProjMgrExtGenerator m_extGenerator;
  ProjMgrWorker m_worker;
  ProjMgrGenerator m_generator;
  ProjMgrYamlEmitter m_emitter;

  std::string m_csolutionFile;
  std::string m_cdefaultFile;
  std::vector<std::string> m_context;
  std::string m_contextReplacement;
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
  bool m_checkSchema;
  bool m_missingPacks;
  bool m_updateRteFiles;
  bool m_verbose;
  bool m_debug;
  bool m_dryRun;
  bool m_ymlOrder;
  bool m_contextSet;
  GroupNode m_files;
  std::vector<ContextItem*> m_processedContexts;

  bool RunConfigure(bool printConfig = false);
  bool RunConvert(void);
  bool RunCodeGenerator(void);
  bool RunListPacks(void);
  bool RunListBoards(void);
  bool RunListDevices(void);
  bool RunListComponents(void);
  bool RunListConfigs(void);
  bool RunListDependencies(void);
  bool RunListContexts(void);
  bool RunListGenerators(void);
  bool RunListLayers(void);
  bool RunListToolchains(void);
  bool RunListEnvironment(void);
  bool PopulateContexts(void);
  bool SetLoadPacksPolicy(void);
};

#endif  // PROJMGR_H
