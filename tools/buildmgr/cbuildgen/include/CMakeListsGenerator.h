/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

 // CMakeListsGenerator.h
#ifndef CMAKELISTSGENERATOR_H
#define CMAKELISTSGENERATOR_H

#include "BuildSystemGenerator.h"

class CMakeListsGenerator : public BuildSystemGenerator {
public:
  /**
   * @brief generate CMakeLists for target building
   * @return true if CMakelists file is generated successfully, otherwise false
  */
  bool GenBuildCMakeLists(void);
};

#endif  // CMAKELISTSGENERATOR_H
