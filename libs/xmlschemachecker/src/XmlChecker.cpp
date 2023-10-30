/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "XmlChecker.h"
#include "XmlValidator.h"

bool XmlChecker::Validate(const std::string& xmlfile, const std::string& schemafile)
{
  XmlValidator validator;
  return validator.Validate(xmlfile, schemafile);
}
