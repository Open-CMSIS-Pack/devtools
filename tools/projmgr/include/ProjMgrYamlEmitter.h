/*
 * Copyright (c) 2020-2025 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PROJMGRYAMLEMITTER_H
#define PROJMGRYAMLEMITTER_H

#include "ProjMgrRunDebug.h"
#include "ProjMgrWorker.h"

/**
 * Forward declarations
*/
namespace YAML {
  class Node;
}

/**
  * @brief projmgr emitter yaml class
*/
class ProjMgrYamlEmitter {
public:
  /**
   * @brief class constructor
  */
  ProjMgrYamlEmitter(ProjMgrParser* parser, ProjMgrWorker* worker);

  /**
   * @brief class destructor
  */
  ~ProjMgrYamlEmitter(void);

  /**
   * @brief set check schema
   * @param bool checkSchema
  */
  void SetCheckSchema(bool checkSchema);

  /**
   * @brief set output directory
   * @param string outputDir
  */
  void SetOutputDir(const std::string& outputDir);

  /**
   * @brief generate cbuild-idx.yml file
   * @param contexts vector with pointers to contexts
   * @param failed contexts
   * @param executes nodes at solution level
   * @return true if executed successfully
  */
  bool GenerateCbuildIndex(const std::vector<ContextItem*>& contexts,
    const std::set<std::string>& failedContexts, const std::map<std::string, ExecutesItem>& executes);

  /**
   * @brief generate cbuild-gen-idx.yml file
   * @param contexts vector with pointers to sibling contexts
   * @param type project type
   * @param outdir output directory
   * @param gendir generated files directory
   * @return true if executed successfully
  */
  bool GenerateCbuildGenIndex(const std::vector<ContextItem*> siblings,
    const std::string& type, const std::string& outdir, const std::string& gendir);

  /**
   * @brief generate cbuild.yml or cbuild-gen.yml file
   * @param context pointer to the context
   * @param reference to generator identifier
   * @param reference to generator pack
   * @param ignoreRteFileMissing if true, ignore missing rte files
   * @return true if executed successfully
  */
  bool GenerateCbuild(ContextItem* context, const std::string& generatorId = std::string(),
    const std::string& generatorPack = std::string(), bool ignoreRteFileMissing = false);

  /**
   * @brief generate cbuild set file
   * @param contexts list of selected contexts
   * @param selectedCompiler name of the selected compiler
   * @param cbuildSetFile path to *.cbuild-set file to be generated
   * @param ignoreRteFileMissing if true, ignore missing rte files
   * @return true if executed successfully
  */
  bool GenerateCbuildSet(const std::vector<std::string> selectedContexts,
    const std::string& selectedCompiler, const std::string& cbuildSetFile, bool ignoreRteFileMissing = false);

  /**
   * @brief generate cbuild pack file
   * @param contexts vector with pointers to contexts
   * @param keepExistingPackContent if true, all entries from existing cbuild-pack should be preserved
   * @param cbuildPackFrozen if true, reject updates to cbuild pack file
   * @return true if executed successfully
  */
  bool GenerateCbuildPack(const std::vector<ContextItem*> contexts, bool keepExistingPackContent, bool cbuildPackFrozen);

  /**
   * @brief generate cbuild run file
   * @param reference to struct with debug/run management info
   * @return true if executed successfully
  */
  bool GenerateCbuildRun(const RunDebugType& debugRun);

protected:
  ProjMgrParser* m_parser = nullptr;
  ProjMgrWorker* m_worker = nullptr;
  std::string m_outputDir;
  std::string m_cbuildRun;
  bool m_checkSchema;

  bool WriteFile(YAML::Node& rootNode, const std::string& filename, const std::string& context = std::string(), bool allowUpdate = true);
  bool CompareFile(const std::string& filename, const YAML::Node& rootNode);
  bool CompareNodes(const YAML::Node& lhs, const YAML::Node& rhs);
  bool NeedRebuild(const std::string& filename, const YAML::Node& rootNode);
  void CopyWestGroups(const std::string& filename, YAML::Node rootNode);
};

#endif  // PROJMGRYAMLEMITTER_H
