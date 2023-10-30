/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef YML_SCHEMACHECKER_H
#define YML_SCHEMACHECKER_H

#include "ISchemaChecker.h"

class YmlSchemaChecker : public ISchemaChecker{
public:

  /**
   * @brief Validates the YAML data file with respect to schema given
   * @param file input YAML file to be validated
   * @param schemaFile input schema file defines the structure of YAML
   * @return true if validation pass, otherwise false
  */
  bool ValidateFile(const std::string& file, const std::string& schemaFile) override;
};

#endif // YML_SCHEMACHECKER_H
