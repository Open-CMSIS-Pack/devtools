/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TraceCompassXmlWriter.hpp"

#include "CtfSchema.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <sstream>
#include <stdexcept>
#include <string>

constexpr const char* kTraceCompassAnalysisVersionPlaceholder = "__SWO_ANALYSIS_VERSION__";

static std::uint32_t traceCompassAnalysisVersion(const std::string& xml)
{
  constexpr std::uint32_t kFnvOffsetBasis = 2166136261U;
  constexpr std::uint32_t kFnvPrime = 16777619U;
  constexpr std::uint32_t kTraceCompassPositiveMask = 0x7fffffffU;

  std::uint32_t hash = kFnvOffsetBasis;
  for (const auto ch : xml) {
    hash ^= static_cast<std::uint8_t>(ch);
    hash *= kFnvPrime;
  }
  const auto version = hash & kTraceCompassPositiveMask;
  return version == 0U ? 1U : version; // LCOV_EXCL_BR_LINE: a zero hash is not constructible through the fixed XML
}

static std::string withTraceCompassAnalysisVersion(std::string xml)
{
  const auto version = std::to_string(traceCompassAnalysisVersion(xml));
  const auto placeholder = std::string(kTraceCompassAnalysisVersionPlaceholder);
  const auto position = xml.find(placeholder);
  xml.replace(position, placeholder.size(), version);
  return xml;
}

static std::string valueHandlers(CtfSchema::EventId eventId, const char* prefix, const char* routeField,
                                 const char* valueAttribute)
{
  std::ostringstream handlers;
  for (const auto& arm : CtfSchema::ValueVariants) {
    handlers << R"(            <stateChange>
                <if>
                    <condition>
                        <stateValue type="eventField" value="cmsis_)"
             << prefix << R"(_value_type" />
                        <stateValue type="string" value=")"
             << arm.name << R"(" />
                    </condition>
                </if>
                <then>
                    <stateAttribute type="constant" value=")"
             << CtfSchema::eventName(eventId) << R"(" />
                    <stateAttribute type="eventField" value=")"
             << routeField << R"(" />
                    <stateAttribute type="constant" value=")"
             << valueAttribute << R"(" />
                    <stateValue type="eventField" value="cmsis_)"
             << prefix << R"(_value.)" << arm.name << R"(" forcedType=")" << arm.traceCompassType << R"(" />
                </then>
            </stateChange>
)";
  }
  return handlers.str();
}

static std::string stateProviderXml()
{
  std::ostringstream xml;
  xml << R"(    <stateProvider version="__SWO_ANALYSIS_VERSION__" id="arm.cmsis.swo.analysis.v1">
        <head><label value="SWO Trace Analysis" /></head>
        <eventHandler eventName=")"
      << CtfSchema::eventName(CtfSchema::EventId::DwtValue)
      << R"(">
)"
      // LCOV_EXCL_START: GCC maps this covered raw-string expression to a continuation line
      << valueHandlers(CtfSchema::EventId::DwtValue, "dwt", "cmsis_dwt_comparator", "data")
      // LCOV_EXCL_STOP
      << R"(        </eventHandler>
        <eventHandler eventName=")"
      << CtfSchema::eventName(CtfSchema::EventId::DwtAddress) << R"(">
            <stateChange>
                <stateAttribute type="constant" value=")"
      << CtfSchema::eventName(CtfSchema::EventId::DwtAddress) << R"(" />
                <stateAttribute type="eventField" value="cmsis_dwt_comparator" />
                <stateAttribute type="constant" value="address" />
                <stateValue type="eventField" value="cmsis_address_lo16" forcedType="long" />
            </stateChange>
        </eventHandler>
        <eventHandler eventName=")"
      << CtfSchema::eventName(CtfSchema::EventId::Itm) << R"(">
)" << valueHandlers(CtfSchema::EventId::Itm, "itm", "cmsis_itm_channel", "value")
      << R"(        </eventHandler>
        <eventHandler eventName=")"
      << CtfSchema::eventName(CtfSchema::EventId::Exception) << R"(">
            <stateChange>
                <if>
                    <condition>
                        <stateValue type="eventField" value="cmsis_exception_action" />
                        <stateValue type="string" value="entered" />
                    </condition>
                </if>
                <then>
                    <stateAttribute type="constant" value=")"
      << CtfSchema::eventName(CtfSchema::EventId::Exception) << R"(" />
                    <stateAttribute type="eventField" value="cmsis_exception_number" />
                    <stateAttribute type="constant" value="action" />
                    <stateValue type="eventField" value="cmsis_exception_number_value" forcedType="long" />
                </then>
            </stateChange>
            <stateChange>
                <if>
                    <condition>
                        <stateValue type="eventField" value="cmsis_exception_action" />
                        <stateValue type="string" value="exited" />
                    </condition>
                </if>
                <then>
                    <stateAttribute type="constant" value=")"
      << CtfSchema::eventName(CtfSchema::EventId::Exception) << R"(" />
                    <stateAttribute type="eventField" value="cmsis_exception_number" />
                    <stateAttribute type="constant" value="action" />
                    <stateValue type="null" />
                </then>
            </stateChange>
        </eventHandler>
        <eventHandler eventName=")"
      << CtfSchema::eventName(CtfSchema::EventId::TraceStatus) << R"(">
            <stateChange>
                <stateAttribute type="constant" value=")"
      << CtfSchema::eventName(CtfSchema::EventId::TraceStatus) << R"(" />
                <stateAttribute type="eventField" value="cmsis_trace_status_reason" />
                <stateValue type="eventField" value="cmsis_trace_status_reason" />
            </stateChange>
        </eventHandler>
    </stateProvider>
)";
  return xml.str();
}

static std::string viewsXml()
{
  std::ostringstream xml;
  xml << R"(    <xyView id="arm.cmsis.swo.xy.dwt_value.v1">
        <head><analysis id="arm.cmsis.swo.analysis.v1" /><label value=")"
      << CtfSchema::eventName(CtfSchema::EventId::DwtValue) << R"(" /></head>
        <entry path=")"
      << CtfSchema::eventName(CtfSchema::EventId::DwtValue)
      << R"(/*"><display type="constant" value="data" /><name type="self" /></entry>
    </xyView>
    <xyView id="arm.cmsis.swo.xy.dwt_addr.v1">
        <head><analysis id="arm.cmsis.swo.analysis.v1" /><label value=")"
      << CtfSchema::eventName(CtfSchema::EventId::DwtAddress) << R"(" /></head>
        <entry path=")"
      << CtfSchema::eventName(CtfSchema::EventId::DwtAddress)
      << R"(/*"><display type="constant" value="address" /><name type="self" /></entry>
    </xyView>
    <timeGraphView id="arm.cmsis.swo.tg.itm.v1">
        <head><analysis id="arm.cmsis.swo.analysis.v1" /><label value=")"
      << CtfSchema::eventName(CtfSchema::EventId::Itm) << R"(" /></head>
        <entry path=")"
      << CtfSchema::eventName(CtfSchema::EventId::Itm)
      << R"(/*" displayText="true"><display type="constant" value="value" /><name type="self" /></entry>
    </timeGraphView>
    <timeGraphView id="arm.cmsis.swo.tg.exception.v1">
        <head><analysis id="arm.cmsis.swo.analysis.v1" /><label value=")"
      << CtfSchema::eventName(CtfSchema::EventId::Exception) << R"(" /></head>
        <entry path=")"
      << CtfSchema::eventName(CtfSchema::EventId::Exception)
      << R"(/*" displayText="false"><display type="constant" value="action" /><name type="self" /></entry>
    </timeGraphView>
    <timeGraphView id="arm.cmsis.swo.tg.trace_status.v1">
        <head><analysis id="arm.cmsis.swo.analysis.v1" /><label value=")"
      << CtfSchema::eventName(CtfSchema::EventId::TraceStatus) << R"(" /></head>
        <entry path=")"
      << CtfSchema::eventName(CtfSchema::EventId::TraceStatus)
      << R"(/*" displayText="true"><display type="self" /><name type="self" /></entry>
    </timeGraphView>
)";
  return xml.str();
}

static std::string traceCompassXml()
{
  std::ostringstream xml;
  xml << R"(<?xml version="1.0" encoding="UTF-8"?>
<tmfxml xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    xsi:noNamespaceSchemaLocation="xmlDefinition.xsd">
)" << stateProviderXml() // LCOV_EXCL_LINE: GCC raw-string mapping artifact
      << viewsXml() << R"(</tmfxml>
)";
  return withTraceCompassAnalysisVersion(xml.str());
}

void TraceCompassXmlWriter::writeFile(const std::filesystem::path& filePath)
{
  if (!filePath.parent_path().empty()) {
    std::filesystem::create_directories(filePath.parent_path());
  }
  std::ofstream out(filePath, std::ios::out | std::ios::trunc);
  if (!out) { // LCOV_EXCL_BR_LINE: covered with a directory target
    throw std::runtime_error("Failed to write Trace Compass XML " + filePath.string()); // LCOV_EXCL_LINE
  }
  out << traceCompassXml();
  out.close();
  if (!out) { // LCOV_EXCL_BR_LINE: covered on Linux with /dev/full
    throw std::runtime_error("Failed to write Trace Compass XML " + filePath.string()); // LCOV_EXCL_LINE
  }
}
