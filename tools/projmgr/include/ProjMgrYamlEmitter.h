/*
 * Copyright (c) 2020-2024 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PROJMGRYAMLEMITTER_H
#define PROJMGRYAMLEMITTER_H

#include "ProjMgrWorker.h"

/**
  * @brief projmgr emitter yaml class
*/
class ProjMgrYamlEmitter {
public:
  /**
   * @brief class constructor
  */
  ProjMgrYamlEmitter(void);

  /**
   * @brief class destructor
  */
  ~ProjMgrYamlEmitter(void);

  /**
   * @brief generate cbuild-idx.yml file
   * @param parser reference
   * @param worker reference
   * @param contexts vector with pointers to contexts
   * @param outputDir directory
   * @param failed contexts
   * @param executes nodes at solution level
   * @param boolean check schema of generated file
   * @return true if executed successfully
  */
  static bool GenerateCbuildIndex(ProjMgrParser& parser, ProjMgrWorker& worker,
    const std::vector<ContextItem*>& contexts, const std::string& outputDir,
    const std::set<std::string>& failedContexts,
    const std::map<std::string, ExecutesItem>& executes, bool checkSchema);

  /**
   * @brief generate cbuild-gen-idx.yml file
   * @param parser reference
   * @param contexts vector with pointers to sibling contexts
   * @param type project type
   * @param outdir output directory
   * @param gendir generated files directory
   * @param boolean check schema of generated file
   * @return true if executed successfully
  */
  static bool GenerateCbuildGenIndex(ProjMgrParser& parser, const std::vector<ContextItem*> siblings,
    const std::string& type, const std::string& outdir, const std::string& gendir, bool checkSchema);

  /**
   * @brief generate cbuild.yml or cbuild-gen.yml file
   * @param context pointer to the context
   * @param boolean check schema of generated file
   * @param reference to generator identifier
   * @param reference to generator pack
   * @return true if executed successfully
  */
  static bool GenerateCbuild(ContextItem* context, bool checkSchema,
    const std::string& generatorId = std::string(),
    const std::string& generatorPack = std::string(),
    bool ignoreRteFileMissing = false);

  /**
   * @brief generate cbuild set file
   * @param contexts list of selected contexts
   * @param selectedCompiler name of the selected compiler
   * @param cbuildSetFile path to *.cbuild-set file to be generated
   * @param boolean check schema of generated file
   * @return true if executed successfully
  */
  static bool GenerateCbuildSet(const std::vector<std::string> selectedContexts,
    const std::string& selectedCompiler, const std::string& cbuildSetFile, bool checkSchema, bool ignoreRteFileMissing = false);

  /**
   * @brief generate cbuild pack file
   * @param contexts vector with pointers to contexts
   * @param keepExistingPackContent if true, all entries from existing cbuild-pack should be preserved
   * @param cbuildPackFrozen if true, reject updates to cbuild pack file
   * @param boolean check schema of generated file
   * @return true if executed successfully
  */
  static bool GenerateCbuildPack(ProjMgrParser& parser, const std::vector<ContextItem*> contexts, bool keepExistingPackContent, bool cbuildPackFrozen, bool checkSchema);
};

#endif  // PROJMGRYAMLEMITTER_H
