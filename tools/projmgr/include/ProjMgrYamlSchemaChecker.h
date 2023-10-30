/*
 * Copyright (c) 2020-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PROJMGRYAMLSCHEMACHECKER_H
#define PROJMGRYAMLSCHEMACHECKER_H

#include "YmlSchemaChecker.h"

/**
  * @brief projmgr schema checker implementation class, directly coupled to underlying YamlSchemaChecker library
*/
class ProjMgrYamlSchemaChecker : public YmlSchemaChecker {
public:
  enum class FileType
  {
    DEFAULT = 0,
    SOLUTION,
    PROJECT,
    LAYER,
    BUILD,
    BUILDIDX,
    BUILDSET,
    GENERATOR,
    BUILDGEN,
    BUILDGENIDX,
    GENERATOR_IMPORT
  };
  /**
   * @brief class constructor
  */
  ProjMgrYamlSchemaChecker(void);

  /**
   * @brief class destructor
  */
  ~ProjMgrYamlSchemaChecker(void);

  /**
   * @brief Validate file data with schemas
   * @param input data to be validated
   * @param type File type
   * @return true if the validation pass otherwise false
  */
  bool Validate(const std::string& input, ProjMgrYamlSchemaChecker::FileType type);

protected:

  bool GetSchemaFile(std::string& schemaFile, const ProjMgrYamlSchemaChecker::FileType& type);
};

#endif  // PROJMGRYAMLSCHEMACHECKER_H
