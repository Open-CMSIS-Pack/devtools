/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TraceCompassXmlWriter.h"

#include "CtfSchema.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

constexpr const char* kTraceCompassAnalysisVersionPlaceholder = "__SWO_ANALYSIS_VERSION__";
// Stack depth keeps overlapping visual pulses active until their last scheduled pop.
constexpr std::uint64_t kEventPulseNanoseconds = 1000U;

/** @brief Reads the analysis version encoded in generated Trace Compass XML. */
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
  return version == 0U ? 1U : version;
}

/** @brief Inserts the current analysis version into generated XML. */
static std::string withTraceCompassAnalysisVersion(std::string xml)
{
  const auto version = std::to_string(traceCompassAnalysisVersion(xml));
  const auto placeholder = std::string(kTraceCompassAnalysisVersionPlaceholder);
  const auto position = xml.find(placeholder);
  xml.replace(position, placeholder.size(), version);
  return xml;
}

/** @brief Renders route path components while generating a formatted-stream analysis. */
static std::string statePathPrefix(bool routePrefixed)
{
  return routePrefixed
             ? "                    <stateAttribute type=\"eventField\" value=\"context.ctrace_route\" />\n"
               "                    <stateAttribute type=\"eventField\" value=\"context.cmsis_trace_bus_id\" />\n"
             : "";
}

/** @brief Generates Trace Compass value handlers for one CTF event route. */
static std::string valueHandlers(CtfSchema::EventId eventId, const char* prefix, const char* routeField,
                                 const char* valueAttribute, bool routePrefixed)
{
  std::ostringstream handlers;
  for (const auto& arm : CtfSchema::ValueVariants) {
    handlers << R"(            <stateChange>
                <if>
                    <condition>
                        <stateValue type="eventField" value="m_cmsis)"
             << prefix << R"(_value_type" />
                        <stateValue type="string" value=")"
             << arm.name << R"(" />
                    </condition>
                </if>
                <then>
)";
    handlers << statePathPrefix(routePrefixed);
    handlers << R"(                    <stateAttribute type="constant" value=")"
             << CtfSchema::eventName(eventId) << R"(" />
                    <stateAttribute type="eventField" value=")"
             << routeField << R"(" />
                    <stateAttribute type="constant" value=")"
             << valueAttribute << R"(" />
                    <stateValue type="eventField" value="m_cmsis)"
             << prefix << R"(_value.)" << arm.name << R"(" forcedType=")" << arm.traceCompassType << R"(" />
                </then>
            </stateChange>
)";
  }
  return handlers.str();
}

/** @brief Generates visible pulses for one family of event counters. */
template <typename Counter, std::size_t Size, typename CounterName>
static std::string eventCounterHandlers(CtfSchema::EventId eventId, std::string_view field,
                                        const std::array<Counter, Size>& counters, CounterName counterName,
                                        bool routePrefixed)
{
  std::ostringstream handlers;
  for (const auto counter : counters) {
    const auto name = counterName(counter);
    const auto value = static_cast<unsigned>(CtfSchema::value(counter));
    handlers << R"(            <stateChange>
                <if>
                    <condition>
                        <stateValue type="eventField" value=")"
             << field << R"(" />
                        <stateValue type="string" value=")"
             << name << R"(" />
                    </condition>
                </if>
                <then>
)"
             << statePathPrefix(routePrefixed) << R"(                    <stateAttribute type="constant" value=")"
             << CtfSchema::eventName(eventId) << R"(" />
                    <stateAttribute type="constant" value=")"
             << name << R"(" />
                    <stateValue type="int" value=")" << value << R"(" stack="push" />
                </then>
            </stateChange>
            <stateChange>
                <if>
                    <condition>
                        <stateValue type="eventField" value=")"
             << field << R"(" />
                        <stateValue type="string" value=")"
             << name << R"(" />
                    </condition>
                </if>
                <then>
)"
             << statePathPrefix(routePrefixed) << R"(                    <stateAttribute type="constant" value=")"
             << CtfSchema::eventName(eventId) << R"(" />
                    <stateAttribute type="constant" value=")"
             << name << R"(" />
                    <stateValue type="null" stack="pop" />
                    <futureTime type="script" value="timestamp + )"
             << kEventPulseNanoseconds << R"(" scriptEngine="rhino">
                        <stateValue id="timestamp" type="eventField" value="timestamp" />
                    </futureTime>
                </then>
            </stateChange>
)";
  }
  return handlers.str();
}

/** @brief Generates one visible pulse for every comparator-only DWT match. */
static std::string dwtMatchHandler(bool routePrefixed)
{
  std::ostringstream handler;
  handler << R"(            <stateChange>
)";
  handler << statePathPrefix(routePrefixed);
  handler << R"(                <stateAttribute type="constant" value=")"
          << CtfSchema::eventName(CtfSchema::EventId::DwtMatch) << R"(" />
                <stateAttribute type="eventField" value="cmsis_dwt_comparator" />
                <stateValue type="int" value="1" stack="push" />
            </stateChange>
            <stateChange>
)";
  handler << statePathPrefix(routePrefixed);
  handler << R"(                <stateAttribute type="constant" value=")"
          << CtfSchema::eventName(CtfSchema::EventId::DwtMatch) << R"(" />
                <stateAttribute type="eventField" value="cmsis_dwt_comparator" />
                <stateValue type="null" stack="pop" />
                <futureTime type="script" value="timestamp + )"
          << kEventPulseNanoseconds << R"(" scriptEngine="rhino">
                    <stateValue id="timestamp" type="eventField" value="timestamp" />
                </futureTime>
            </stateChange>
)";
  return handler.str();
}

/** @brief Generates one DWT address handler for each encoded data-address width. */
static std::string dwtAddressHandlers(bool routePrefixed)
{
  std::ostringstream handlers;
  static_assert(CtfSchema::DwtAddressVariants.front().tag == CtfSchema::DwtAddressTag::None);
  for (std::size_t index = 1U; index < CtfSchema::DwtAddressVariants.size(); ++index) {
    const auto& variant = CtfSchema::DwtAddressVariants[index];
    handlers << R"(            <stateChange>
                <if>
                    <condition>
                        <stateValue type="eventField" value="cmsis_dwt_address_type" />
                        <stateValue type="string" value=")"
             << variant.name << R"(" />
                    </condition>
                </if>
                <then>
)";
    handlers << statePathPrefix(routePrefixed);
    handlers << R"(                    <stateAttribute type="constant" value=")"
             << CtfSchema::eventName(CtfSchema::EventId::DwtAddress) << R"(" />
                    <stateAttribute type="eventField" value="cmsis_dwt_comparator" />
                    <stateAttribute type="constant" value="address" />
                    <stateValue type="eventField" value="cmsis_dwt_address.)"
             << variant.name << R"(" forcedType="long" />
                </then>
            </stateChange>
)";
  }
  return handlers.str();
}

/** @brief Returns whether a selected graphical view requires its corresponding event handler. */
static bool includesView(TraceCompassXmlWriter::ViewMask views, TraceCompassXmlWriter::View view)
{
  return (views & TraceCompassXmlWriter::viewMask(view)) != 0U;
}

/** @brief Generates the Trace Compass state-provider definition. */
static std::string stateProviderXml(bool routePrefixed, TraceCompassXmlWriter::ViewMask views)
{
  // Numeric time-graph states are exposed as TSP style keys; string states are
  // serialized without a style and appear as gaps in compatible clients.
  std::ostringstream xml;
  xml << R"(    <stateProvider version="__SWO_ANALYSIS_VERSION__" id="arm.cmsis.swo.analysis.v1">
        <head><label value="SWO Trace Analysis" /></head>
)";
  if (includesView(views, TraceCompassXmlWriter::View::DwtValue)) {
    xml << R"(        <eventHandler eventName=")"
        << CtfSchema::eventName(CtfSchema::EventId::DwtValue) << R"(">
)";
    xml << valueHandlers(CtfSchema::EventId::DwtValue, "dwt", "cmsis_dwt_comparator", "data", routePrefixed);
    xml << R"(        </eventHandler>
)";
  }
  if (includesView(views, TraceCompassXmlWriter::View::DwtAddress)) {
    xml << R"(        <eventHandler eventName=")"
        << CtfSchema::eventName(CtfSchema::EventId::DwtAddress) << R"(">
)";
    xml << dwtAddressHandlers(routePrefixed);
    xml << R"(        </eventHandler>
)";
  }
  if (includesView(views, TraceCompassXmlWriter::View::DwtMatch)) {
    xml << R"(        <eventHandler eventName=")"
        << CtfSchema::eventName(CtfSchema::EventId::DwtMatch) << R"(">
)";
    xml << dwtMatchHandler(routePrefixed);
    xml << R"(        </eventHandler>
)";
  }
  if (includesView(views, TraceCompassXmlWriter::View::DwtEvent)) {
    xml << R"(        <eventHandler eventName=")"
        << CtfSchema::eventName(CtfSchema::EventId::DwtEvent) << R"(">
)";
    xml << eventCounterHandlers(CtfSchema::EventId::DwtEvent, "cmsis_dwt_event_counter", kDwtEventCounters,
                                CtfSchema::dwtEventCounterName, routePrefixed);
    xml << R"(        </eventHandler>
)";
  }
  if (includesView(views, TraceCompassXmlWriter::View::PmuEvent)) {
    xml << R"(        <eventHandler eventName=")"
        << CtfSchema::eventName(CtfSchema::EventId::PmuEvent) << R"(">
)";
    xml << eventCounterHandlers(CtfSchema::EventId::PmuEvent, "cmsis_pmu_event_counter", kPmuEventCounters,
                                CtfSchema::pmuEventCounterName, routePrefixed);
    xml << R"(        </eventHandler>
)";
  }
  const auto exceptionView = includesView(views, TraceCompassXmlWriter::View::Exception);
  if (exceptionView) {
    xml << R"(        <eventHandler eventName=")"
        << CtfSchema::eventName(CtfSchema::EventId::Exception) << R"(">
            <stateChange>
                <if>
                    <condition>
                        <stateValue type="eventField" value="cmsis_exception_action" />
                        <stateValue type="string" value="entered" />
                    </condition>
                </if>
                <then>
)"
      << statePathPrefix(routePrefixed) << R"(                    <stateAttribute type="constant" value=")"
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
                        <stateValue type="string" value="entered" />
                    </condition>
                </if>
                <then>
)"
      << statePathPrefix(routePrefixed) << R"(                    <stateAttribute type="constant" value="EXCEPTION_RETURN" />
                    <stateAttribute type="constant" value="Exception Return" />
                    <stateAttribute type="constant" value="action" />
                    <stateValue type="null" />
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
)"
      << statePathPrefix(routePrefixed) << R"(                    <stateAttribute type="constant" value=")"
      << CtfSchema::eventName(CtfSchema::EventId::Exception) << R"(" />
                    <stateAttribute type="eventField" value="cmsis_exception_number" />
                    <stateAttribute type="constant" value="action" />
                    <stateValue type="null" />
                </then>
            </stateChange>
            <stateChange>
                <if>
                    <and>
                        <condition>
                            <stateValue type="eventField" value="cmsis_exception_action" />
                            <stateValue type="string" value="exited" />
                        </condition>
                        <condition>
                            <stateValue type="eventField" value="cmsis_exception_origin" />
                            <stateValue type="string" value="trace" />
                        </condition>
                    </and>
                </if>
                <then>
)"
      << statePathPrefix(routePrefixed) << R"(                    <stateAttribute type="constant" value="EXCEPTION_RETURN" />
                    <stateAttribute type="constant" value="Exception Return" />
                    <stateAttribute type="constant" value="action" />
                    <stateValue type="eventField" value="cmsis_exception_number_value" forcedType="long" />
                </then>
            </stateChange>
            <stateChange>
                <if>
                    <condition>
                        <stateValue type="eventField" value="cmsis_exception_action" />
                        <stateValue type="string" value="returned" />
                    </condition>
                </if>
                <then>
)"
      << statePathPrefix(routePrefixed) << R"(                    <stateAttribute type="constant" value=")"
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
                        <stateValue type="string" value="returned" />
                    </condition>
                </if>
                <then>
)"
      << statePathPrefix(routePrefixed) << R"(                    <stateAttribute type="constant" value="EXCEPTION_RETURN" />
                    <stateAttribute type="constant" value="Exception Return" />
                    <stateAttribute type="constant" value="action" />
                    <stateValue type="null" />
                </then>
            </stateChange>
        </eventHandler>
)";
  }
  const auto processorStateView = includesView(views, TraceCompassXmlWriter::View::ProcessorState);
  if (processorStateView) {
    xml << R"(        <eventHandler eventName=")"
        << CtfSchema::eventName(CtfSchema::EventId::PcSample) << R"(">
            <stateChange>
                <if>
                    <condition>
                        <stateValue type="eventField" value="cmsis_pc_sample_state" />
                        <stateValue type="long" value=")"
      << static_cast<unsigned>(CtfSchema::value(CtfSchema::PcSampleState::Sleep)) << R"(" />
                    </condition>
                </if>
                <then>
)"
      << statePathPrefix(routePrefixed) << R"(                    <stateAttribute type="constant" value=")"
      << CtfSchema::eventName(CtfSchema::EventId::PcSample) << R"(" />
                    <stateAttribute type="constant" value="Sleep" />
                    <stateValue type="int" value=")"
      << static_cast<unsigned>(CtfSchema::value(CtfSchema::PcSampleState::Sleep)) << R"(" />
                </then>
            </stateChange>
            <stateChange>
                <if>
                    <condition>
                        <stateValue type="eventField" value="cmsis_pc_sample_state" />
                        <stateValue type="long" value=")"
      << static_cast<unsigned>(CtfSchema::value(CtfSchema::PcSampleState::Pc)) << R"(" />
                    </condition>
                </if>
                <then>
)"
      << statePathPrefix(routePrefixed) << R"(                    <stateAttribute type="constant" value=")"
      << CtfSchema::eventName(CtfSchema::EventId::PcSample) << R"(" />
                    <stateAttribute type="constant" value="Sleep" />
                    <stateValue type="null" />
                </then>
            </stateChange>
        </eventHandler>
)";
  }
  if (exceptionView || processorStateView) {
    xml << R"(        <eventHandler eventName=")"
        << CtfSchema::eventName(CtfSchema::EventId::TraceStatus) << R"(">
)";
  }
  if (processorStateView) {
    xml << R"(            <stateChange>
                <if>
                    <or>
                        <condition>
                            <stateValue type="eventField" value="cmsis_trace_status_reason" />
                            <stateValue type="string" value="overflow" />
                        </condition>
                        <condition>
                            <stateValue type="eventField" value="cmsis_trace_status_reason" />
                            <stateValue type="string" value="data_loss" />
                        </condition>
                    </or>
                </if>
                <then>
)"
      << statePathPrefix(routePrefixed) << R"(                    <stateAttribute type="constant" value=")"
      << CtfSchema::eventName(CtfSchema::EventId::PcSample) << R"(" />
                    <stateAttribute type="constant" value="Sleep" />
                    <stateValue type="null" />
                </then>
            </stateChange>
)";
  }
  if (exceptionView) {
    xml << R"(            <stateChange>
                <if>
                    <condition>
                        <stateValue type="eventField" value="cmsis_trace_status_reason" />
                        <stateValue type="string" value="overflow" />
                    </condition>
                </if>
                <then>
)"
      << statePathPrefix(routePrefixed) << R"(                    <stateAttribute type="constant" value="EXCEPTION_RETURN" />
                    <stateAttribute type="constant" value="Exception Return" />
                    <stateAttribute type="constant" value="action" />
                    <stateValue type="null" />
                </then>
            </stateChange>
            <stateChange>
                <if>
                    <condition>
                        <stateValue type="eventField" value="cmsis_trace_status_reason" />
                        <stateValue type="string" value="data_loss" />
                    </condition>
                </if>
                <then>
)"
      << statePathPrefix(routePrefixed) << R"(                    <stateAttribute type="constant" value="EXCEPTION_RETURN" />
                    <stateAttribute type="constant" value="Exception Return" />
                    <stateAttribute type="constant" value="action" />
                    <stateValue type="null" />
                </then>
            </stateChange>
)";
  }
  if (exceptionView || processorStateView) {
    xml << R"(        </eventHandler>
)";
  }
  xml << R"(    </stateProvider>
)";
  return xml.str();
}

/** @brief Escapes arbitrary route labels for double-quoted XML attributes. */
static std::string xmlAttribute(std::string_view value)
{
  std::string escaped;
  for (const auto character : value) {
    switch (character) {
    case '&':
      escaped += "&amp;";
      break;
    case '<':
      escaped += "&lt;";
      break;
    case '>':
      escaped += "&gt;";
      break;
    case '"':
      escaped += "&quot;";
      break;
    case '\'':
      escaped += "&apos;";
      break;
    default:
      escaped += character;
      break;
    }
  }
  return escaped;
}

/** @brief Builds one provider ID, optionally scoped to a concrete formatted stream. */
static std::string viewId(std::string_view base, const TraceCompassXmlWriter::ViewRoute* route)
{
  return std::string(base) + (route == nullptr ? ".v1" : ".stream" + std::to_string(route->traceBusId) + ".v1");
}

/** @brief Builds one visible label whose route suffix never exposes an available processor's numeric ID. */
static std::string viewLabel(std::string_view base, const TraceCompassXmlWriter::ViewRoute* route)
{
  if (route == nullptr || route->label.empty()) {
    return xmlAttribute(base);
  }
  return xmlAttribute(std::string(base) + " - " + route->label);
}

/** @brief Selects a concrete route by ID while leaving its display label out of the state-system query. */
static std::string viewPath(std::string_view path, const TraceCompassXmlWriter::ViewRoute* route)
{
  return route == nullptr ? std::string(path)
                          : "*/" + std::to_string(route->traceBusId) + '/' + std::string(path);
}

/** @brief Generates only graphical Trace Compass views, one per route for generalized CTF. */
static std::string viewsXml(bool routePrefixed,
                            const std::vector<TraceCompassXmlWriter::ViewRoute>& routes,
                            TraceCompassXmlWriter::ViewMask legacyViews)
{
  std::ostringstream xml;
  const auto forEachRoute = [&](TraceCompassXmlWriter::View view, const auto& emit) {
    if (!routePrefixed) {
      if ((legacyViews & TraceCompassXmlWriter::viewMask(view)) != 0U) {
        const TraceCompassXmlWriter::ViewRoute* legacyRoute = nullptr;
        emit(legacyRoute);
      }
      return;
    }
    for (const auto& route : routes) {
      if ((route.views & TraceCompassXmlWriter::viewMask(view)) != 0U) {
        emit(&route);
      }
    }
  };

  forEachRoute(TraceCompassXmlWriter::View::DwtValue, [&](const auto* route) {
    xml << "    <xyView id=\"" << viewId("arm.cmsis.swo.xy.dwt_value", route) << "\">\n"
        << "        <head><analysis id=\"arm.cmsis.swo.analysis.v1\" /><label value=\""
        << viewLabel(CtfSchema::eventName(CtfSchema::EventId::DwtValue), route) << "\" /></head>\n"
        << "        <entry path=\""
        << viewPath(std::string(CtfSchema::eventName(CtfSchema::EventId::DwtValue)) + "/*", route)
        << "\"><display type=\"constant\" value=\"data\" /><name type=\"self\" /></entry>\n"
        << "    </xyView>\n";
  });
  forEachRoute(TraceCompassXmlWriter::View::DwtAddress, [&](const auto* route) {
    xml << "    <xyView id=\"" << viewId("arm.cmsis.swo.xy.dwt_addr", route) << "\">\n"
        << "        <head><analysis id=\"arm.cmsis.swo.analysis.v1\" /><label value=\""
        << viewLabel(CtfSchema::eventName(CtfSchema::EventId::DwtAddress), route) << "\" /></head>\n"
        << "        <entry path=\""
        << viewPath(std::string(CtfSchema::eventName(CtfSchema::EventId::DwtAddress)) + "/*", route)
        << "\"><display type=\"constant\" value=\"address\" /><name type=\"self\" /></entry>\n"
        << "    </xyView>\n";
  });
  forEachRoute(TraceCompassXmlWriter::View::DwtMatch, [&](const auto* route) {
    xml << "    <timeGraphView id=\"" << viewId("arm.cmsis.swo.tg.dwt_match", route) << "\">\n"
        << "        <head><analysis id=\"arm.cmsis.swo.analysis.v1\" /><label value=\""
        << viewLabel("DWT Match", route) << "\" /></head>\n"
        << "        <definedValue name=\"Something happened\" value=\"1\" color=\"#F6BD16\" />\n"
        << "        <entry path=\""
        << viewPath(std::string(CtfSchema::eventName(CtfSchema::EventId::DwtMatch)) + "/*", route)
        << "\" displayText=\"true\"><display type=\"constant\" value=\"1\" /><name type=\"self\" /></entry>\n"
        << "    </timeGraphView>\n";
  });
  forEachRoute(TraceCompassXmlWriter::View::DwtEvent, [&](const auto* route) {
    xml << "    <timeGraphView id=\"" << viewId("arm.cmsis.swo.tg.dwt_event", route) << "\">\n"
        << "        <head><analysis id=\"arm.cmsis.swo.analysis.v1\" /><label value=\""
        << viewLabel("DWT Event Counters", route) << "\" /></head>\n";
    for (const auto counter : kDwtEventCounters) {
      const auto value = static_cast<unsigned>(CtfSchema::value(counter));
      xml << "        <definedValue name=\"" << value << "\" value=\"" << value
          << "\" color=\"#F6BD16\" />\n";
    }
    xml << "        <entry path=\""
        << viewPath(std::string(CtfSchema::eventName(CtfSchema::EventId::DwtEvent)) + "/*", route)
        << "\" displayText=\"true\"><display type=\"constant\" value=\"1\" /></entry>\n"
        << "    </timeGraphView>\n";
  });
  forEachRoute(TraceCompassXmlWriter::View::PmuEvent, [&](const auto* route) {
    xml << "    <timeGraphView id=\"" << viewId("arm.cmsis.swo.tg.pmu_event", route) << "\">\n"
        << "        <head><analysis id=\"arm.cmsis.swo.analysis.v1\" /><label value=\""
        << viewLabel("PMU Event Counters", route) << "\" /></head>\n";
    for (const auto counter : kPmuEventCounters) {
      const auto value = static_cast<unsigned>(CtfSchema::value(counter));
      xml << "        <definedValue name=\"" << value << "\" value=\"" << value
          << "\" color=\"#F6BD16\" />\n";
    }
    xml << "        <entry path=\""
        << viewPath(std::string(CtfSchema::eventName(CtfSchema::EventId::PmuEvent)) + "/*", route)
        << "\" displayText=\"true\"><display type=\"constant\" value=\"1\" /></entry>\n"
        << "    </timeGraphView>\n";
  });
  forEachRoute(TraceCompassXmlWriter::View::Exception, [&](const auto* route) {
    xml << "    <timeGraphView id=\"" << viewId("arm.cmsis.swo.tg.exception", route) << "\">\n"
        << "        <head><analysis id=\"arm.cmsis.swo.analysis.v1\" /><label value=\""
        << viewLabel(CtfSchema::eventName(CtfSchema::EventId::Exception), route) << "\" /></head>\n"
        << "        <entry path=\"" << viewPath("EXCEPTION/Thread Mode", route)
        << "\" displayText=\"false\"><display type=\"constant\" value=\"action\" /><name type=\"self\" /></entry>\n"
        << "        <entry path=\"" << viewPath("EXCEPTION_RETURN/*", route)
        << "\" displayText=\"true\"><display type=\"constant\" value=\"action\" /><name type=\"self\" /></entry>\n"
        << "        <entry path=\"" << viewPath("EXCEPTION/(?!Thread Mode).+", route)
        << "\" displayText=\"false\"><display type=\"constant\" value=\"action\" /><name type=\"self\" /></entry>\n"
        << "    </timeGraphView>\n";
  });
  forEachRoute(TraceCompassXmlWriter::View::ProcessorState, [&](const auto* route) {
    xml << "    <timeGraphView id=\"" << viewId("arm.cmsis.swo.tg.processor_state", route) << "\">\n"
        << "        <head><analysis id=\"arm.cmsis.swo.analysis.v1\" /><label value=\""
        << viewLabel("Processor State", route) << "\" /></head>\n"
        << "        <definedValue name=\"Sleep\" value=\""
        << static_cast<unsigned>(CtfSchema::value(CtfSchema::PcSampleState::Sleep))
        << "\" color=\"#5B8FF9\" />\n"
        << "        <entry path=\""
        << viewPath(std::string(CtfSchema::eventName(CtfSchema::EventId::PcSample)) + "/*", route)
        << "\" displayText=\"true\"><display type=\"self\" /></entry>\n"
        << "    </timeGraphView>\n";
  });
  return xml.str();
}

/** @brief Assembles the complete versioned Trace Compass analysis XML. */
static std::string traceCompassXml(bool routePrefixed,
                                   const std::vector<TraceCompassXmlWriter::ViewRoute>& routes,
                                   TraceCompassXmlWriter::ViewMask legacyViews)
{
  std::ostringstream xml;
  xml << R"(<?xml version="1.0" encoding="UTF-8"?>
<tmfxml xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    xsi:noNamespaceSchemaLocation="xmlDefinition.xsd">
)";
  auto activeViews = legacyViews;
  if (routePrefixed) {
    activeViews = 0U;
    for (const auto& route : routes) {
      activeViews |= route.views;
    }
  }
  xml << stateProviderXml(routePrefixed, activeViews);
  xml << viewsXml(routePrefixed, routes, legacyViews);
  xml << R"(</tmfxml>
)";
  return withTraceCompassAnalysisVersion(xml.str());
}

static void writeXmlFile(const std::filesystem::path& filePath, bool routePrefixed,
                         const std::vector<TraceCompassXmlWriter::ViewRoute>& routes,
                         TraceCompassXmlWriter::ViewMask legacyViews)
{
  if ((legacyViews & ~TraceCompassXmlWriter::AllViews) != 0U) {
    throw std::invalid_argument("Trace Compass XML contains an unsupported legacy view selection");
  }
  if (routePrefixed) {
    if (routes.empty()) {
      throw std::invalid_argument("route-prefixed Trace Compass XML requires at least one view route");
    }
    std::array<bool, 256U> seenIds{};
    for (const auto& route : routes) {
      if ((route.views & ~TraceCompassXmlWriter::AllViews) != 0U) {
        throw std::invalid_argument("Trace Compass XML contains an unsupported route view selection");
      }
      if (route.traceBusId == 0U || seenIds[route.traceBusId]) {
        throw std::invalid_argument("route-prefixed Trace Compass XML requires unique nonzero Trace Bus IDs");
      }
      seenIds[route.traceBusId] = true;
    }
  }
  if (!filePath.parent_path().empty()) {
    std::filesystem::create_directories(filePath.parent_path());
  }
  std::ofstream out(filePath, std::ios::out | std::ios::trunc);
  if (!out) {
    throw std::runtime_error("Failed to write Trace Compass XML " + filePath.string());
  }
  out << traceCompassXml(routePrefixed, routes, legacyViews);
  out.close();
  if (!out) {
    throw std::runtime_error("Failed to write Trace Compass XML " + filePath.string());
  }
}

void TraceCompassXmlWriter::writeLegacyFile(const std::filesystem::path& filePath, ViewMask views)
{
  writeXmlFile(filePath, false, {}, views);
}

void TraceCompassXmlWriter::writeRoutedFile(const std::filesystem::path& filePath,
                                             const std::vector<ViewRoute>& routes)
{
  writeXmlFile(filePath, true, routes, 0U);
}
