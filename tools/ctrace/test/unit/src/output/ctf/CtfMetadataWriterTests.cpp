/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CtfTestSupport.h"
#include "TestPath.h"
#include "TestPlatform.h"
#include "TestSupport.h"

#include <gtest/gtest.h>

#include "ctf/CtfMetadataWriter.h"
#include "ctf/CtfSchema.h"
#include "ctf/TraceCompassXmlWriter.h"
#include "TraceOutputConfig.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

TEST(CtraceUnitTests, testCtfMetadataWriterEscapesAndDeduplicatesSourceLabels)
{
  const TemporaryTestPath path("ctrace-metadata-writer");
  path.createDirectory();
  const TraceRouteIdentity route{};
  const std::vector<CtfSourceDescriptor> sources{
      {"itm", 1U, route, std::string("ITM3"), std::nullopt, "unsigned", 4U},
      {"itm", 2U, route, std::string("ITM3_1"), std::nullopt, "unsigned", 4U},
      {"itm", 3U, route, std::string("ITM3"), std::nullopt, "unsigned", 4U},
      {"itm", 4U, route, std::string("line\rbreak"), std::nullopt, "unsigned", 4U},
      {"itm", 5U, route, std::nullopt, std::nullopt, "unsigned", 4U},
      {"itm", 6U, route, std::string("ITM3"), std::nullopt, "unsigned", 4U},
      {"dwt", 0U, route, std::nullopt, std::numeric_limits<std::uint64_t>::max() - 3U, "unsigned", 4U},
  };

  CtfMetadataModel model(CtfTestSupport::testUuid(), CtfTestSupport::legacyTopology(1000000U, route, sources));
  for (const auto number : {8U, 10U, 13U, 16U, 54U}) {
    model.observeException(CtfStreamClassId{0U}, number);
  }
  CtfMetadataWriter::write(path.path(), model);
  const auto metadata = readTestTextFile(path.path() / "metadata");
  EXPECT_NE(metadata.find("ITM3_2"), std::string::npos);
  EXPECT_NE(metadata.find("\"ITM6\" = 6"), std::string::npos);
  EXPECT_NE(metadata.find("line\\rbreak"), std::string::npos);
  EXPECT_NE(metadata.find("\"Reserved 8\" = 8"), std::string::npos);
  EXPECT_NE(metadata.find("\"Reserved 10\" = 10"), std::string::npos);
  EXPECT_NE(metadata.find("\"Reserved 13\" = 13"), std::string::npos);
  EXPECT_NE(metadata.find("\"External IRQ 0\" = 16"), std::string::npos);
  EXPECT_NE(metadata.find("\"External IRQ 38\" = 54"), std::string::npos);
  EXPECT_NE(metadata.find("\"Thread Mode\" = 0"), std::string::npos);
  EXPECT_NE(metadata.find("cmsis_exception_origin_t cmsis_exception_origin;"), std::string::npos);
  EXPECT_NE(metadata.find("\"CPICNT\" = 0"), std::string::npos);
  EXPECT_NE(metadata.find("\"CYCCNT\" = 5"), std::string::npos);
  EXPECT_NE(metadata.find("cmsis_dwt_event_counter_t cmsis_dwt_event_counter;"), std::string::npos);
  EXPECT_NE(metadata.find("\"Event0\" = 0"), std::string::npos);
  EXPECT_NE(metadata.find("\"Event7\" = 7"), std::string::npos);
  EXPECT_NE(metadata.find("cmsis_pmu_event_counter_t cmsis_pmu_event_counter;"), std::string::npos);
  EXPECT_NE(metadata.find("none = 0, u8 = 1, u16 = 2, u32 = 4"), std::string::npos);
  EXPECT_NE(metadata.find("variant <cmsis_dwt_pc_type>"), std::string::npos);
  EXPECT_NE(metadata.find("variant <cmsis_dwt_address_type>"), std::string::npos);
  EXPECT_NE(metadata.find("uint32_t u32;"), std::string::npos);
  EXPECT_NE(metadata.find("cmsis_dwt0_address_end = \"0xFFFFFFFFFFFFFFFF\""), std::string::npos);
}

TEST(CtraceUnitTests, testCtfMetadataWriterRejectsMissingOutputDirectory)
{
  const TemporaryTestPath path("ctrace-metadata-writer-missing");
  const CtfMetadataModel model(CtfTestSupport::testUuid(), CtfTestSupport::legacyTopology(1U));
  EXPECT_THROW(CtfMetadataWriter::write(path.path(), model), std::runtime_error);
}

TEST(CtraceUnitTests, testCtfMetadataWriterSerializesRouteScopedMultiStreamTopology)
{
  const TemporaryTestPath path("ctrace-multistream-metadata-writer");
  path.createDirectory();
  const TraceRouteIdentity first{TraceRouteId{8U}, 1U};
  const TraceRouteIdentity second{TraceRouteId{91U}, 111U};
  CtfMetadataTopology topology{
      {
          {CtfClockDomainId{19U}, "clock_nineteen", CtfTestSupport::testUuid(19U), 240000000U, false},
          {CtfClockDomainId{3U}, "clock_three", CtfTestSupport::testUuid(3U), 240000000U, false},
      },
      {
          {CtfStreamClassId{111U}, second, CtfSourceKind::Itm, std::string("second"), CtfClockDomainId{19U}},
          {CtfStreamClassId{1U}, first, CtfSourceKind::Itm, std::string("first"), CtfClockDomainId{3U}},
      },
      {
          {"dwt", 0U, first, std::string("First DWT"), 0x1000U, "unsigned", 4U},
          {"dwt", 0U, second, std::string("Second DWT"), 0x2000U, "signed", 2U},
          {"itm", 1U, first, std::string("First console"), std::nullopt, "unsigned", 4U},
          {"itm", 1U, second, std::string("Second console"), std::nullopt, "unsigned", 4U},
      },
  };
  CtfMetadataModel model(CtfTestSupport::testUuid(), std::move(topology));
  model.observeException(CtfStreamClassId{1U}, 54U);
  model.observeException(CtfStreamClassId{111U}, 75U);
  CtfMetadataWriter::write(path.path(), model);

  const auto metadata = readTestTextFile(path.path() / "metadata");
  EXPECT_NE(metadata.find("uuid = \"" + CtfTestSupport::testUuid().toString() + "\";"), std::string::npos);
  EXPECT_NE(metadata.find("name = clock_three;"), std::string::npos);
  EXPECT_NE(metadata.find("uuid = \"" + CtfTestSupport::testUuid(3U).toString() + "\";"), std::string::npos);
  EXPECT_NE(metadata.find("name = clock_nineteen;"), std::string::npos);
  EXPECT_NE(metadata.find("uuid = \"" + CtfTestSupport::testUuid(19U).toString() + "\";"), std::string::npos);
  EXPECT_NE(metadata.find("map = clock.clock_three.value; } := clock_three_t;"), std::string::npos);
  EXPECT_NE(metadata.find("map = clock.clock_nineteen.value; } := clock_nineteen_t;"), std::string::npos);
  EXPECT_NE(metadata.find("stream {\n    id = 1;"), std::string::npos);
  EXPECT_NE(metadata.find("stream {\n    id = 111;"), std::string::npos);
  EXPECT_NE(metadata.find("stream_id = 1;"), std::string::npos);
  EXPECT_NE(metadata.find("stream_id = 111;"), std::string::npos);
  EXPECT_NE(metadata.find("cmsis_stream_1_processor_name = \"first\";"), std::string::npos);
  EXPECT_NE(metadata.find("cmsis_stream_111_processor_name = \"second\";"), std::string::npos);
  EXPECT_NE(metadata.find("\"first\" = 1,\n} := cmsis_stream_1_route_t;"), std::string::npos);
  EXPECT_NE(metadata.find("\"second\" = 111,\n} := cmsis_stream_111_route_t;"), std::string::npos);
  EXPECT_NE(metadata.find("uint8_t cmsis_trace_bus_id;\n        cmsis_stream_1_route_t ctrace_route;"),
            std::string::npos);
  EXPECT_NE(metadata.find("uint8_t cmsis_trace_bus_id;\n        cmsis_stream_111_route_t ctrace_route;"),
            std::string::npos);
  EXPECT_NE(metadata.find("cmsis_stream_1_dwt0_value_type = \"unsigned\";"), std::string::npos);
  EXPECT_NE(metadata.find("cmsis_stream_111_dwt0_value_type = \"signed\";"), std::string::npos);
  EXPECT_NE(metadata.find("cmsis_stream_1_dwt0_address_start = \"0x1000\";"), std::string::npos);
  EXPECT_NE(metadata.find("cmsis_stream_111_dwt0_address_end = \"0x2001\";"), std::string::npos);
  EXPECT_NE(metadata.find("\"First DWT\" = 0"), std::string::npos);
  EXPECT_NE(metadata.find("\"Second DWT\" = 0"), std::string::npos);
  EXPECT_NE(metadata.find("\"First console\" = 1"), std::string::npos);
  EXPECT_NE(metadata.find("\"Second console\" = 1"), std::string::npos);
  EXPECT_NE(metadata.find("\"External IRQ 38\" = 54"), std::string::npos);
  EXPECT_NE(metadata.find("\"External IRQ 59\" = 75"), std::string::npos);
  EXPECT_EQ(metadata.find("\n    cmsis_dwt0_value_type"), std::string::npos);
  EXPECT_EQ(metadata.find("name = swo_clock;"), std::string::npos);
  EXPECT_EQ(metadata.find("stream_id = 0;"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(path.path() / "stream_0"));
}

TEST(CtraceUnitTests, testCtfMetadataWriterUsesStreamClassIdLabelForUnboundRoute)
{
  const TemporaryTestPath path("ctrace-unbound-route-metadata-writer");
  path.createDirectory();
  const TraceRouteIdentity route{TraceRouteId{8U}, 7U};
  CtfMetadataTopology topology{
      {{CtfClockDomainId{3U}, "clock_three", CtfTestSupport::testUuid(3U), 240000000U, false}},
      {{CtfStreamClassId{7U}, route, CtfSourceKind::Itm, std::nullopt, CtfClockDomainId{3U}}},
      {},
  };
  CtfMetadataWriter::write(path.path(), CtfMetadataModel(CtfTestSupport::testUuid(), std::move(topology)));

  const auto metadata = readTestTextFile(path.path() / "metadata");
  EXPECT_NE(metadata.find("\"7\" = 7,\n} := cmsis_stream_7_route_t;"), std::string::npos);
  EXPECT_NE(metadata.find("uint8_t cmsis_trace_bus_id;\n        cmsis_stream_7_route_t ctrace_route;"),
            std::string::npos);
  EXPECT_EQ(metadata.find("cmsis_stream_7_processor_name"), std::string::npos);
}

TEST(CtraceUnitTests, testTraceCompassXmlWriterRejectsDirectoryTarget)
{
  const TemporaryTestPath path("ctrace-trace-compass-directory-target");
  path.createDirectory();
  EXPECT_THROW(TraceCompassXmlWriter::writeFile(path.path()), std::runtime_error);
}

TEST(CtraceUnitTests, testTraceCompassXmlWriterSupportsParentlessTarget)
{
  const auto path = std::filesystem::path("ctrace-parentless-trace-compass.xml");
  std::error_code ignored;
  std::filesystem::remove(path, ignored);
  EXPECT_NO_THROW(TraceCompassXmlWriter::writeFile(path));
  EXPECT_TRUE(std::filesystem::is_regular_file(path));
  std::filesystem::remove(path, ignored);
}

TEST(CtraceUnitTests, testTraceCompassXmlUsesCurrentCtfEvents)
{
  const TemporaryTestPath path("ctrace-trace-compass-schema.xml");
  TraceCompassXmlWriter::writeFile(path.path());
  const auto xml = readTestTextFile(path.path());
  constexpr std::array<CtfSchema::EventId, 9U> visualizedEvents{
      CtfSchema::EventId::Itm,       CtfSchema::EventId::DwtValue,    CtfSchema::EventId::DwtAddress,
      CtfSchema::EventId::Exception, CtfSchema::EventId::TraceStatus, CtfSchema::EventId::PcSample,
      CtfSchema::EventId::DwtEvent,  CtfSchema::EventId::PmuEvent,    CtfSchema::EventId::DwtMatch,
  };

  for (const auto eventId : visualizedEvents) {
    EXPECT_NE(xml.find("eventName=\"" + std::string(CtfSchema::eventName(eventId)) + "\""), std::string::npos);
  }
  EXPECT_NE(xml.find("value=\"cmsis_pc_sample_state\""), std::string::npos);
  EXPECT_NE(xml.find("<stateAttribute type=\"constant\" value=\"Sleep\" />"), std::string::npos);
  EXPECT_NE(xml.find("<stateValue type=\"int\" value=\"0\" />"), std::string::npos);
  EXPECT_EQ(xml.find("<stateValue type=\"string\" value=\"Sleep\" />"), std::string::npos);
  EXPECT_NE(xml.find("<label value=\"PC Sampling\" />"), std::string::npos);
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

TEST(CtraceUnitTests, testCtfTextWritersReportDeviceWriteFailures)
{
  if (!TestPlatform::supports(TestPlatformCapability::LinuxSpecialFiles)) {
    GTEST_SKIP();
  }
  const TemporaryTestPath path("ctrace-metadata-device-failure");
  path.createDirectory();
  std::filesystem::create_symlink(TestPlatform::writeFailurePath(), path.path() / "metadata");
  const CtfMetadataModel model(CtfTestSupport::testUuid(), CtfTestSupport::legacyTopology(1U));
  EXPECT_THROW(CtfMetadataWriter::write(path.path(), model), std::runtime_error);
  EXPECT_THROW(TraceCompassXmlWriter::writeFile(TestPlatform::writeFailurePath()), std::runtime_error);
}
