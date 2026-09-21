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
struct TraceByteSkip;
struct TraceDecodeAbort;

/** @brief Maps semantic trace events to the stable CSV representation. */
class CsvRowMapper final {
public:
  /** @brief Returns the stable CSV header row. */
  static std::string header();
  /** @brief Returns one CSV row for a semantic trace event. */
  static std::string row(const TraceEvent& event);
  /** @brief Returns a skipped-byte info row with the observed ID, if any, and no timestamp. */
  static std::string byteSkipRow(const TraceByteSkip& skipped);
  /** @brief Returns a global error row marking partial output, without a synthetic route or timestamp. */
  static std::string decodeAbortRow(const TraceDecodeAbort& failure);

private:
  /** @brief Prevents construction of this stateless mapping utility. */
  CsvRowMapper() = delete;
};

#endif  // CTRACE_SRC_OUTPUT_CSV_CSVROWMAPPER_H
