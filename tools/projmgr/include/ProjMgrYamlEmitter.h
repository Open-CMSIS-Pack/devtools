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
   * @brief generate cbuild-idx.yml file
   * @param parser reference
   * @param contexts vector with pointers to contexts
   * @param outputDir directory
   * @return true if executed successfully
  */
  static bool GenerateCbuildIndex(ProjMgrParser& parser, const std::vector<ContextItem*> contexts, const std::string& outputDir);

  /**
   * @brief generate cbuild.yml file
   * @param context pointer to the context
   * @return true if executed successfully
  */
  static bool GenerateCbuild(ContextItem* context);
};

#endif  // PROJMGRYAMLEMITTER_H
