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
   * @return true if success otherwise false
  */
  static bool EmitContextInfo(const ContextItem& context, const std::string& dst);
};

#endif  // PROJMGRYAMLEMITTER_H
