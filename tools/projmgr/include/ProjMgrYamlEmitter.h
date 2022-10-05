/*
 * Copyright (c) 2020-2022 Arm Limited. All rights reserved.
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
   * @brief emit context info
   * @param context
   * @param destinationPath A folder where the generator should place the resulting generated files
   * @return Returns the file path of the created generator input file if success
  */
  static std::optional<std::string> EmitContextInfo(const ContextItem& context, const std::string& destinationPath);

  /**
   * @brief generate cbuild.yml file
   * @param parser reference
   * @param processedContexts vector with pointers to contexts
   * @param output directory
   * @return true if executed successfully
  */
  bool GenerateCbuild(ProjMgrParser& parser, const std::vector<ContextItem*> processedContexts, const std::string& outputDir);
};

#endif  // PROJMGRYAMLEMITTER_H
