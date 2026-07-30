/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CsvRowMapper.hpp"

#include "TraceEvent.hpp"
#include "TraceSelection.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>

namespace {

enum class CsvColumn : std::size_t {
  Cycles,
  Stream,
  Type,
  Source,
  Value,
  Pc,
  Offset,
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
    "offset",
    "note",
}};

using CsvRow = std::array<std::string, static_cast<std::size_t>(CsvColumn::Count)>;

template <typename Columns> std::string joinColumns(const Columns& columns)
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

std::size_t column(CsvColumn value)
{
  return static_cast<std::size_t>(value);
}

std::string escapeCsvField(const std::string& value)
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

std::string renderCsvRow(const CsvRow& fields)
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

std::string hexValue(std::uint64_t value, std::uint32_t widthBytes)
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

std::string_view exceptionActionCsvValue(ExceptionAction action)
{
  switch (action) {
  case ExceptionAction::Entered:
    return "0x1";
  case ExceptionAction::Exited:
    return "0x2";
  case ExceptionAction::Returned:
    return "0x3";
  case ExceptionAction::Unknown:
    return "0x0";
  }
  return "0x0";
}

CsvRow eventToCsvRow(const TraceEvent& event)
{
  CsvRow row{};
  if (event.tcyc.has_value()) {
    row[column(CsvColumn::Cycles)] = std::to_string(*event.tcyc);
  }
  row[column(CsvColumn::Stream)] = std::to_string(event.traceBusId);
  if (const auto type = traceEventType(event)) {
    row[column(CsvColumn::Type)] = traceEventTypeName(*type);
  }

  if (const auto* software = traceEventPayload<SoftwareTraceEvent>(event)) {
    row[column(CsvColumn::Source)] = std::to_string(software->channel);
    row[column(CsvColumn::Value)] = hexValue(software->value, software->size);
  } else if (const auto* data = traceEventPayload<DwtDataTraceEvent>(event)) {
    row[column(CsvColumn::Source)] = std::to_string(data->comparator);
    row[column(CsvColumn::Value)] = hexValue(data->value, data->size);
    if (data->pc.has_value()) {
      row[column(CsvColumn::Pc)] = hexValue(*data->pc, 4);
    }
    if (data->addressLo16.has_value()) {
      row[column(CsvColumn::Offset)] = hexValue(*data->addressLo16, 2);
    }
  } else if (const auto* address = traceEventPayload<DwtAddressTraceEvent>(event)) {
    row[column(CsvColumn::Source)] = std::to_string(address->comparator);
    if (const auto pc = dwtAddressPc(*address)) {
      row[column(CsvColumn::Pc)] = hexValue(*pc, 4);
    }
    if (const auto offset = dwtAddressOffset(*address)) {
      row[column(CsvColumn::Offset)] = hexValue(*offset, 2);
    }
  } else if (const auto* exception = traceEventPayload<ExceptionTraceEvent>(event)) {
    row[column(CsvColumn::Source)] = std::to_string(exception->number);
    if (exception->action != ExceptionAction::Unknown) {
      row[column(CsvColumn::Value)] = exceptionActionCsvValue(exception->action);
    }
  } else if (const auto* timestamp = traceEventPayload<GlobalTimestampTraceEvent>(event)) {
    row[column(CsvColumn::Cycles)] = std::to_string(timestamp->value);
  } else if (const auto* overflow = traceEventPayload<OverflowTraceEvent>(event)) {
    row[column(CsvColumn::Note)] = overflow->message.empty()
                                       ? "overflow: new timestamp segment; time across boundary may be unreliable"
                                       : overflow->message;
  } else if (const auto* issue = traceEventPayload<TraceIssueEvent>(event)) {
    row[column(CsvColumn::Note)] = issue->message;
  }

  return row;
}

} // namespace

std::string CsvRowMapper::header()
{
  return joinColumns(kCsvColumnNames);
}

std::string CsvRowMapper::row(const TraceEvent& event)
{
  return renderCsvRow(eventToCsvRow(event));
}
