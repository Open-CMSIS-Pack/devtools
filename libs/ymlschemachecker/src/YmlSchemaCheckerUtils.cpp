/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "YmlSchemaCheckerUtils.h"
#include "RteUtils.h"

#include <regex>

void YmlSchemaCheckerUtils::SanitizeJsonForNumericSuffixes(json& jObj) {
  if (jObj.is_object()) {
    for (auto &item : jObj.items()) {
      SanitizeJsonForNumericSuffixes(item.value());
    }
  }
  else if (jObj.is_array()) {
    for (auto &obj : jObj)
      SanitizeJsonForNumericSuffixes(obj);
  }
  else if (jObj.is_string()) {
    ConvertSuffixedHexString(jObj);
  }
}

// Try to convert a string like 0x1234U, 0XFFul, 12345UL, 0755U into a numeric JSON value.
// - suffixes: U, L, UL, LU, LL, ULL (case-insensitive)
// Returns true and replaces 'value' if conversion succeeds.
bool YmlSchemaCheckerUtils::ConvertSuffixedHexString(json& value) {
  if (!value.is_string())
    return false;

  // Only allow valid C/C++ integer suffixes: U, L, UL, LU, LL, ULL, LLU (case-insensitive, as a single group)
  static const std::regex hex_re(R"(^([+-]?0[xX])([0-9A-Fa-f]+)(u|l|ul|lu|ll|ull|llu)?$)", std::regex_constants::icase);
  static const std::regex dec_re(R"(^([+-]?\d+)(u|l|ul|lu|ll|ull|llu)?$)", std::regex_constants::icase);
  std::string str = value.get<std::string>();
  std::smatch m;

  auto is_valid_suffix = [](const std::string& suffix) -> bool {
    std::string s = suffix;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s.empty() || s == "u" || s == "l" || s == "ul" || s == "lu" || s == "ll" || s == "ull" || s == "llu";
  };

  try {
    if (std::regex_match(str, m, hex_re)) {
      std::string digits = m[2].str();
      std::string suffix = m[3].matched ? m[3].str() : "";
      if (!is_valid_suffix(suffix))
        return false;
      unsigned long long v = std::stoull(digits, nullptr, 16);
      value = json(v);
      return true;
    }
    else if (std::regex_match(str, m, dec_re)) {
      std::string digits = m[1].str();
      std::string suffix = m[2].matched ? m[2].str() : "";
      if (!is_valid_suffix(suffix))
        return false;
      if (!digits.empty() && digits[0] == '-') {
        long long v = std::stoll(digits, nullptr, 10);
        value = json(v);
      } else {
        unsigned long long v = std::stoull(digits, nullptr, 10);
        value = json(v);
      }
      return true;
    }
  }
  catch (const std::exception &e) {
    return false;
  }
  return false;
}
