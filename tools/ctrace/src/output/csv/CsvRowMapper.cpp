/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CsvRowMapper.h"

#include "TraceEvent.h"
#include "TraceSelection.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>

/** @brief Identifies columns in the stable ctrace CSV schema. */
enum class CsvColumn : std::size_t {
  Cycles,
  Stream,
  Type,
  Source,
  Value,
  Pc,
  Address,
  Note,
  Count,
};

constexpr std::array<std::string_view, static_cast<std::size_t>(CsvColumn::Count)> kCsvColumnNames{{
    "cycles",
    "stream",
    "type",
    "source",
    "value",
    "pc",
    "address",
    "note",
}};

using CsvRow = std::array<std::string, static_cast<std::size_t>(CsvColumn::Count)>;

template <typename Columns> static std::string joinColumns(const Columns& columns)
{
  std::ostringstream out;
  for (std::size_t index = 0; index < columns.size(); ++index) {
    if (index != 0U) {
      out << ",";
    }
    out << columns[index];
  }
  return out.str();
}

/** @brief Converts a CSV column identifier to its array index. */
static std::size_t column(CsvColumn value)
{
  return static_cast<std::size_t>(value);
}

/** @brief Applies RFC-style quoting to one CSV field when required. */
static std::string escapeCsvField(const std::string& value)
{
  if (value.find_first_of("\",\r\n") == std::string::npos) {
    return value;
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

/** @brief Joins escaped fields into one CSV row. */
static std::string renderCsvRow(const CsvRow& fields)
{
  std::ostringstream out;
  for (std::size_t index = 0; index < fields.size(); ++index) {
    if (index != 0U) {
      out << ",";
    }
    out << escapeCsvField(fields[index]);
  }
  return out.str();
}

/** @brief Formats an integer as a zero-padded hexadecimal CSV field. */
static std::string hexValue(std::uint64_t value, std::uint32_t widthBytes)
{
  static constexpr char digits[] = "0123456789abcdef";
  const auto width = std::max<std::uint32_t>(2U, widthBytes * 2U);
  std::string out(width, '0');
  for (std::uint32_t index = 0; index < width; ++index) {
    out[width - 1U - index] = digits[value & 0x0fU];
    value >>= 4U;
  }
  return "0x" + out;
}

/** @brief Writes one optional raw DWT address fragment using its original width. */
static void writeDwtAddressFragment(CsvRow& row, CsvColumn target,
                                    const std::optional<DwtAddressFragment>& fragment)
{
  if (fragment.has_value()) {
    row[column(target)] = hexValue(fragment->value, fragment->size);
  }
}

/** @brief Maps a semantic exception action to its CSV value. */
static std::string_view exceptionActionCsvValue(ExceptionAction action)
{
  switch (action) {
  case ExceptionAction::Entered:
    return "0x1";
  case ExceptionAction::Exited:
    return "0x2";
  case ExceptionAction::Returned:
    return "0x3";
  case ExceptionAction::Unknown:
    return {};
  }
  return "0x0";
}

/** @brief Writes one ITM software packet to the CSV event columns. */
static void writePayloadColumns(CsvRow& row, const SoftwareTraceEvent& event)
{
  row[column(CsvColumn::Source)] = std::to_string(event.channel);
  row[column(CsvColumn::Value)] = hexValue(event.value, event.size);
}

/** @brief Writes one DWT data packet to the CSV event columns. */
static void writePayloadColumns(CsvRow& row, const DwtDataTraceEvent& event)
{
  row[column(CsvColumn::Source)] = std::to_string(event.comparator);
  row[column(CsvColumn::Value)] = hexValue(event.value, event.size);
  writeDwtAddressFragment(row, CsvColumn::Pc, event.pc);
  writeDwtAddressFragment(row, CsvColumn::Address, event.address);
}

/** @brief Writes one DWT address packet to the CSV event columns. */
static void writePayloadColumns(CsvRow& row, const DwtAddressTraceEvent& event)
{
  row[column(CsvColumn::Source)] = std::to_string(event.comparator);
  writeDwtAddressFragment(row, CsvColumn::Pc, dwtAddressPc(event));
  writeDwtAddressFragment(row, CsvColumn::Address, dwtDataAddress(event));
}

/** @brief Writes one comparator-only DWT match to the CSV event columns. */
static void writePayloadColumns(CsvRow& row, const DwtMatchTraceEvent& event)
{
  row[column(CsvColumn::Source)] = std::to_string(event.comparator);
}

/** @brief Writes one exception transition to the CSV event columns. */
static void writePayloadColumns(CsvRow& row, const ExceptionTraceEvent& event)
{
  row[column(CsvColumn::Source)] = std::to_string(event.number);
  row[column(CsvColumn::Value)] = exceptionActionCsvValue(event.action);
}

/** @brief Writes one DWT event-counter packet to the CSV event columns. */
static void writePayloadColumns(CsvRow& row, const DwtEventTraceEvent& event)
{
  row[column(CsvColumn::Source)] = "0";
  row[column(CsvColumn::Value)] = hexValue(event.counterMask, 1U);
}

/** @brief Writes one PMU trace-on-overflow packet to the CSV event columns. */
static void writePayloadColumns(CsvRow& row, const PmuTraceEvent& event)
{
  row[column(CsvColumn::Source)] = "3";
  row[column(CsvColumn::Value)] = hexValue(event.overflowMask, 1U);
}

/** @brief Writes one periodic PC sample to the CSV event columns. */
static void writePayloadColumns(CsvRow& row, const PcSampleTraceEvent& event)
{
  if (!event.sleeping) {
    row[column(CsvColumn::Pc)] = hexValue(event.pc, 4U);
  }
}

/** @brief Leaves local timestamp control packets without payload-specific CSV columns. */
static void writePayloadColumns(CsvRow&, const LocalTimestampTraceEvent&)
{
}

/** @brief Writes one global timestamp to the CSV cycle column. */
static void writePayloadColumns(CsvRow& row, const GlobalTimestampTraceEvent& event)
{
  row[column(CsvColumn::Cycles)] = std::to_string(event.value);
}

/** @brief Writes one overflow diagnostic to the CSV note column. */
static void writePayloadColumns(CsvRow& row, const OverflowTraceEvent& event)
{
  row[column(CsvColumn::Note)] = event.message.empty()
                                     ? "overflow: new timestamp segment; time across boundary may be unreliable"
                                     : event.message;
}

/** @brief Leaves synchronization control packets without payload-specific CSV columns. */
static void writePayloadColumns(CsvRow&, const SyncTraceEvent&)
{
}

/** @brief Writes one retained decoder issue to the CSV note column. */
static void writePayloadColumns(CsvRow& row, const TraceIssueEvent& event)
{
  row[column(CsvColumn::Note)] = event.message;
}

/** @brief Maps one semantic trace event to all CSV columns. */
static CsvRow eventToCsvRow(const TraceEvent& event)
{
  CsvRow row{};
  if (event.tcyc.has_value()) {
    row[column(CsvColumn::Cycles)] = std::to_string(*event.tcyc);
  }
  if (event.route.traceBusId.has_value()) {
    row[column(CsvColumn::Stream)] = std::to_string(*event.route.traceBusId);
  }
  if (const auto type = traceEventType(event)) {
    row[column(CsvColumn::Type)] = traceEventTypeName(*type);
  }

  std::visit([&row](const auto& payload) { writePayloadColumns(row, payload); }, event.payload);

  return row;
}

std::string CsvRowMapper::header()
{
  return joinColumns(kCsvColumnNames);
}

std::string CsvRowMapper::row(const TraceEvent& event)
{
  return renderCsvRow(eventToCsvRow(event));
}

std::string CsvRowMapper::byteSkipRow(const TraceByteSkip& skipped)
{
  CsvRow row{};
  if (skipped.traceId.has_value()) {
    row[column(CsvColumn::Stream)] = std::to_string(*skipped.traceId);
  }
  row[column(CsvColumn::Type)] = "info";
  row[column(CsvColumn::Note)] = traceByteSkipMessage(skipped);
  return renderCsvRow(row);
}

std::string CsvRowMapper::decodeAbortRow(const TraceDecodeAbort& failure)
{
  CsvRow row{};
  row[column(CsvColumn::Type)] = "error";
  row[column(CsvColumn::Note)] = traceDecodeAbortMessage(failure);
  return renderCsvRow(row);
}
