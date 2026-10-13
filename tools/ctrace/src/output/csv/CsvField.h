/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_OUTPUT_CSV_CSVFIELD_H
#define CTRACE_SRC_OUTPUT_CSV_CSVFIELD_H

#include <string>
#include <string_view>

namespace Csv {

/** @brief Quotes a CSV field containing commas, quotes, or line breaks, doubling embedded quotes. */
inline std::string escapeField(std::string_view value)
{
  if (value.find_first_of("\",\r\n") == std::string_view::npos) {
    return std::string(value);
  }
  std::string escaped = "\"";
  for (const auto ch : value) {
    if (ch == '"') {
      escaped += "\"\"";
    } else {
      escaped += ch;
    }
  }
  escaped += "\"";
  return escaped;
}

} // namespace Csv

#endif // CTRACE_SRC_OUTPUT_CSV_CSVFIELD_H
