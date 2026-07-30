/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CtfMetadataWriter.hpp"

#include "CtfSchema.hpp"
#include "TraceOutputConfig.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

static std::string tsdlString(const std::string& value)
{
  static constexpr char hexDigits[] = "0123456789ABCDEF";
  std::string out = "\"";
  for (const auto raw : value) {
    const auto ch = static_cast<unsigned char>(raw);
    if (ch == '\\' || ch == '"') {
      out.push_back('\\');
      out.push_back(static_cast<char>(ch));
    } else if (ch == '\n') {
      out += "\\n";
    } else if (ch == '\r') {
      out += "\\r";
    } else if (ch == '\t') {
      out += "\\t";
    } else if (ch < 0x20U || ch == 0x7fU) {
      out += "\\x";
      out.push_back(hexDigits[(ch >> 4U) & 0x0fU]);
      out.push_back(hexDigits[ch & 0x0fU]);
    } else {
      out.push_back(static_cast<char>(ch));
    }
  }
  out.push_back('"');
  return out;
}

static std::string hexValue(std::uint64_t value)
{
  std::ostringstream out;
  out << std::hex << std::uppercase << value;
  return out.str();
}

static std::string uniqueEnumLabel(const std::string& preferred, const std::string& fallback,
                                   std::set<std::string>& used)
{
  auto candidate = preferred.empty() ? fallback : preferred;
  if (used.insert(candidate).second) {
    return candidate;
  }
  if (used.insert(fallback).second) {
    return fallback;
  }
  for (std::uint32_t suffix = 1;; ++suffix) {
    const auto label = fallback + "_" + std::to_string(suffix);
    if (used.insert(label).second) {
      return label;
    }
  }
}

static std::string mapValueOrEmpty(const std::map<std::uint32_t, std::string>& values, std::uint32_t key)
{
  const auto found = values.find(key);
  return found == values.end() ? "" : found->second;
}

static std::vector<std::uint32_t>
exceptionNumbersWithDefaults(const std::vector<std::uint32_t>& observedExceptionNumbers)
{
  std::vector<std::uint32_t> exceptions = {0, 1, 2, 3, 4, 5, 6, 7, 11, 12, 14, 15};
  for (const auto number : observedExceptionNumbers) {
    if (std::find(exceptions.begin(), exceptions.end(), number) == exceptions.end()) {
      exceptions.push_back(number);
    }
  }
  std::sort(exceptions.begin(), exceptions.end());
  return exceptions;
}

static std::string exceptionName(std::uint32_t number)
{
  switch (number) {
  case 0:
    return "ThreadMode";
  case 1:
    return "Reset";
  case 2:
    return "NMI";
  case 3:
    return "HardFault";
  case 4:
    return "MemManage";
  case 5:
    return "BusFault";
  case 6:
    return "UsageFault";
  case 7:
    return "SecureFault";
  case 11:
    return "SVCall";
  case 12:
    return "DebugMonitor";
  case 14:
    return "PendSV";
  case 15:
    return "SysTick";
  default:
    return "External IRQ " + std::to_string(number);
  }
}

static std::string_view tsdlValueType(const CtfSchema::ValueVariant& variant)
{
  if (variant.floatingPoint) {
    return "ieee_float32_t";
  }
  if (variant.tag == CtfSchema::ValueTag::Character8) {
    return "uint8_t";
  }
  if (variant.signedInteger) {
    return variant.byteSize == 1U ? "int8_t" : variant.byteSize == 2U ? "int16_t" : "int32_t";
  }
  return variant.byteSize == 1U ? "uint8_t" : variant.byteSize == 2U ? "uint16_t" : "uint32_t";
}

static std::string ctfValueFields(std::string_view prefix)
{
  std::ostringstream out;
  out << "        enum : uint8_t { ";
  for (std::size_t index = 0; index < CtfSchema::ValueVariants.size(); ++index) {
    const auto& variant = CtfSchema::ValueVariants[index];
    if (index > 0U) {
      out << ", ";
    }
    out << variant.name << " = " << static_cast<unsigned>(CtfSchema::value(variant.tag));
  }
  out << " } cmsis_" << prefix << "_value_type;\n"
      << "        variant <cmsis_" << prefix << "_value_type> {\n";
  for (const auto& variant : CtfSchema::ValueVariants) {
    out << "            " << tsdlValueType(variant) << " " << variant.name << ";\n";
  }
  out << "        } cmsis_" << prefix << "_value;\n";
  return out.str();
}

struct MetadataSymbols {
  std::map<std::uint32_t, std::string> dwtValueTypes;
  std::map<std::uint32_t, std::string> itmNames;
  std::map<std::uint32_t, std::string> dwtNames;
  std::map<std::uint32_t, std::uint64_t> dwtAddressStarts;
  std::map<std::uint32_t, std::uint64_t> dwtAddressEnds;
};

static MetadataSymbols collectMetadataSymbols(const std::vector<ResolvedTraceSource>& sources)
{
  MetadataSymbols symbols;
  for (const auto& source : sources) {
    const auto id = source.source;
    if (source.type == "itm") {
      if (source.label.has_value()) {
        symbols.itmNames[id] = *source.label;
      }
      continue;
    }
    if (source.type != "dwt") {
      continue;
    }
    symbols.dwtValueTypes[id] = source.valueType;
    if (source.label.has_value()) {
      symbols.dwtNames[id] = *source.label;
    }
    if (source.symbolAddress.has_value()) {
      symbols.dwtAddressStarts[id] = *source.symbolAddress;
      const auto extent = static_cast<std::uint64_t>(source.valueSize - 1U);
      if (*source.symbolAddress <= std::numeric_limits<std::uint64_t>::max() - extent) {
        symbols.dwtAddressEnds[id] = *source.symbolAddress + extent;
      }
    }
  }
  return symbols;
}

static void writeTraceEnvironment(std::ostream& out, const std::string& uuidString, std::uint64_t coreClockHz,
                                  const MetadataSymbols& symbols)
{
  out << R"(/* CTF 1.8 */
trace {
    major = 1;
    minor = 8;
    uuid = ")"
      << uuidString << R"(";
    byte_order = le;
    packet.header := struct {
        integer { size = 32; align = 8; signed = false; } magic;
        integer { size = 8; align = 8; signed = false; } uuid[16];
        integer { size = 32; align = 8; signed = false; } stream_id;
    };
};

env {
    cmsis_ctf_profile = "cmsis.ctf";
    cmsis_ctf_profile_version = 1;
)";
  for (const auto& entry : symbols.dwtValueTypes) {
    out << "    cmsis_dwt" << entry.first << "_value_type = " << tsdlString(entry.second) << ";\n";
  }
  for (const auto& entry : symbols.dwtAddressStarts) {
    out << "    cmsis_dwt" << entry.first << "_address_start = " << tsdlString("0x" + hexValue(entry.second)) << ";\n";
  }
  for (const auto& entry : symbols.dwtAddressEnds) {
    out << "    cmsis_dwt" << entry.first << "_address_end = " << tsdlString("0x" + hexValue(entry.second)) << ";\n";
  }
  out << R"(};

clock {
    name = swo_clock;
    precision = 0;
    offset_s = 0;
    offset = 0;
    absolute = false;
    freq = )"
      << coreClockHz << R"(;
};
)";
}

static void writeTypeDefinitions(std::ostream& out, const MetadataSymbols& symbols,
                                 const std::vector<std::uint32_t>& observedExceptionNumbers)
{
  out << R"(
typealias integer { size = 8; align = 8; signed = false; } := uint8_t;
typealias integer { size = 16; align = 8; signed = false; byte_order = le; } := uint16_t;
typealias integer { size = 32; align = 8; signed = false; byte_order = le; } := uint32_t;
typealias integer { size = 8; align = 8; signed = true; } := int8_t;
typealias integer { size = 16; align = 8; signed = true; byte_order = le; } := int16_t;
typealias integer { size = 32; align = 8; signed = true; byte_order = le; } := int32_t;
typealias floating_point { exp_dig = 8; mant_dig = 24; align = 8; byte_order = le; } := ieee_float32_t;
typealias integer { size = 64; align = 8; signed = false; byte_order = le; } := uint64_t;
typealias integer { size = 64; align = 8; signed = false; map = clock.swo_clock.value; } := swo_clock_t;

typealias enum : uint8_t {
    "read" = )"
      << static_cast<unsigned>(CtfSchema::value(CtfSchema::DwtAccess::Read)) << R"(,
    "write" = )"
      << static_cast<unsigned>(CtfSchema::value(CtfSchema::DwtAccess::Write)) << R"(
} := cmsis_dwt_access_t;
typealias enum : uint8_t {
    "trace_start" = )"
      << static_cast<unsigned>(CtfSchema::value(CtfSchema::TraceStatusReason::TraceStart)) << R"(,
    "resync" = )"
      << static_cast<unsigned>(CtfSchema::value(CtfSchema::TraceStatusReason::Resync)) << R"(,
    "overflow" = )"
      << static_cast<unsigned>(CtfSchema::value(CtfSchema::TraceStatusReason::Overflow)) << R"(,
    "decode_error" = )"
      << static_cast<unsigned>(CtfSchema::value(CtfSchema::TraceStatusReason::DecodeError)) << R"(,
    "data_loss" = )"
      << static_cast<unsigned>(CtfSchema::value(CtfSchema::TraceStatusReason::DataLoss)) << R"(
} := cmsis_trace_status_reason_t;
typealias enum : uint8_t {
    "entered" = )"
      << static_cast<unsigned>(CtfSchema::value(CtfSchema::ExceptionAction::Entered)) << R"(,
    "exited" = )"
      << static_cast<unsigned>(CtfSchema::value(CtfSchema::ExceptionAction::Exited)) << R"(
} := cmsis_exception_action_t;
typealias enum : uint8_t {
)";
  std::set<std::string> itmLabels;
  for (std::uint32_t channel = 1U; channel < 32U; ++channel) {
    const auto fallback = "ITM" + std::to_string(channel);
    out << "    " << tsdlString(uniqueEnumLabel(mapValueOrEmpty(symbols.itmNames, channel), fallback, itmLabels))
        << " = " << channel << ",\n";
  }
  out << R"(} := cmsis_itm_channel_t;
typealias enum : uint8_t {
)";
  std::set<std::string> dwtLabels;
  for (std::uint32_t comparator = 0U; comparator < 4U; ++comparator) {
    const auto label = mapValueOrEmpty(symbols.dwtNames, comparator);
    const auto fallback = "DWT" + std::to_string(comparator);
    out << "    " << tsdlString(uniqueEnumLabel(label, fallback, dwtLabels)) << " = " << comparator << ",\n";
  }
  out << R"(
} := cmsis_dwt_comparator_t;
typealias enum : uint16_t {
)";
  for (const auto number : exceptionNumbersWithDefaults(observedExceptionNumbers)) {
    out << "    " << tsdlString(exceptionName(number)) << " = " << number << ",\n";
  }
  out << R"(} := cmsis_exception_number_t;
)";
}

static void writeStreamDefinition(std::ostream& out)
{
  out << R"(
stream {
    id = )"
      << CtfSchema::SwoStreamId << R"(;
    event.header := struct {
        uint32_t id;
        swo_clock_t timestamp;
    };
    event.context := struct {
        uint8_t cmsis_trace_bus_id;
    };
    packet.context := struct {
        uint32_t packet_size;
        uint32_t content_size;
        swo_clock_t timestamp_begin;
        swo_clock_t timestamp_end;
        uint32_t events_discarded;
        uint32_t packet_seq_num;
    };
};
)";
}

static void writeItmEvent(std::ostream& out)
{
  out << R"(
event {
    id = )"
      << CtfSchema::value(CtfSchema::EventId::Itm) << R"(;
    name = ")"
      << CtfSchema::eventName(CtfSchema::EventId::Itm) << R"(";
    stream_id = )"
      << CtfSchema::SwoStreamId << R"(;
    fields := struct {
        cmsis_itm_channel_t cmsis_itm_channel;
)" << ctfValueFields("itm")
      << R"(        uint8_t cmsis_sample_flags;
        uint32_t cmsis_overflow_count;
    };
};
)";
}

static void writeDwtValueEvent(std::ostream& out)
{
  out << R"(
event {
    id = )"
      << CtfSchema::value(CtfSchema::EventId::DwtValue) << R"(;
    name = ")"
      << CtfSchema::eventName(CtfSchema::EventId::DwtValue) << R"(";
    stream_id = )"
      << CtfSchema::SwoStreamId << R"(;
    fields := struct {
        cmsis_dwt_comparator_t cmsis_dwt_comparator;
        cmsis_dwt_access_t cmsis_dwt_access;
)" << ctfValueFields("dwt")
      << R"(        uint8_t cmsis_has_pc;
        uint32_t cmsis_pc[cmsis_has_pc];
        uint8_t cmsis_has_address_lo16;
        uint16_t cmsis_address_lo16[cmsis_has_address_lo16];
        uint8_t cmsis_sample_flags;
        uint32_t cmsis_overflow_count;
    };
};
)";
}

static void writeDwtAddressEvent(std::ostream& out)
{
  out << R"(
event {
    id = )"
      << CtfSchema::value(CtfSchema::EventId::DwtAddress) << R"(;
    name = ")"
      << CtfSchema::eventName(CtfSchema::EventId::DwtAddress) << R"(";
    stream_id = )"
      << CtfSchema::SwoStreamId << R"(;
    fields := struct {
        cmsis_dwt_comparator_t cmsis_dwt_comparator;
        uint8_t cmsis_has_pc;
        uint8_t cmsis_has_address_lo16;
        uint32_t cmsis_pc;
        uint16_t cmsis_address_lo16;
        uint8_t cmsis_sample_flags;
        uint32_t cmsis_overflow_count;
    };
};
)";
}

static void writeStatusEvents(std::ostream& out)
{
  out << R"(
event {
    id = )"
      << CtfSchema::value(CtfSchema::EventId::TraceStatus) << R"(;
    name = ")"
      << CtfSchema::eventName(CtfSchema::EventId::TraceStatus) << R"(";
    stream_id = )"
      << CtfSchema::SwoStreamId << R"(;
    fields := struct {
        cmsis_trace_status_reason_t cmsis_trace_status_reason;
        uint32_t cmsis_overflow_count;
    };
};

event {
    id = )"
      << CtfSchema::value(CtfSchema::EventId::Exception) << R"(;
    name = ")"
      << CtfSchema::eventName(CtfSchema::EventId::Exception) << R"(";
    stream_id = )"
      << CtfSchema::SwoStreamId << R"(;
    fields := struct {
        cmsis_exception_number_t cmsis_exception_number;
        cmsis_exception_action_t cmsis_exception_action;
        uint16_t cmsis_exception_number_value;
    };
};

event {
    id = )"
      << CtfSchema::value(CtfSchema::EventId::GlobalTimestamp) << R"(;
    name = ")"
      << CtfSchema::eventName(CtfSchema::EventId::GlobalTimestamp) << R"(";
    stream_id = )"
      << CtfSchema::SwoStreamId << R"(;
    fields := struct {
        uint64_t cmsis_global_timestamp;
        uint8_t cmsis_clock_change;
    };
};
)";
}

void CtfMetadataWriter::write(const std::filesystem::path& outputDir, const std::string& uuidString,
                              std::uint64_t coreClockHz, const std::vector<ResolvedTraceSource>& sources,
                              const std::vector<std::uint32_t>& observedExceptionNumbers)
{
  const auto metadataPath = outputDir / "metadata";
  std::ofstream out(metadataPath, std::ios::out | std::ios::trunc);
  if (!out) {
    throw std::runtime_error("Failed to write CTF metadata " + metadataPath.string());
  }

  const auto symbols = collectMetadataSymbols(sources);
  writeTraceEnvironment(out, uuidString, coreClockHz, symbols);
  writeTypeDefinitions(out, symbols, observedExceptionNumbers);
  writeStreamDefinition(out);
  writeItmEvent(out);
  writeDwtValueEvent(out);
  writeDwtAddressEvent(out);
  writeStatusEvents(out);
  out.close();
  if (!out) {
    throw std::runtime_error("Failed to write CTF metadata " + metadataPath.string());
  }
}
