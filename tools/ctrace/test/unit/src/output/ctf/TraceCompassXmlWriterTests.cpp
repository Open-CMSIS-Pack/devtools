/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TestPath.h"
#include "TestPlatform.h"
#include "TestSupport.h"

#include <gtest/gtest.h>

#include "ctf/CtfSchema.h"
#include "ctf/TraceCompassXmlWriter.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <regex>
#include <set>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace {

constexpr const char* kFirstClockUuid = "00000000-0000-4000-8000-000000000001";
constexpr const char* kSecondClockUuid = "00000000-0000-4000-8000-000000000002";

TraceCompassXmlWriter::ViewRoute viewRoute(
    std::uint8_t id = 0U, const std::string& label = {},
    TraceCompassXmlWriter::ViewMask views = TraceCompassXmlWriter::AllViews,
    const std::string& clockUuid = kFirstClockUuid)
{
  return {id, label, views, clockUuid};
}

std::string xmlRoutePath(const std::string& topic, std::uint8_t id = 0U,
                          const std::string& clockUuid = kFirstClockUuid)
{
  return "(?:" + clockUuid + "|&quot;" + clockUuid + "&quot;)/" + std::to_string(id) + '/' + topic;
}

std::vector<std::string> xmlAttributes(const std::string& xml, const std::string& tag,
                                      const std::string& attribute)
{
  const std::regex expression("<" + tag + "\\b[^>]*\\b" + attribute + "=\"([^\"]*)\"");
  std::vector<std::string> values;
  for (auto match = std::sregex_iterator(xml.begin(), xml.end(), expression); match != std::sregex_iterator(); ++match) {
    values.push_back((*match)[1].str());
  }
  return values;
}

std::string xmlAnalysisId(const std::string& xml)
{
  const auto ids = xmlAttributes(xml, "stateProvider", "id");
  return ids.empty() ? std::string{} : ids.front();
}

std::string xmlViewId(const std::string& xml, const std::string& base, std::uint8_t id,
                       const std::string& clockUuid = kFirstClockUuid)
{
  const auto analysis = xmlAnalysisId(xml);
  const auto suffixEnd = analysis.rfind(".v1");
  if (suffixEnd == std::string::npos) {
    return {};
  }
  const auto suffixBegin = analysis.rfind('.', suffixEnd - 1U);
  const auto nameSpace = analysis.substr(suffixBegin + 1U, suffixEnd - suffixBegin - 1U);
  return base + '.' + nameSpace + ".clock" + clockUuid + ".stream" + std::to_string(id) + ".v1";
}

} // namespace

TEST(CtraceUnitTests, testTraceCompassXmlWriterRejectsDirectoryTarget)
{
  const TemporaryTestPath path("ctrace-trace-compass-directory-target");
  path.createDirectory();
  EXPECT_THROW(TraceCompassXmlWriter::writeFile(path.path(), {viewRoute()}), std::runtime_error);
}

TEST(CtraceUnitTests, testTraceCompassXmlWriterSupportsParentlessTarget)
{
  const auto path = std::filesystem::path("ctrace-parentless-trace-compass.xml");
  std::error_code ignored;
  std::filesystem::remove(path, ignored);
  EXPECT_NO_THROW(TraceCompassXmlWriter::writeFile(path, {viewRoute()}));
  EXPECT_TRUE(std::filesystem::is_regular_file(path));
  std::filesystem::remove(path, ignored);
}

TEST(CtraceUnitTests, testTraceCompassXmlUsesCurrentCtfEvents)
{
  const TemporaryTestPath path("ctrace-trace-compass-schema.xml");
  TraceCompassXmlWriter::writeFile(path.path(), {viewRoute()});
  const auto xml = readTestTextFile(path.path());
  constexpr std::array<CtfSchema::EventId, 9U> stateDrivenEvents{
      CtfSchema::EventId::DwtValue,    CtfSchema::EventId::DwtAddress, CtfSchema::EventId::Exception,
      CtfSchema::EventId::TraceStatus, CtfSchema::EventId::PcSample,   CtfSchema::EventId::DwtEvent,
      CtfSchema::EventId::PmuEvent,    CtfSchema::EventId::DwtMatch,
      CtfSchema::EventId::PcSampleProhibited,
  };

  for (const auto eventId : stateDrivenEvents) {
    EXPECT_NE(xml.find("eventName=\"" + std::string(CtfSchema::eventName(eventId)) + "\""), std::string::npos);
  }
  EXPECT_EQ(xml.find("<eventHandler eventName=\"ITM\">"), std::string::npos);
  EXPECT_NE(xml.find("value=\"cmsis_pc_sample_state\""), std::string::npos);
  EXPECT_NE(xml.find("<stateAttribute type=\"constant\" value=\"Sleep\" />"), std::string::npos);
  EXPECT_NE(xml.find("<stateValue type=\"int\" value=\"0\" />"), std::string::npos);
  EXPECT_EQ(xml.find("<stateValue type=\"string\" value=\"Sleep\" />"), std::string::npos);
  EXPECT_NE(xml.find("<label value=\"Processor State\" />"), std::string::npos);
  EXPECT_EQ(xml.find("<label value=\"PC Sampling\" />"), std::string::npos);
  EXPECT_EQ(xml.find("arm.cmsis.swo.tg.itm"), std::string::npos);
  EXPECT_EQ(xml.find("arm.cmsis.swo.tg.trace_status"), std::string::npos);
  EXPECT_NE(xml.find("<definedValue name=\"Sleep\" value=\"0\""), std::string::npos);
  EXPECT_EQ(xml.find("<definedValue name=\"Running\""), std::string::npos);
  EXPECT_NE(xml.find("<entry path=\"" + xmlRoutePath("PC_SAMPLE/*") + "\" displayText=\"true\"><display type=\"self\" /></entry>"),
            std::string::npos);
  EXPECT_NE(xml.find("<stateValue type=\"string\" value=\"overflow\" />"), std::string::npos);
  EXPECT_NE(xml.find("<stateValue type=\"string\" value=\"data_loss\" />"), std::string::npos);
  EXPECT_NE(xml.find("value=\"returned\""), std::string::npos);
  EXPECT_NE(xml.find("value=\"cmsis_exception_origin\""), std::string::npos);
  EXPECT_NE(xml.find("value=\"trace\""), std::string::npos);
  EXPECT_NE(xml.find("value=\"EXCEPTION_RETURN\""), std::string::npos);
  EXPECT_NE(xml.find("value=\"Exception Return\""), std::string::npos);
  EXPECT_NE(xml.find("value=\"cmsis_dwt_event_counter\""), std::string::npos);
  EXPECT_NE(xml.find("<stateValue type=\"string\" value=\"CPICNT\" />"), std::string::npos);
  EXPECT_NE(xml.find("<stateAttribute type=\"constant\" value=\"CYCCNT\" />"), std::string::npos);
  EXPECT_NE(xml.find("<stateValue type=\"int\" value=\"0\" stack=\"push\" />"), std::string::npos);
  EXPECT_NE(xml.find("<stateValue type=\"int\" value=\"5\" stack=\"push\" />"), std::string::npos);
  EXPECT_NE(xml.find("type=\"null\" stack=\"pop\""), std::string::npos);
  EXPECT_NE(xml.find("value=\"timestamp + 1000\" scriptEngine=\"rhino\""), std::string::npos);
  EXPECT_NE(xml.find("<label value=\"DWT Event Counters\" />"), std::string::npos);
  EXPECT_NE(xml.find("<definedValue name=\"0\" value=\"0\""), std::string::npos);
  EXPECT_NE(xml.find("<definedValue name=\"5\" value=\"5\""), std::string::npos);
  EXPECT_NE(xml.find("<entry path=\"" + xmlRoutePath("DWT_EVENT/*") + "\" displayText=\"true\"><display type=\"constant\" value=\"1\" /></entry>"),
            std::string::npos);
  EXPECT_NE(xml.find("value=\"cmsis_pmu_event_counter\""), std::string::npos);
  EXPECT_NE(xml.find("<stateValue type=\"string\" value=\"Event0\" />"), std::string::npos);
  EXPECT_NE(xml.find("<stateAttribute type=\"constant\" value=\"Event7\" />"), std::string::npos);
  EXPECT_NE(xml.find("<stateValue type=\"int\" value=\"7\" stack=\"push\" />"), std::string::npos);
  EXPECT_NE(xml.find("<label value=\"PMU Event Counters\" />"), std::string::npos);
  EXPECT_NE(xml.find("<definedValue name=\"7\" value=\"7\""), std::string::npos);
  EXPECT_NE(xml.find("<entry path=\"" + xmlRoutePath("PMU_EVENT/*") + "\" displayText=\"true\"><display type=\"constant\" value=\"1\" /></entry>"),
            std::string::npos);
  EXPECT_NE(xml.find("<stateAttribute type=\"constant\" value=\"DWT_MATCH\" />"), std::string::npos);
  EXPECT_NE(xml.find("<stateAttribute type=\"eventField\" value=\"cmsis_dwt_comparator\" />"),
            std::string::npos);
  EXPECT_NE(xml.find("<label value=\"DWT Match\" />"), std::string::npos);
  EXPECT_NE(xml.find("<definedValue name=\"Something happened\" value=\"1\""), std::string::npos);
  EXPECT_NE(xml.find("<entry path=\"" + xmlRoutePath("DWT_MATCH/*") + "\" displayText=\"true\"><display type=\"constant\" value=\"1\" />"
                     "<name type=\"self\" /></entry>"),
            std::string::npos);
  const auto matchHandler = xml.find("<eventHandler eventName=\"DWT_MATCH\">");
  const auto matchHandlerEnd = xml.find("</eventHandler>", matchHandler);
  ASSERT_NE(matchHandler, std::string::npos);
  ASSERT_NE(matchHandlerEnd, std::string::npos);
  const auto matchPulseEnd = xml.find("value=\"timestamp + 1000\"", matchHandler);
  ASSERT_NE(matchPulseEnd, std::string::npos);
  EXPECT_LT(matchPulseEnd, matchHandlerEnd);
  EXPECT_NE(xml.find("value=\"cmsis_dwt_address_type\""), std::string::npos);
  EXPECT_NE(xml.find("value=\"cmsis_dwt_address.u8\" forcedType=\"long\""), std::string::npos);
  EXPECT_NE(xml.find("value=\"cmsis_dwt_address.u16\" forcedType=\"long\""), std::string::npos);
  EXPECT_NE(xml.find("value=\"cmsis_dwt_address.u32\" forcedType=\"long\""), std::string::npos);
  const auto threadModeEntry = xml.find("path=\"" + xmlRoutePath("EXCEPTION/Thread Mode") + "\"");
  const auto returnEntry = xml.find("path=\"" + xmlRoutePath("EXCEPTION_RETURN/*") + "\" displayText=\"true\"");
  const auto interruptEntries = xml.find("path=\"" + xmlRoutePath("EXCEPTION/(?!Thread Mode).+") + "\"");
  ASSERT_NE(threadModeEntry, std::string::npos);
  ASSERT_NE(returnEntry, std::string::npos);
  ASSERT_NE(interruptEntries, std::string::npos);
  EXPECT_LT(threadModeEntry, returnEntry);
  EXPECT_LT(returnEntry, interruptEntries);
}

TEST(CtraceUnitTests, testTraceCompassXmlScopesGraphicalViewsPerRoute)
{
  const TemporaryTestPath path("ctrace-trace-compass-route-views.xml");
  TraceCompassXmlWriter::writeFile(path.path(), {viewRoute(1U, "CM4"), viewRoute(2U, "CM&<7>\"'"), viewRoute(111U)});
  const auto xml = readTestTextFile(path.path());

  for (const auto id : {1U, 2U, 111U}) {
    const auto expected = xmlViewId(xml, "arm.cmsis.swo.xy.dwt_value", static_cast<std::uint8_t>(id));
    EXPECT_NE(xml.find("id=\"" + expected + "\""), std::string::npos);
  }
  EXPECT_NE(xml.find("<label value=\"DWT_VALUE - CM4\" />"), std::string::npos);
  EXPECT_NE(xml.find("<label value=\"DWT_VALUE - CM&amp;&lt;7&gt;&quot;&apos;\" />"), std::string::npos);
  EXPECT_EQ(xml.find("<label value=\"DWT_VALUE - 1\" />"), std::string::npos);
  EXPECT_EQ(xml.find("<label value=\"DWT_VALUE - 111\" />"), std::string::npos);
  EXPECT_NE(xml.find("<label value=\"DWT_VALUE\" />"), std::string::npos);
  EXPECT_NE(xml.find("<entry path=\"" + xmlRoutePath("DWT_VALUE/*", 1U) + "\""), std::string::npos);
  EXPECT_NE(xml.find("<entry path=\"" + xmlRoutePath("DWT_VALUE/*", 2U) + "\""), std::string::npos);
  EXPECT_NE(xml.find("<entry path=\"" + xmlRoutePath("PC_SAMPLE/*", 1U) + "\""), std::string::npos);
  EXPECT_NE(xml.find("<label value=\"Processor State - CM4\" />"), std::string::npos);
  EXPECT_EQ(xml.find("<label value=\"PC Sampling - CM4\" />"), std::string::npos);
  EXPECT_EQ(xml.find("arm.cmsis.swo.tg.itm"), std::string::npos);
  EXPECT_EQ(xml.find("arm.cmsis.swo.tg.trace_status"), std::string::npos);
  EXPECT_EQ(xml.find("<entry path=\"*/"), std::string::npos);
  EXPECT_NE(xml.find("value=\"hostId\""), std::string::npos);
  EXPECT_NE(xml.find("value=\"context.cmsis_trace_bus_id\""), std::string::npos);
  EXPECT_EQ(xml.find("value=\"context.ctrace_route\""), std::string::npos);
}

TEST(CtraceUnitTests, testTraceCompassXmlClosesSleepOnPcSampleProhibited)
{
  const TemporaryTestPath root("ctrace-trace-compass-pc-sample-prohibited-test");
  root.createDirectory();
  const auto views = TraceCompassXmlWriter::viewMask(TraceCompassXmlWriter::View::ProcessorState);
  for (const auto id : {0U, 1U}) {
    const auto path = root.path() / (std::to_string(id) + ".xml");
    TraceCompassXmlWriter::writeFile(path, {viewRoute(static_cast<std::uint8_t>(id), {}, views)});
    const auto xml = readTestTextFile(path);
    const auto handlerStart = xml.find("<eventHandler eventName=\"PC_SAMPLE_PROHIBITED\">");
    ASSERT_NE(handlerStart, std::string::npos);
    const auto handlerEnd = xml.find("</eventHandler>", handlerStart);
    ASSERT_NE(handlerEnd, std::string::npos);
    const auto handler = xml.substr(handlerStart, handlerEnd - handlerStart);
    EXPECT_NE(handler.find("<stateAttribute type=\"constant\" value=\"PC_SAMPLE\" />"), std::string::npos);
    EXPECT_NE(handler.find("<stateAttribute type=\"constant\" value=\"Sleep\" />"), std::string::npos);
    EXPECT_NE(handler.find("<stateValue type=\"null\" />"), std::string::npos);
    EXPECT_NE(handler.find("value=\"hostId\""), std::string::npos);
    EXPECT_NE(handler.find("value=\"context.cmsis_trace_bus_id\""), std::string::npos);
    EXPECT_EQ(handler.find("value=\"context.ctrace_route\""), std::string::npos);
    EXPECT_EQ(handler.find("cmsis_pc_sample_state"), std::string::npos);
    EXPECT_EQ(handler.find("<futureTime"), std::string::npos);
    EXPECT_EQ(handler.find("stack="), std::string::npos);
    EXPECT_EQ(handler.find("<stateValue type=\"int\""), std::string::npos);
    EXPECT_EQ(xml.find("<definedValue name=\"Trace prohibited\""), std::string::npos);
  }
}

TEST(CtraceUnitTests, testTraceCompassXmlEmitsOnlySelectedGraphicalViews)
{
  const TemporaryTestPath path("ctrace-trace-compass-selected-route-views.xml");
  using View = TraceCompassXmlWriter::View;
  TraceCompassXmlWriter::writeFile(path.path(), {
      viewRoute(1U, "CM4", TraceCompassXmlWriter::viewMask(View::Exception)),
      viewRoute(2U, "CM7", TraceCompassXmlWriter::viewMask(View::DwtValue)),
      viewRoute(3U, "silent", 0U),
  });
  const auto xml = readTestTextFile(path.path());

  EXPECT_NE(xml.find("id=\"" + xmlViewId(xml, "arm.cmsis.swo.tg.exception", 1U) + "\""), std::string::npos);
  EXPECT_NE(xml.find("id=\"" + xmlViewId(xml, "arm.cmsis.swo.xy.dwt_value", 2U) + "\""), std::string::npos);
  EXPECT_EQ(xml.find("id=\"" + xmlViewId(xml, "arm.cmsis.swo.xy.dwt_value", 1U) + "\""), std::string::npos);
  EXPECT_EQ(xml.find("id=\"" + xmlViewId(xml, "arm.cmsis.swo.tg.exception", 2U) + "\""), std::string::npos);
  EXPECT_EQ(xml.find(" - silent"), std::string::npos);
  EXPECT_EQ(xml.find("arm.cmsis.swo.xy.dwt_addr"), std::string::npos);
  EXPECT_EQ(xml.find("arm.cmsis.swo.tg.dwt_match"), std::string::npos);
  EXPECT_EQ(xml.find("arm.cmsis.swo.tg.dwt_event"), std::string::npos);
  EXPECT_EQ(xml.find("arm.cmsis.swo.tg.pmu_event"), std::string::npos);
  EXPECT_EQ(xml.find("arm.cmsis.swo.tg.processor_state"), std::string::npos);
  EXPECT_EQ(xml.find("eventName=\"DWT_ADDR\""), std::string::npos);
  EXPECT_EQ(xml.find("eventName=\"DWT_MATCH\""), std::string::npos);
  EXPECT_EQ(xml.find("eventName=\"DWT_EVENT\""), std::string::npos);
  EXPECT_EQ(xml.find("eventName=\"PMU_EVENT\""), std::string::npos);
  EXPECT_EQ(xml.find("eventName=\"PC_SAMPLE\""), std::string::npos);
  EXPECT_EQ(xml.find("eventName=\"PC_SAMPLE_PROHIBITED\""), std::string::npos);
}

TEST(CtraceUnitTests, testTraceCompassXmlEmitsOnlyHandlersRequiredBySelectedViews)
{
  using View = TraceCompassXmlWriter::View;
  const TemporaryTestPath valuePath("ctrace-trace-compass-dwt-value-handler.xml");
  TraceCompassXmlWriter::writeFile(valuePath.path(), {viewRoute(0U, {}, TraceCompassXmlWriter::viewMask(View::DwtValue))});
  const auto valueXml = readTestTextFile(valuePath.path());

  EXPECT_NE(valueXml.find("eventName=\"DWT_VALUE\""), std::string::npos);
  EXPECT_EQ(valueXml.find("eventName=\"DWT_ADDR\""), std::string::npos);
  EXPECT_EQ(valueXml.find("eventName=\"DWT_MATCH\""), std::string::npos);
  EXPECT_EQ(valueXml.find("eventName=\"DWT_EVENT\""), std::string::npos);
  EXPECT_EQ(valueXml.find("eventName=\"PMU_EVENT\""), std::string::npos);
  EXPECT_EQ(valueXml.find("eventName=\"EXCEPTION\""), std::string::npos);
  EXPECT_EQ(valueXml.find("eventName=\"PC_SAMPLE\""), std::string::npos);
  EXPECT_EQ(valueXml.find("eventName=\"PC_SAMPLE_PROHIBITED\""), std::string::npos);
  EXPECT_EQ(valueXml.find("eventName=\"TRACE_STATUS\""), std::string::npos);

  const TemporaryTestPath exceptionPath("ctrace-trace-compass-exception-handler.xml");
  TraceCompassXmlWriter::writeFile(exceptionPath.path(), {
      viewRoute(0U, {}, TraceCompassXmlWriter::viewMask(View::Exception)),
  });
  const auto exceptionXml = readTestTextFile(exceptionPath.path());

  EXPECT_NE(exceptionXml.find("eventName=\"EXCEPTION\""), std::string::npos);
  EXPECT_NE(exceptionXml.find("eventName=\"TRACE_STATUS\""), std::string::npos);
  EXPECT_EQ(exceptionXml.find("eventName=\"PC_SAMPLE\""), std::string::npos);
  EXPECT_EQ(exceptionXml.find("eventName=\"PC_SAMPLE_PROHIBITED\""), std::string::npos);
  EXPECT_EQ(exceptionXml.find("value=\"PC_SAMPLE\""), std::string::npos);

  const TemporaryTestPath processorPath("ctrace-trace-compass-processor-handler.xml");
  TraceCompassXmlWriter::writeFile(processorPath.path(), {
      viewRoute(0U, {}, TraceCompassXmlWriter::viewMask(View::ProcessorState)),
  });
  const auto processorXml = readTestTextFile(processorPath.path());

  EXPECT_NE(processorXml.find("eventName=\"PC_SAMPLE\""), std::string::npos);
  EXPECT_NE(processorXml.find("eventName=\"PC_SAMPLE_PROHIBITED\""), std::string::npos);
  EXPECT_NE(processorXml.find("eventName=\"TRACE_STATUS\""), std::string::npos);
  EXPECT_EQ(processorXml.find("eventName=\"EXCEPTION\""), std::string::npos);
  EXPECT_EQ(processorXml.find("value=\"EXCEPTION_RETURN\""), std::string::npos);
}

TEST(CtraceUnitTests, testTraceCompassXmlSeparatesRepeatedTraceIdsAcrossCaptures)
{
  const TemporaryTestPath path("ctrace-trace-compass-multiple-captures.xml");
  using Writer = TraceCompassXmlWriter;
  const auto value = Writer::viewMask(Writer::View::DwtValue);
  const auto sleep = Writer::viewMask(Writer::View::ProcessorState);
  Writer::writeFile(path.path(), {
      viewRoute(1U, "SWO - CM4", value),
      viewRoute(1U, "TB - CM4", sleep, kSecondClockUuid),
  });
  const auto xml = readTestTextFile(path.path());
  const auto ids = xmlAttributes(xml, "(?:stateProvider|xyView|timeGraphView)", "id");
  EXPECT_EQ(ids.size(), 3U);
  EXPECT_EQ(std::set<std::string>(ids.begin(), ids.end()).size(), ids.size());
  const auto references = xmlAttributes(xml, "analysis", "id");
  ASSERT_EQ(references.size(), 2U);
  EXPECT_TRUE(std::all_of(references.begin(), references.end(), [&](const auto& id) { return id == xmlAnalysisId(xml); }));
  EXPECT_NE(xml.find("<entry path=\"" + xmlRoutePath("DWT_VALUE/*", 1U) + "\""), std::string::npos);
  EXPECT_EQ(xml.find("<entry path=\"" + xmlRoutePath("PC_SAMPLE/*", 1U) + "\""), std::string::npos);
  EXPECT_NE(xml.find("<entry path=\"" + xmlRoutePath("PC_SAMPLE/*", 1U, kSecondClockUuid) + "\""), std::string::npos);
  EXPECT_EQ(xml.find("<entry path=\"" + xmlRoutePath("DWT_VALUE/*", 1U, kSecondClockUuid) + "\""), std::string::npos);
  EXPECT_NE(xml.find("<label value=\"DWT_VALUE - SWO - CM4\""), std::string::npos);
  EXPECT_NE(xml.find("<label value=\"Processor State - TB - CM4\""), std::string::npos);
}

TEST(CtraceUnitTests, testTraceCompassXmlNamespacesIndependentCaptures)
{
  const TemporaryTestPath firstPath("ctrace-trace-compass-first-namespace.xml");
  const TemporaryTestPath secondPath("ctrace-trace-compass-second-namespace.xml");
  TraceCompassXmlWriter::writeFile(firstPath.path(), {viewRoute()});
  TraceCompassXmlWriter::writeFile(secondPath.path(), {
      viewRoute(0U, {}, TraceCompassXmlWriter::AllViews, kSecondClockUuid),
  });
  const auto first = readTestTextFile(firstPath.path());
  const auto second = readTestTextFile(secondPath.path());
  const auto firstIds = xmlAttributes(first, "(?:stateProvider|xyView|timeGraphView)", "id");
  const auto secondIds = xmlAttributes(second, "(?:stateProvider|xyView|timeGraphView)", "id");
  ASSERT_EQ(firstIds.size(), 8U);
  ASSERT_EQ(secondIds.size(), 8U);
  for (const auto& id : firstIds) {
    EXPECT_EQ(std::find(secondIds.begin(), secondIds.end(), id), secondIds.end()) << id;
  }
}

TEST(CtraceUnitTests, testTraceCompassXmlMatchesQuotedAndUnquotedClockIdentityExactly)
{
  const TemporaryTestPath path("ctrace-trace-compass-quoted-clock.xml");
  TraceCompassXmlWriter::writeFile(path.path(), {viewRoute()});
  const auto paths = xmlAttributes(readTestTextFile(path.path()), "entry", "path");
  ASSERT_FALSE(paths.empty());
  const auto clockPattern = paths.front().substr(0U, paths.front().find('/'));
  const std::regex expression(std::regex_replace(clockPattern, std::regex("&quot;"), "\""));
  EXPECT_TRUE(std::regex_match(kFirstClockUuid, expression));
  EXPECT_TRUE(std::regex_match('"' + std::string(kFirstClockUuid) + '"', expression));
  EXPECT_FALSE(std::regex_match(kSecondClockUuid, expression));
  EXPECT_FALSE(std::regex_match(std::string(kFirstClockUuid) + "extra", expression));
  EXPECT_FALSE(std::regex_match('"' + std::string(kFirstClockUuid), expression));
  EXPECT_FALSE(std::regex_match(std::string(kFirstClockUuid) + '"', expression));
}

TEST(CtraceUnitTests, testTraceCompassXmlKeepsNamespaceStableAndVersionsViewChanges)
{
  const TemporaryTestPath path("ctrace-trace-compass-stable-namespace.xml");
  std::vector<TraceCompassXmlWriter::ViewRoute> routes{
      viewRoute(0U, "SWO"), viewRoute(1U, "TB", TraceCompassXmlWriter::AllViews, kSecondClockUuid),
  };
  TraceCompassXmlWriter::writeFile(path.path(), routes);
  const auto original = readTestTextFile(path.path());
  std::reverse(routes.begin(), routes.end());
  TraceCompassXmlWriter::writeFile(path.path(), routes);
  EXPECT_EQ(xmlAnalysisId(original), xmlAnalysisId(readTestTextFile(path.path())));
  routes.front().label = "TB - renamed";
  routes.front().views = TraceCompassXmlWriter::viewMask(TraceCompassXmlWriter::View::DwtValue);
  TraceCompassXmlWriter::writeFile(path.path(), routes);
  const auto updated = readTestTextFile(path.path());
  EXPECT_EQ(xmlAnalysisId(original), xmlAnalysisId(updated));
  EXPECT_NE(xmlAttributes(original, "stateProvider", "version"), xmlAttributes(updated, "stateProvider", "version"));
  EXPECT_EQ(updated.find("__SWO_ANALYSIS_VERSION__"), std::string::npos);
  routes.front().traceBusId = 2U;
  TraceCompassXmlWriter::writeFile(path.path(), routes);
  EXPECT_NE(xmlAnalysisId(original), xmlAnalysisId(readTestTextFile(path.path())));
}

TEST(CtraceUnitTests, testTraceCompassXmlRejectsInvalidRouteViews)
{
  const TemporaryTestPath path("ctrace-trace-compass-invalid-route-views.xml");
  EXPECT_THROW(TraceCompassXmlWriter::writeFile(path.path(), {}), std::invalid_argument);
  for (const auto id : {112U, 127U, 255U}) {
    EXPECT_THROW(TraceCompassXmlWriter::writeFile(path.path(), {viewRoute(static_cast<std::uint8_t>(id))}),
                 std::invalid_argument);
  }
  EXPECT_THROW(TraceCompassXmlWriter::writeFile(path.path(), {viewRoute(1U, "first"), viewRoute(1U, "duplicate")}),
               std::invalid_argument);
  EXPECT_THROW(TraceCompassXmlWriter::writeFile(path.path(), {viewRoute(0U, {}, TraceCompassXmlWriter::AllViews + 1U)}),
               std::invalid_argument);
  EXPECT_THROW(TraceCompassXmlWriter::writeFile(path.path(), {viewRoute(0U, {}, 0U)}), std::invalid_argument);
  EXPECT_FALSE(std::filesystem::exists(path.path()));
}

TEST(CtraceUnitTests, testTraceCompassXmlRejectsMalformedClockUuidBeforeReplacingOutput)
{
  const TemporaryTestPath path("ctrace-trace-compass-invalid-clock.xml");
  writeTestFile(path.path(), "previous output");
  for (const auto* uuid : {"", "not-a-uuid", "00000000-0000-4000-8000-00000000000",
                           "00000000-0000-4000-8000-0000000000000", "00000000_0000-4000-8000-000000000000",
                           "00000000-0000-4000-8000-00000000000A", "00000000-0000-4000-8000-00000000000g",
                           "00000000-0000-4000-8000-00000000000/", "../*/1", "\"/><entry path=\"*"}) {
    SCOPED_TRACE(uuid);
    EXPECT_THROW(TraceCompassXmlWriter::writeFile(path.path(), {viewRoute(0U, {}, TraceCompassXmlWriter::AllViews, uuid)}),
                 std::invalid_argument);
    EXPECT_EQ(readTestTextFile(path.path()), "previous output");
  }
}

TEST(CtraceUnitTests, testTraceCompassXmlWriterReportsDeviceWriteFailures)
{
  if (!TestPlatform::supports(TestPlatformCapability::LinuxSpecialFiles)) {
    GTEST_SKIP();
  }
  EXPECT_THROW(TraceCompassXmlWriter::writeFile(TestPlatform::writeFailurePath(), {viewRoute()}), std::runtime_error);
}
