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

/** @brief Maps semantic trace events to the stable CSV representation. */
class CsvRowMapper final {
public:
  /** @brief Returns the stable CSV header row. */
  static std::string header();
  /** @brief Returns one CSV row for a semantic trace event. */
  static std::string row(const TraceEvent& event);

private:
  /** @brief Prevents construction of this stateless mapping utility. */
  CsvRowMapper() = delete;
};

#endif  // CTRACE_SRC_OUTPUT_CSV_CSVROWMAPPER_H
