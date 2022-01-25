/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SCHEMACHECKER_H
#define SCHEMACHECKER_H

#include "SchemaError.h"

#include <string>

class SchemaChecker {
public:

  /**
   * @brief Validates the yaml data file with respect to schema given
   * @param datafile input yaml file to be validated
   * @param schemafile input schema file defines the structure of yaml
   * @param errList list of errors found in data file
   * @return true if validation pass, otherwise false
  */
  static bool Validate(const std::string& datafile,
                       const std::string& schemafile,
                       SchemaErrors& errList);
};

#endif // SCHEMACHECKER_H
