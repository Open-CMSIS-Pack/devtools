/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TraceCompassXmlWriter.h"

#include "CtfSchema.h"
#include "TraceStreamId.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <ostream>
#include <set>
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

/** @brief Separates every event by its capture clock and architectural route. */
static std::string statePathPrefix()
{
  return "                    <stateAttribute type=\"eventField\" value=\"hostId\" />\n"
         "                    <stateAttribute type=\"eventField\" value=\"context.cmsis_trace_bus_id\" />\n";
}

/** @brief Generates Trace Compass value handlers for one CTF event route. */
static std::string valueHandlers(CtfSchema::EventId eventId, const char* prefix, const char* routeField,
                                 const char* valueAttribute)
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
    handlers << statePathPrefix();
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
                                        const std::array<Counter, Size>& counters, CounterName counterName)
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
             << statePathPrefix() << R"(                    <stateAttribute type="constant" value=")"
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
             << statePathPrefix() << R"(                    <stateAttribute type="constant" value=")"
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
static std::string dwtMatchHandler()
{
  std::ostringstream handler;
  handler << R"(            <stateChange>
)";
  handler << statePathPrefix();
  handler << R"(                <stateAttribute type="constant" value=")"
          << CtfSchema::eventName(CtfSchema::EventId::DwtMatch) << R"(" />
                <stateAttribute type="eventField" value="cmsis_dwt_comparator" />
                <stateValue type="int" value="1" stack="push" />
            </stateChange>
            <stateChange>
)";
  handler << statePathPrefix();
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
static std::string dwtAddressHandlers()
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
    handlers << statePathPrefix();
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

/** @brief Writes one complete state-provider event handler. */
static void writeStateHandler(std::ostream& xml, CtfSchema::EventId eventId, const std::string& stateChanges)
{
  xml << R"(        <eventHandler eventName=")" << CtfSchema::eventName(eventId) << R"(">
)";
  xml << stateChanges;
  xml << R"(        </eventHandler>
)";
}

/** @brief Writes exception-entry state changes. */
static void writeExceptionEnteredStateChanges(std::ostream& xml)
{
  xml << R"(            <stateChange>
                <if>
                    <condition>
                        <stateValue type="eventField" value="cmsis_exception_action" />
                        <stateValue type="string" value="entered" />
                    </condition>
                </if>
                <then>
)";
  xml << statePathPrefix();
  xml << R"(                    <stateAttribute type="constant" value=")"
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
      << statePathPrefix() << R"(                    <stateAttribute type="constant" value="EXCEPTION_RETURN" />
                    <stateAttribute type="constant" value="Exception Return" />
                    <stateAttribute type="constant" value="action" />
                    <stateValue type="null" />
                </then>
            </stateChange>
)";
}

/** @brief Writes exception-exit state changes. */
static void writeExceptionExitedStateChanges(std::ostream& xml)
{
  xml << R"(            <stateChange>
                <if>
                    <condition>
                        <stateValue type="eventField" value="cmsis_exception_action" />
                        <stateValue type="string" value="exited" />
                    </condition>
                </if>
                <then>
)";
  xml << statePathPrefix();
  xml << R"(                    <stateAttribute type="constant" value=")"
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
      << statePathPrefix() << R"(                    <stateAttribute type="constant" value="EXCEPTION_RETURN" />
                    <stateAttribute type="constant" value="Exception Return" />
                    <stateAttribute type="constant" value="action" />
                    <stateValue type="eventField" value="cmsis_exception_number_value" forcedType="long" />
                </then>
            </stateChange>
)";
}

/** @brief Writes exception-return state changes. */
static void writeExceptionReturnedStateChanges(std::ostream& xml)
{
  xml << R"(            <stateChange>
                <if>
                    <condition>
                        <stateValue type="eventField" value="cmsis_exception_action" />
                        <stateValue type="string" value="returned" />
                    </condition>
                </if>
                <then>
)";
  xml << statePathPrefix();
  xml << R"(                    <stateAttribute type="constant" value=")"
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
      << statePathPrefix() << R"(                    <stateAttribute type="constant" value="EXCEPTION_RETURN" />
                    <stateAttribute type="constant" value="Exception Return" />
                    <stateAttribute type="constant" value="action" />
                    <stateValue type="null" />
                </then>
            </stateChange>
)";
}

/** @brief Writes state changes that track exception nesting and returns. */
static void writeExceptionStateHandler(std::ostream& xml)
{
  std::ostringstream stateChanges;
  writeExceptionEnteredStateChanges(stateChanges);
  writeExceptionExitedStateChanges(stateChanges);
  writeExceptionReturnedStateChanges(stateChanges);
  writeStateHandler(xml, CtfSchema::EventId::Exception, stateChanges.str());
}

/** @brief Writes state changes that expose processor sleep intervals. */
static void writeProcessorStateHandler(std::ostream& xml)
{
  std::ostringstream stateChanges;
  stateChanges << R"(            <stateChange>
                <if>
                    <condition>
                        <stateValue type="eventField" value="cmsis_pc_sample_state" />
                        <stateValue type="long" value=")"
      << static_cast<unsigned>(CtfSchema::value(CtfSchema::PcSampleState::Sleep)) << R"(" />
                    </condition>
                </if>
                <then>
)";
  stateChanges << statePathPrefix();
  stateChanges << R"(                    <stateAttribute type="constant" value=")"
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
      << statePathPrefix() << R"(                    <stateAttribute type="constant" value=")"
      << CtfSchema::eventName(CtfSchema::EventId::PcSample) << R"(" />
                    <stateAttribute type="constant" value="Sleep" />
                    <stateValue type="null" />
                </then>
            </stateChange>
)";
  writeStateHandler(xml, CtfSchema::EventId::PcSample, stateChanges.str());

  std::ostringstream prohibitedStateChange;
  prohibitedStateChange << R"(            <stateChange>
)"
      << statePathPrefix() << R"(                <stateAttribute type="constant" value=")"
      << CtfSchema::eventName(CtfSchema::EventId::PcSample) << R"(" />
                <stateAttribute type="constant" value="Sleep" />
                <stateValue type="null" />
            </stateChange>
)";
  writeStateHandler(xml, CtfSchema::EventId::PcSampleProhibited, prohibitedStateChange.str());
}

/** @brief Writes the processor-state reset performed after discontinuities. */
static void writeProcessorDiscontinuityStateChange(std::ostream& xml)
{
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
      << statePathPrefix() << R"(                    <stateAttribute type="constant" value=")"
      << CtfSchema::eventName(CtfSchema::EventId::PcSample) << R"(" />
                    <stateAttribute type="constant" value="Sleep" />
                    <stateValue type="null" />
                </then>
            </stateChange>
)";
}

/** @brief Writes the exception-return reset performed after discontinuities. */
static void writeExceptionDiscontinuityStateChanges(std::ostream& xml)
{
  xml << R"(            <stateChange>
                <if>
                    <condition>
                        <stateValue type="eventField" value="cmsis_trace_status_reason" />
                        <stateValue type="string" value="overflow" />
                    </condition>
                </if>
                <then>
)";
  xml << statePathPrefix();
  xml << R"(                    <stateAttribute type="constant" value="EXCEPTION_RETURN" />
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
      << statePathPrefix() << R"(                    <stateAttribute type="constant" value="EXCEPTION_RETURN" />
                    <stateAttribute type="constant" value="Exception Return" />
                    <stateAttribute type="constant" value="action" />
                    <stateValue type="null" />
                </then>
            </stateChange>
)";
}

/** @brief Writes state resets for trace discontinuities. */
static void writeDiscontinuityStateHandler(std::ostream& xml, bool exceptionView,
                                           bool processorStateView)
{
  if (!exceptionView && !processorStateView) {
    return;
  }

  std::ostringstream stateChanges;
  if (processorStateView) {
    writeProcessorDiscontinuityStateChange(stateChanges);
  }
  if (exceptionView) {
    writeExceptionDiscontinuityStateChanges(stateChanges);
  }
  writeStateHandler(xml, CtfSchema::EventId::TraceStatus, stateChanges.str());
}

/** @brief Names one imported analysis and all of its graphical providers. */
struct AnalysisIdentity {
  std::string nameSpace;
  std::string providerId;
};

/** @brief Accepts exactly the canonical UUID spelling emitted by CtfUuid::toString. */
static bool isCanonicalClockUuid(std::string_view uuid)
{
  if (uuid.size() != 36U) {
    return false;
  }
  for (std::size_t index = 0U; index < uuid.size(); ++index) {
    const auto character = uuid[index];
    if (index == 8U || index == 13U || index == 18U || index == 23U) {
      if (character != '-') {
        return false;
      }
    } else if (!((character >= '0' && character <= '9') || (character >= 'a' && character <= 'f'))) {
      return false;
    }
  }
  return true;
}

/** @brief Rejects route values that cannot safely identify one state-system path. */
static void validateViewRoute(const TraceCompassXmlWriter::ViewRoute& route)
{
  if ((route.views & ~TraceCompassXmlWriter::AllViews) != 0U) {
    throw std::invalid_argument("Trace Compass XML contains an unsupported route view selection");
  }
  if (route.traceBusId != 0U && !CoreSight::isAtbTraceId(route.traceBusId)) {
    throw std::invalid_argument("Trace Compass XML requires Trace Bus IDs between 0 and 111");
  }
  if (!isCanonicalClockUuid(route.clockUuid)) {
    throw std::invalid_argument("Trace Compass XML requires canonical lower-case clock UUIDs");
  }
}

/** @brief Validates routes and derives an order-independent namespace from their identities. */
static AnalysisIdentity analysisIdentity(const std::vector<TraceCompassXmlWriter::ViewRoute>& routes)
{
  if (routes.empty()) {
    throw std::invalid_argument("Trace Compass XML requires at least one view route");
  }
  std::set<std::pair<std::string, std::uint8_t>> identities;
  auto views = TraceCompassXmlWriter::ViewMask{0U};
  for (const auto& route : routes) {
    validateViewRoute(route);
    if (!identities.emplace(route.clockUuid, route.traceBusId).second) {
      throw std::invalid_argument("Trace Compass XML requires unique clock UUID and Trace Bus ID pairs");
    }
    views |= route.views;
  }
  if (views == 0U) {
    throw std::invalid_argument("Trace Compass XML requires at least one graphical view");
  }

  constexpr std::uint64_t kFnvOffsetBasis = 14695981039346656037ULL;
  constexpr std::uint64_t kFnvPrime = 1099511628211ULL;
  auto hash = kFnvOffsetBasis;
  for (const auto& [clockUuid, traceBusId] : identities) {
    const auto key = clockUuid + '/' + std::to_string(traceBusId) + ';';
    for (const auto character : key) {
      hash ^= static_cast<std::uint8_t>(character);
      hash *= kFnvPrime;
    }
  }
  std::ostringstream nameSpace;
  nameSpace << std::hex << hash;
  const auto suffix = nameSpace.str();
  return {suffix, "arm.cmsis.ctrace.analysis." + suffix + ".v1"};
}

/** @brief Generates the Trace Compass state-provider definition. */
static std::string stateProviderXml(const AnalysisIdentity& identity, TraceCompassXmlWriter::ViewMask views)
{
  // Numeric time-graph states are exposed as TSP style keys; string states are
  // serialized without a style and appear as gaps in compatible clients.
  std::ostringstream xml;
  xml << "    <stateProvider version=\"" << kTraceCompassAnalysisVersionPlaceholder << "\" id=\""
      << identity.providerId << "\">\n"
      << "        <head><label value=\"CMSIS Trace Analysis\" /></head>\n";
  if (includesView(views, TraceCompassXmlWriter::View::DwtValue)) {
    writeStateHandler(xml, CtfSchema::EventId::DwtValue,
                      valueHandlers(CtfSchema::EventId::DwtValue, "dwt", "cmsis_dwt_comparator", "data"));
  }
  if (includesView(views, TraceCompassXmlWriter::View::DwtAddress)) {
    writeStateHandler(xml, CtfSchema::EventId::DwtAddress, dwtAddressHandlers());
  }
  if (includesView(views, TraceCompassXmlWriter::View::DwtMatch)) {
    writeStateHandler(xml, CtfSchema::EventId::DwtMatch, dwtMatchHandler());
  }
  if (includesView(views, TraceCompassXmlWriter::View::DwtEvent)) {
    writeStateHandler(xml, CtfSchema::EventId::DwtEvent,
                      eventCounterHandlers(CtfSchema::EventId::DwtEvent, "cmsis_dwt_event_counter", kDwtEventCounters,
                                           CtfSchema::dwtEventCounterName));
  }
  if (includesView(views, TraceCompassXmlWriter::View::PmuEvent)) {
    writeStateHandler(xml, CtfSchema::EventId::PmuEvent,
                      eventCounterHandlers(CtfSchema::EventId::PmuEvent, "cmsis_pmu_event_counter", kPmuEventCounters,
                                           CtfSchema::pmuEventCounterName));
  }
  const auto exceptionView = includesView(views, TraceCompassXmlWriter::View::Exception);
  if (exceptionView) {
    writeExceptionStateHandler(xml);
  }
  const auto processorStateView = includesView(views, TraceCompassXmlWriter::View::ProcessorState);
  if (processorStateView) {
    writeProcessorStateHandler(xml);
  }
  writeDiscontinuityStateHandler(xml, exceptionView, processorStateView);
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

/** @brief Scopes one graphical provider to its analysis, capture clock, and stream. */
static std::string viewId(std::string_view base, const TraceCompassXmlWriter::ViewRoute& route,
                          const AnalysisIdentity& identity)
{
  return std::string(base) + '.' + identity.nameSpace + ".clock" + route.clockUuid + ".stream" +
         std::to_string(route.traceBusId) + ".v1";
}

/** @brief Adds the caller's source label without exposing the architectural Trace Bus ID. */
static std::string viewLabel(std::string_view base, const TraceCompassXmlWriter::ViewRoute& route)
{
  return xmlAttribute(route.label.empty() ? std::string(base) : std::string(base) + " - " + route.label);
}

/** @brief Selects exactly one capture and route, independent of its display label. */
static std::string viewPath(std::string_view path, const TraceCompassXmlWriter::ViewRoute& route)
{
  // Trace Compass versions may retain the TSDL string quotes in the clock's hostId.
  const auto clockPattern = "(?:" + route.clockUuid + "|\"" + route.clockUuid + "\")";
  return xmlAttribute(clockPattern + '/' + std::to_string(route.traceBusId) + '/' + std::string(path));
}

using ViewRenderer = void (*)(std::ostream&, const TraceCompassXmlWriter::ViewRoute&, const AnalysisIdentity&);

/** @brief Writes one graphical view for each route that emitted its topic. */
static void writeSelectedViews(std::ostream& xml, const std::vector<TraceCompassXmlWriter::ViewRoute>& routes,
                               const AnalysisIdentity& identity, TraceCompassXmlWriter::View view,
                               ViewRenderer renderer)
{
  for (const auto& route : routes) {
    if (includesView(route.views, view)) {
      renderer(xml, route, identity);
    }
  }
}

/** @brief Writes one numeric DWT XY view. */
static void writeDwtXyView(std::ostream& xml, const TraceCompassXmlWriter::ViewRoute& route,
                           const AnalysisIdentity& identity,
                           std::string_view id, CtfSchema::EventId eventId, std::string_view display)
{
  const auto eventName = CtfSchema::eventName(eventId);
  xml << "    <xyView id=\"" << viewId(id, route, identity) << "\">\n"
      << "        <head><analysis id=\"" << identity.providerId << "\" /><label value=\""
      << viewLabel(eventName, route) << "\" /></head>\n"
      << "        <entry path=\"" << viewPath(std::string(eventName) + "/*", route)
      << "\"><display type=\"constant\" value=\"" << display << "\" /><name type=\"self\" /></entry>\n"
      << "    </xyView>\n";
}

/** @brief Writes the DWT-value XY view. */
static void writeDwtValueView(std::ostream& xml, const TraceCompassXmlWriter::ViewRoute& route,
                           const AnalysisIdentity& identity)
{
  writeDwtXyView(xml, route, identity, "arm.cmsis.swo.xy.dwt_value", CtfSchema::EventId::DwtValue, "data");
}

/** @brief Writes the DWT-address XY view. */
static void writeDwtAddressView(std::ostream& xml, const TraceCompassXmlWriter::ViewRoute& route,
                           const AnalysisIdentity& identity)
{
  writeDwtXyView(xml, route, identity, "arm.cmsis.swo.xy.dwt_addr", CtfSchema::EventId::DwtAddress, "address");
}

/** @brief Writes the comparator-match pulse view. */
static void writeDwtMatchView(std::ostream& xml, const TraceCompassXmlWriter::ViewRoute& route,
                           const AnalysisIdentity& identity)
{
  xml << "    <timeGraphView id=\"" << viewId("arm.cmsis.swo.tg.dwt_match", route, identity) << "\">\n"
      << "        <head><analysis id=\"" << identity.providerId << "\" /><label value=\""
      << viewLabel("DWT Match", route) << "\" /></head>\n"
      << "        <definedValue name=\"Something happened\" value=\"1\" color=\"#F6BD16\" />\n"
      << "        <entry path=\""
      << viewPath(std::string(CtfSchema::eventName(CtfSchema::EventId::DwtMatch)) + "/*", route)
      << "\" displayText=\"true\"><display type=\"constant\" value=\"1\" /><name type=\"self\" /></entry>\n"
      << "    </timeGraphView>\n";
}

/** @brief Writes one event-counter pulse view. */
template <typename Counter, std::size_t Size>
static void writeEventCounterView(std::ostream& xml, const TraceCompassXmlWriter::ViewRoute& route,
                           const AnalysisIdentity& identity,
                                  std::string_view id, std::string_view label, CtfSchema::EventId eventId,
                                  const std::array<Counter, Size>& counters)
{
  xml << "    <timeGraphView id=\"" << viewId(id, route, identity) << "\">\n"
      << "        <head><analysis id=\"" << identity.providerId << "\" /><label value=\"" << viewLabel(label, route)
      << "\" /></head>\n";
  for (const auto counter : counters) {
    const auto value = static_cast<unsigned>(CtfSchema::value(counter));
    xml << "        <definedValue name=\"" << value << "\" value=\"" << value
        << "\" color=\"#F6BD16\" />\n";
  }
  xml << "        <entry path=\"" << viewPath(std::string(CtfSchema::eventName(eventId)) + "/*", route)
      << "\" displayText=\"true\"><display type=\"constant\" value=\"1\" /></entry>\n"
      << "    </timeGraphView>\n";
}

/** @brief Writes the DWT event-counter view. */
static void writeDwtEventView(std::ostream& xml, const TraceCompassXmlWriter::ViewRoute& route,
                           const AnalysisIdentity& identity)
{
  writeEventCounterView(xml, route, identity, "arm.cmsis.swo.tg.dwt_event", "DWT Event Counters",
                        CtfSchema::EventId::DwtEvent, kDwtEventCounters);
}

/** @brief Writes the PMU event-counter view. */
static void writePmuEventView(std::ostream& xml, const TraceCompassXmlWriter::ViewRoute& route,
                           const AnalysisIdentity& identity)
{
  writeEventCounterView(xml, route, identity, "arm.cmsis.swo.tg.pmu_event", "PMU Event Counters",
                        CtfSchema::EventId::PmuEvent, kPmuEventCounters);
}

/** @brief Writes the exception timeline view. */
static void writeExceptionView(std::ostream& xml, const TraceCompassXmlWriter::ViewRoute& route,
                           const AnalysisIdentity& identity)
{
  xml << "    <timeGraphView id=\"" << viewId("arm.cmsis.swo.tg.exception", route, identity) << "\">\n"
      << "        <head><analysis id=\"" << identity.providerId << "\" /><label value=\""
      << viewLabel(CtfSchema::eventName(CtfSchema::EventId::Exception), route) << "\" /></head>\n"
      << "        <entry path=\"" << viewPath("EXCEPTION/Thread Mode", route)
      << "\" displayText=\"false\"><display type=\"constant\" value=\"action\" /><name type=\"self\" /></entry>\n"
      << "        <entry path=\"" << viewPath("EXCEPTION_RETURN/*", route)
      << "\" displayText=\"true\"><display type=\"constant\" value=\"action\" /><name type=\"self\" /></entry>\n"
      << "        <entry path=\"" << viewPath("EXCEPTION/(?!Thread Mode).+", route)
      << "\" displayText=\"false\"><display type=\"constant\" value=\"action\" /><name type=\"self\" /></entry>\n"
      << "    </timeGraphView>\n";
}

/** @brief Writes the processor-state timeline view. */
static void writeProcessorStateView(std::ostream& xml, const TraceCompassXmlWriter::ViewRoute& route,
                           const AnalysisIdentity& identity)
{
  xml << "    <timeGraphView id=\"" << viewId("arm.cmsis.swo.tg.processor_state", route, identity) << "\">\n"
      << "        <head><analysis id=\"" << identity.providerId << "\" /><label value=\""
      << viewLabel("Processor State", route) << "\" /></head>\n"
      << "        <definedValue name=\"Sleep\" value=\""
      << static_cast<unsigned>(CtfSchema::value(CtfSchema::PcSampleState::Sleep))
      << "\" color=\"#5B8FF9\" />\n"
      << "        <entry path=\""
      << viewPath(std::string(CtfSchema::eventName(CtfSchema::EventId::PcSample)) + "/*", route)
      << "\" displayText=\"true\"><display type=\"self\" /></entry>\n"
      << "    </timeGraphView>\n";
}

/** @brief Generates only graphical views observed on each concrete capture route. */
static std::string viewsXml(const std::vector<TraceCompassXmlWriter::ViewRoute>& routes,
                            const AnalysisIdentity& identity)
{
  std::ostringstream xml;
  writeSelectedViews(xml, routes, identity, TraceCompassXmlWriter::View::DwtValue, writeDwtValueView);
  writeSelectedViews(xml, routes, identity, TraceCompassXmlWriter::View::DwtAddress, writeDwtAddressView);
  writeSelectedViews(xml, routes, identity, TraceCompassXmlWriter::View::DwtMatch, writeDwtMatchView);
  writeSelectedViews(xml, routes, identity, TraceCompassXmlWriter::View::DwtEvent, writeDwtEventView);
  writeSelectedViews(xml, routes, identity, TraceCompassXmlWriter::View::PmuEvent, writePmuEventView);
  writeSelectedViews(xml, routes, identity, TraceCompassXmlWriter::View::Exception, writeExceptionView);
  writeSelectedViews(xml, routes, identity, TraceCompassXmlWriter::View::ProcessorState, writeProcessorStateView);
  return xml.str();
}

/** @brief Assembles one versioned analysis for all validated capture routes. */
static std::string traceCompassXml(const std::vector<TraceCompassXmlWriter::ViewRoute>& routes,
                                   const AnalysisIdentity& identity)
{
  std::ostringstream xml;
  xml << R"(<?xml version="1.0" encoding="UTF-8"?>
<tmfxml xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    xsi:noNamespaceSchemaLocation="xmlDefinition.xsd">
)";
  auto activeViews = TraceCompassXmlWriter::ViewMask{0U};
  for (const auto& route : routes) {
    activeViews |= route.views;
  }
  xml << stateProviderXml(identity, activeViews);
  xml << viewsXml(routes, identity);
  xml << R"(</tmfxml>
)";
  return withTraceCompassAnalysisVersion(xml.str());
}

void TraceCompassXmlWriter::writeFile(const std::filesystem::path& filePath, const std::vector<ViewRoute>& routes)
{
  const auto identity = analysisIdentity(routes);
  if (!filePath.parent_path().empty()) {
    std::filesystem::create_directories(filePath.parent_path());
  }
  std::ofstream out(filePath, std::ios::out | std::ios::binary | std::ios::trunc);
  if (!out) {
    throw std::runtime_error("Failed to write Trace Compass XML " + filePath.string());
  }
  out << traceCompassXml(routes, identity);
  out.close();
  if (!out) {
    throw std::runtime_error("Failed to write Trace Compass XML " + filePath.string());
  }
}
