/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_OUTPUT_CSV_CSVROWMAPPER_H
#define CTRACE_SRC_OUTPUT_CSV_CSVROWMAPPER_H

#include <string>

struct TraceEvent;

class CsvRowMapper final {
public:
  static std::string header();
  static std::string row(const TraceEvent& event);

private:
  CsvRowMapper() = delete;
};

#endif  // CTRACE_SRC_OUTPUT_CSV_CSVROWMAPPER_H
