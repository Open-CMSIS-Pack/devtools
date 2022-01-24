/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "SchemaChecker.h"
#include "SchemaValidator.h"

bool SchemaChecker::Validate(const std::string& datafile,
  const std::string& schemafile, SchemaErrors& errList)
{
  SchemaValidator validator(datafile, schemafile);
  return validator.Validate(errList);
}
