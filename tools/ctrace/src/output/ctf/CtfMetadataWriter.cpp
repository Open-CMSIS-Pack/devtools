/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CtfMetadataWriter.h"

#include "CtfSchema.h"
#include "TraceOutputConfig.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <limits>
#include <map>
#include <ostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

/** @brief Escapes one string for use in TSDL metadata. */
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

/** @brief Formats an integer as an unsigned hexadecimal TSDL value. */
static std::string hexValue(std::uint64_t value)
{
  std::ostringstream out;
  out << std::hex << std::uppercase << value;
  return out.str();
}

/** @brief Allocates a stable unique label for one metadata enumeration value. */
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

/** @brief Returns a mapped metadata label or an empty string. */
static std::string mapValueOrEmpty(const std::map<std::uint32_t, std::string>& values, std::uint32_t key)
{
  const auto found = values.find(key);
  return found == values.end() ? "" : found->second;
}

static std::vector<ExceptionNumber>
exceptionNumbersWithDefaults(const std::vector<ExceptionNumber>& observedExceptionNumbers)
{
  std::vector<ExceptionNumber> exceptions = {0, 1, 2, 3, 4, 5, 6, 7, 11, 12, 14, 15};
  for (const auto number : observedExceptionNumbers) {
    if (std::find(exceptions.begin(), exceptions.end(), number) == exceptions.end()) {
      exceptions.push_back(number);
    }
  }
  std::sort(exceptions.begin(), exceptions.end());
  return exceptions;
}

/** @brief Returns the architectural name for a known Cortex-M exception. */
static std::string exceptionName(ExceptionNumber number)
{
  switch (number) {
  case 0:
    return "Thread Mode";
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
    if (number >= 16U) {
      return "External IRQ " + std::to_string(number - 16U);
    }
    return "Reserved " + std::to_string(number);
  }
}

/** @brief Returns the TSDL scalar type used by one value variant. */
static std::string_view tsdlValueType(const CtfSchema::ValueVariant& variant)
{
  if (variant.floatingPoint) {
    return "ieee_float32_t";
  }
  if (variant.signedInteger) {
    return variant.byteSize == 1U ? "int8_t" : variant.byteSize == 2U ? "int16_t" : "int32_t";
  }
  return variant.byteSize == 1U ? "uint8_t" : variant.byteSize == 2U ? "uint16_t" : "uint32_t";
}

/** @brief Generates the variant-specific TSDL fields for sample values. */
static std::string ctfValueFields(const std::string_view& prefix)
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
  out << " } m_cmsis" << prefix << "_value_type;\n"
      << "        variant <m_cmsis" << prefix << "_value_type> {\n";
  for (const auto& variant : CtfSchema::ValueVariants) {
    out << "            " << tsdlValueType(variant) << " " << variant.name << ";\n";
  }
  out << "        } m_cmsis" << prefix << "_value;\n";
  return out.str();
}

/** @brief Generates the width-tagged TSDL field for a raw DWT address offset. */
static std::string ctfDwtOffsetFields()
{
  std::ostringstream out;
  out << "        enum : uint8_t { ";
  for (std::size_t index = 0; index < CtfSchema::DwtOffsetVariants.size(); ++index) {
    const auto& variant = CtfSchema::DwtOffsetVariants[index];
    if (index > 0U) {
      out << ", ";
    }
    out << variant.name << " = " << static_cast<unsigned>(CtfSchema::value(variant.tag));
  }
  out << " } cmsis_dwt_offset_type;\n"
      << "        variant <cmsis_dwt_offset_type> {\n";
  for (const auto& variant : CtfSchema::DwtOffsetVariants) {
    const auto type = variant.byteSize == 1U ? "uint8_t" : variant.byteSize == 2U ? "uint16_t" : "uint32_t";
    out << "            " << type << " " << variant.name << ";\n";
  }
  out << "        } cmsis_dwt_offset;\n";
  return out.str();
}

/** @brief Stores source labels and exception lanes emitted into CTF metadata. */
struct MetadataSymbols {
  std::map<std::uint32_t, std::string> dwtValueTypes;
  std::map<std::uint32_t, std::string> itmNames;
  std::map<std::uint32_t, std::string> dwtNames;
  std::map<std::uint32_t, std::uint64_t> dwtAddressStarts;
  std::map<std::uint32_t, std::uint64_t> dwtAddressEnds;
};

/** @brief Collects deduplicated source and exception symbols for metadata. */
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
    symbols.dwtValueTypes[id] = source.dataType;
    if (source.label.has_value()) {
      symbols.dwtNames[id] = *source.label;
    }
    if (source.address.has_value()) {
      symbols.dwtAddressStarts[id] = *source.address;
      const auto extent = static_cast<std::uint64_t>(source.dataSize - 1U);
      if (*source.address <= std::numeric_limits<std::uint64_t>::max() - extent) {
        symbols.dwtAddressEnds[id] = *source.address + extent;
      }
    }
  }
  return symbols;
}

/** @brief Writes the TSDL trace, environment, and clock declarations. */
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

/** @brief Writes reusable TSDL type and enumeration declarations. */
static void writeTypeDefinitions(std::ostream& out, const MetadataSymbols& symbols,
                                 const std::vector<ExceptionNumber>& observedExceptionNumbers)
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
      << static_cast<unsigned>(CtfSchema::value(CtfSchema::ExceptionAction::Exited)) << R"(,
    "returned" = )"
      << static_cast<unsigned>(CtfSchema::value(CtfSchema::ExceptionAction::Returned)) << R"(
} := cmsis_exception_action_t;
typealias enum : uint8_t {
    "trace" = )"
      << static_cast<unsigned>(CtfSchema::value(CtfSchema::ExceptionOrigin::Trace)) << R"(,
    "synthetic" = )"
      << static_cast<unsigned>(CtfSchema::value(CtfSchema::ExceptionOrigin::Synthetic)) << R"(
} := cmsis_exception_origin_t;
typealias enum : uint8_t {
)";
  for (const auto counter : kDwtEventCounters) {
    out << "    \"" << CtfSchema::dwtEventCounterName(counter) << "\" = "
        << static_cast<unsigned>(CtfSchema::value(counter)) << ",\n";
  }
  out << R"(} := cmsis_dwt_event_counter_t;
typealias enum : uint8_t {
)";
  for (const auto counter : kPmuEventCounters) {
    out << "    \"" << CtfSchema::pmuEventCounterName(counter) << "\" = "
        << static_cast<unsigned>(CtfSchema::value(counter)) << ",\n";
  }
  out << R"(} := cmsis_pmu_event_counter_t;
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

/** @brief Writes the packet and event context definition for the SWO stream. */
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

/** @brief Writes the ITM software event declaration. */
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

/** @brief Writes the DWT value event declaration. */
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
)" << ctfDwtOffsetFields()
      << R"(        uint8_t cmsis_sample_flags;
        uint32_t cmsis_overflow_count;
    };
};
)";
}

/** @brief Writes the DWT address event declaration. */
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
        uint32_t cmsis_pc[cmsis_has_pc];
)" << ctfDwtOffsetFields()
      << R"(        uint8_t cmsis_sample_flags;
        uint32_t cmsis_overflow_count;
    };
};
)";
}

/** @brief Writes the comparator-only DWT match event declaration. */
static void writeDwtMatchEvent(std::ostream& out)
{
  out << R"(
event {
    id = )"
      << CtfSchema::value(CtfSchema::EventId::DwtMatch) << R"(;
    name = ")"
      << CtfSchema::eventName(CtfSchema::EventId::DwtMatch) << R"(";
    stream_id = )"
      << CtfSchema::SwoStreamId << R"(;
    fields := struct {
        cmsis_dwt_comparator_t cmsis_dwt_comparator;
        uint8_t cmsis_sample_flags;
        uint32_t cmsis_overflow_count;
    };
};
)";
}

/** @brief Writes the DWT event-counter declaration. */
static void writeDwtEvent(std::ostream& out)
{
  out << R"(
event {
    id = )"
      << CtfSchema::value(CtfSchema::EventId::DwtEvent) << R"(;
    name = ")"
      << CtfSchema::eventName(CtfSchema::EventId::DwtEvent) << R"(";
    stream_id = )"
      << CtfSchema::SwoStreamId << R"(;
    fields := struct {
        cmsis_dwt_event_counter_t cmsis_dwt_event_counter;
        uint8_t cmsis_sample_flags;
        uint32_t cmsis_overflow_count;
    };
};
)";
}

/** @brief Writes the programmable PMU event-counter declaration. */
static void writePmuEvent(std::ostream& out)
{
  out << R"(
event {
    id = )"
      << CtfSchema::value(CtfSchema::EventId::PmuEvent) << R"(;
    name = ")"
      << CtfSchema::eventName(CtfSchema::EventId::PmuEvent) << R"(";
    stream_id = )"
      << CtfSchema::SwoStreamId << R"(;
    fields := struct {
        cmsis_pmu_event_counter_t cmsis_pmu_event_counter;
        uint8_t cmsis_sample_flags;
        uint32_t cmsis_overflow_count;
    };
};
)";
}

/** @brief Writes the periodic PC-sample event declaration. */
static void writePcSampleEvent(std::ostream& out)
{
  out << R"(
event {
    id = )"
      << CtfSchema::value(CtfSchema::EventId::PcSample) << R"(;
    name = ")"
      << CtfSchema::eventName(CtfSchema::EventId::PcSample) << R"(";
    stream_id = )"
      << CtfSchema::SwoStreamId << R"(;
    fields := struct {
        uint8_t cmsis_pc_sample_state;
        uint32_t cmsis_pc[cmsis_pc_sample_state];
        uint8_t cmsis_sample_flags;
        uint32_t cmsis_overflow_count;
    };
};
)";
}

/** @brief Writes status, exception, and global timestamp declarations. */
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
        cmsis_exception_origin_t cmsis_exception_origin;
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
                              const std::vector<ExceptionNumber>& observedExceptionNumbers)
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
  writeDwtMatchEvent(out);
  writeDwtEvent(out);
  writePmuEvent(out);
  writeStatusEvents(out);
  writePcSampleEvent(out);
  out.close();
  if (!out) {
    throw std::runtime_error("Failed to write CTF metadata " + metadataPath.string());
  }
}
