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

#include <array>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>

TEST(CtraceUnitTests, testTraceCompassXmlWriterRejectsDirectoryTarget)
{
  const TemporaryTestPath path("ctrace-trace-compass-directory-target");
  path.createDirectory();
  EXPECT_THROW(TraceCompassXmlWriter::writeLegacyFile(path.path()), std::runtime_error);
}

TEST(CtraceUnitTests, testTraceCompassXmlWriterSupportsParentlessTarget)
{
  const auto path = std::filesystem::path("ctrace-parentless-trace-compass.xml");
  std::error_code ignored;
  std::filesystem::remove(path, ignored);
  EXPECT_NO_THROW(TraceCompassXmlWriter::writeLegacyFile(path));
  EXPECT_TRUE(std::filesystem::is_regular_file(path));
  std::filesystem::remove(path, ignored);
}

TEST(CtraceUnitTests, testTraceCompassXmlUsesCurrentCtfEvents)
{
  const TemporaryTestPath path("ctrace-trace-compass-schema.xml");
  TraceCompassXmlWriter::writeLegacyFile(path.path());
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
  EXPECT_NE(xml.find("<entry path=\"PC_SAMPLE/" "*\" displayText=\"true\"><display type=\"self\" /></entry>"),
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
  EXPECT_NE(xml.find("<entry path=\"DWT_EVENT/" "*\" displayText=\"true\"><display type=\"constant\" value=\"1\" /></entry>"),
            std::string::npos);
  EXPECT_NE(xml.find("value=\"cmsis_pmu_event_counter\""), std::string::npos);
  EXPECT_NE(xml.find("<stateValue type=\"string\" value=\"Event0\" />"), std::string::npos);
  EXPECT_NE(xml.find("<stateAttribute type=\"constant\" value=\"Event7\" />"), std::string::npos);
  EXPECT_NE(xml.find("<stateValue type=\"int\" value=\"7\" stack=\"push\" />"), std::string::npos);
  EXPECT_NE(xml.find("<label value=\"PMU Event Counters\" />"), std::string::npos);
  EXPECT_NE(xml.find("<definedValue name=\"7\" value=\"7\""), std::string::npos);
  EXPECT_NE(xml.find("<entry path=\"PMU_EVENT/" "*\" displayText=\"true\"><display type=\"constant\" value=\"1\" /></entry>"),
            std::string::npos);
  EXPECT_NE(xml.find("<stateAttribute type=\"constant\" value=\"DWT_MATCH\" />"), std::string::npos);
  EXPECT_NE(xml.find("<stateAttribute type=\"eventField\" value=\"cmsis_dwt_comparator\" />"),
            std::string::npos);
  EXPECT_NE(xml.find("<label value=\"DWT Match\" />"), std::string::npos);
  EXPECT_NE(xml.find("<definedValue name=\"Something happened\" value=\"1\""), std::string::npos);
  EXPECT_NE(xml.find("<entry path=\"DWT_MATCH/" "*\" displayText=\"true\"><display type=\"constant\" value=\"1\" />"
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
  const auto threadModeEntry = xml.find("path=\"EXCEPTION/Thread Mode\"");
  const auto returnEntry = xml.find("path=\"EXCEPTION_RETURN/*\" displayText=\"true\"");
  const auto interruptEntries = xml.find("path=\"EXCEPTION/(?!Thread Mode).+\"");
  ASSERT_NE(threadModeEntry, std::string::npos);
  ASSERT_NE(returnEntry, std::string::npos);
  ASSERT_NE(interruptEntries, std::string::npos);
  EXPECT_LT(threadModeEntry, returnEntry);
  EXPECT_LT(returnEntry, interruptEntries);
}

TEST(CtraceUnitTests, testTraceCompassXmlScopesGraphicalViewsPerRoute)
{
  const TemporaryTestPath path("ctrace-trace-compass-route-views.xml");
  TraceCompassXmlWriter::writeRoutedFile(path.path(), {{1U, "CM4"}, {2U, "CM&<7>\"'"}, {3U, {}}});
  const auto xml = readTestTextFile(path.path());

  EXPECT_NE(xml.find("id=\"arm.cmsis.swo.xy.dwt_value.stream1.v1\""), std::string::npos);
  EXPECT_NE(xml.find("id=\"arm.cmsis.swo.xy.dwt_value.stream2.v1\""), std::string::npos);
  EXPECT_NE(xml.find("<label value=\"DWT_VALUE - CM4\" />"), std::string::npos);
  EXPECT_NE(xml.find("<label value=\"DWT_VALUE - CM&amp;&lt;7&gt;&quot;&apos;\" />"), std::string::npos);
  EXPECT_EQ(xml.find("<label value=\"DWT_VALUE - 1\" />"), std::string::npos);
  EXPECT_EQ(xml.find("<label value=\"DWT_VALUE - 3\" />"), std::string::npos);
  EXPECT_NE(xml.find("id=\"arm.cmsis.swo.xy.dwt_value.stream3.v1\">\n"
                     "        <head><analysis id=\"arm.cmsis.swo.analysis.v1\" /><label value=\"DWT_VALUE\" />"),
            std::string::npos);
  EXPECT_NE(xml.find("<entry path=\"*/1/DWT_VALUE/*\""), std::string::npos);
  EXPECT_NE(xml.find("<entry path=\"*/2/DWT_VALUE/*\""), std::string::npos);
  EXPECT_NE(xml.find("<entry path=\"*/1/PC_SAMPLE/*\""), std::string::npos);
  EXPECT_NE(xml.find("<label value=\"Processor State - CM4\" />"), std::string::npos);
  EXPECT_EQ(xml.find("<label value=\"PC Sampling - CM4\" />"), std::string::npos);
  EXPECT_EQ(xml.find("arm.cmsis.swo.tg.itm"), std::string::npos);
  EXPECT_EQ(xml.find("arm.cmsis.swo.tg.trace_status"), std::string::npos);
  EXPECT_EQ(xml.find("<entry path=\"*/*/"), std::string::npos);
}

TEST(CtraceUnitTests, testTraceCompassXmlClosesSleepOnPcSampleProhibited)
{
  const TemporaryTestPath root("ctrace-trace-compass-pc-sample-prohibited-test");
  root.createDirectory();
  const auto views = TraceCompassXmlWriter::viewMask(TraceCompassXmlWriter::View::ProcessorState);
  for (const auto routePrefixed : {false, true}) {
    const auto path = root.path() / (routePrefixed ? "routed.xml" : "legacy.xml");
    if (routePrefixed) {
      TraceCompassXmlWriter::writeRoutedFile(path, {{1U, "CM4", views}, {2U, "CM7", views}});
    } else {
      TraceCompassXmlWriter::writeLegacyFile(path, views);
    }
    const auto xml = readTestTextFile(path);
    const auto handlerStart = xml.find("<eventHandler eventName=\"PC_SAMPLE_PROHIBITED\">");
    ASSERT_NE(handlerStart, std::string::npos);
    const auto handlerEnd = xml.find("</eventHandler>", handlerStart);
    ASSERT_NE(handlerEnd, std::string::npos);
    const auto handler = xml.substr(handlerStart, handlerEnd - handlerStart);
    EXPECT_NE(handler.find("<stateAttribute type=\"constant\" value=\"PC_SAMPLE\" />"), std::string::npos);
    EXPECT_NE(handler.find("<stateAttribute type=\"constant\" value=\"Sleep\" />"), std::string::npos);
    EXPECT_NE(handler.find("<stateValue type=\"null\" />"), std::string::npos);
    EXPECT_EQ(handler.find("value=\"context.ctrace_route\"") != std::string::npos, routePrefixed);
    EXPECT_EQ(handler.find("value=\"context.cmsis_trace_bus_id\"") != std::string::npos, routePrefixed);
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
  TraceCompassXmlWriter::writeRoutedFile(
      path.path(),
      {
          {1U, "CM4", TraceCompassXmlWriter::viewMask(View::Exception)},
          {2U, "CM7", TraceCompassXmlWriter::viewMask(View::DwtValue)},
      });
  const auto xml = readTestTextFile(path.path());

  EXPECT_NE(xml.find("id=\"arm.cmsis.swo.tg.exception.stream1.v1\""), std::string::npos);
  EXPECT_NE(xml.find("id=\"arm.cmsis.swo.xy.dwt_value.stream2.v1\""), std::string::npos);
  EXPECT_EQ(xml.find("id=\"arm.cmsis.swo.xy.dwt_value.stream1.v1\""), std::string::npos);
  EXPECT_EQ(xml.find("id=\"arm.cmsis.swo.tg.exception.stream2.v1\""), std::string::npos);
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
  TraceCompassXmlWriter::writeLegacyFile(valuePath.path(), TraceCompassXmlWriter::viewMask(View::DwtValue));
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
  TraceCompassXmlWriter::writeLegacyFile(exceptionPath.path(), TraceCompassXmlWriter::viewMask(View::Exception));
  const auto exceptionXml = readTestTextFile(exceptionPath.path());

  EXPECT_NE(exceptionXml.find("eventName=\"EXCEPTION\""), std::string::npos);
  EXPECT_NE(exceptionXml.find("eventName=\"TRACE_STATUS\""), std::string::npos);
  EXPECT_EQ(exceptionXml.find("eventName=\"PC_SAMPLE\""), std::string::npos);
  EXPECT_EQ(exceptionXml.find("eventName=\"PC_SAMPLE_PROHIBITED\""), std::string::npos);
  EXPECT_EQ(exceptionXml.find("value=\"PC_SAMPLE\""), std::string::npos);

  const TemporaryTestPath processorPath("ctrace-trace-compass-processor-handler.xml");
  TraceCompassXmlWriter::writeLegacyFile(processorPath.path(),
                                         TraceCompassXmlWriter::viewMask(View::ProcessorState));
  const auto processorXml = readTestTextFile(processorPath.path());

  EXPECT_NE(processorXml.find("eventName=\"PC_SAMPLE\""), std::string::npos);
  EXPECT_NE(processorXml.find("eventName=\"PC_SAMPLE_PROHIBITED\""), std::string::npos);
  EXPECT_NE(processorXml.find("eventName=\"TRACE_STATUS\""), std::string::npos);
  EXPECT_EQ(processorXml.find("eventName=\"EXCEPTION\""), std::string::npos);
  EXPECT_EQ(processorXml.find("value=\"EXCEPTION_RETURN\""), std::string::npos);
}

TEST(CtraceUnitTests, testTraceCompassXmlRejectsInvalidRouteViews)
{
  const TemporaryTestPath path("ctrace-trace-compass-invalid-route-views.xml");
  EXPECT_THROW(TraceCompassXmlWriter::writeRoutedFile(path.path(), {}), std::invalid_argument);
  EXPECT_THROW(TraceCompassXmlWriter::writeRoutedFile(path.path(), {{0U, "invalid"}}), std::invalid_argument);
  EXPECT_THROW(TraceCompassXmlWriter::writeRoutedFile(path.path(), {{112U, "invalid"}}), std::invalid_argument);
  EXPECT_THROW(TraceCompassXmlWriter::writeRoutedFile(path.path(), {{1U, "first"}, {1U, "duplicate"}}),
               std::invalid_argument);
  EXPECT_THROW(TraceCompassXmlWriter::writeLegacyFile(path.path(), TraceCompassXmlWriter::AllViews + 1U),
               std::invalid_argument);
  EXPECT_THROW(TraceCompassXmlWriter::writeRoutedFile(
                   path.path(), {{1U, "invalid", TraceCompassXmlWriter::AllViews + 1U}}),
               std::invalid_argument);
  EXPECT_FALSE(std::filesystem::exists(path.path()));
}

TEST(CtraceUnitTests, testTraceCompassXmlWriterReportsDeviceWriteFailures)
{
  if (!TestPlatform::supports(TestPlatformCapability::LinuxSpecialFiles)) {
    GTEST_SKIP();
  }
  EXPECT_THROW(TraceCompassXmlWriter::writeLegacyFile(TestPlatform::writeFailurePath()), std::runtime_error);
}
