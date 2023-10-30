/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "YmlSchemaChecker.h"
#include "YmlSchemaValidator.h"

bool YmlSchemaChecker::ValidateFile(const std::string& file, const std::string& schemaFile)
{
  YmlSchemaValidator validator(file, schemaFile);
  return validator.Validate(m_errors);
}
// end of YmlSchemaChecker.cpp

