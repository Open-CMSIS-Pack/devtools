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

  /**
   * @brief Validates a file against schema obtained by FindSchema() method
   * @param fileName file to validate
   * @return true if successful
  */
  bool Validate(const std::string& file) override;

   /**
   * @brief Finds schema for given file to validate
   * @param fileName file to validate
   * @return schema file name if found, empty string otherwise
  */
  std::string FindSchema(const std::string& file) const override;
};

#endif  // PROJMGRYAMLSCHEMACHECKER_H
