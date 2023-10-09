/*
 * Copyright (c) 2020-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PROJMGRYAMLSCHEMACHECKER_H
#define PROJMGRYAMLSCHEMACHECKER_H

#include "SchemaChecker.h"

/**
  * @brief projmgr schema checker implementation class, directly coupled to underlying YamlSchemaChecker library
*/
class ProjMgrYamlSchemaChecker {
public:
  enum class FileType
  {
    DEFAULT = 0,
    SOLUTION,
    PROJECT,
    LAYER,
    BUILD,
    BUILDSET
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

  /**
   * @brief get list of errors found
   * @return list of errors
  */
  SchemaErrors& GetErrors();

protected:
  SchemaErrors m_errList;
  bool GetSchemaFile(std::string& schemaFile, const ProjMgrYamlSchemaChecker::FileType& type);
};

#endif  // PROJMGRYAMLSCHEMACHECKER_H
