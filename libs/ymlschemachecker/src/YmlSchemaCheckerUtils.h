/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef YML_SCHEMA_CHECKER_UTILS_H
#define YML_SCHEMA_CHECKER_UTILS_H

#include <nlohmann/json.hpp>

using nlohmann::json;

class YmlSchemaCheckerUtils {
public:
  static void SanitizeJsonForNumericSuffixes(json& jObj);
  static bool ConvertSuffixedHexString(json& value);
};

#endif // YML_SCHEMA_CHECKER_UTILS_H
