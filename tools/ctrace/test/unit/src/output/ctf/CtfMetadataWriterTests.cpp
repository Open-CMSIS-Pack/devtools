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

#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
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
  EXPECT_NE(metadata.find("cmsis_ctf_profile_version = 1;"), std::string::npos);
  EXPECT_NE(metadata.find("id = 10;\n    name = \"PC_SAMPLE_PROHIBITED\";\n    stream_id = 0;\n"
                          "    fields := struct {\n        uint8_t cmsis_sample_flags;\n"
                          "        uint32_t cmsis_overflow_count;\n    };"),
            std::string::npos);
}

TEST(CtraceUnitTests, testCtfMetadataWriterRejectsMissingOutputDirectory)
{
  const TemporaryTestPath path("ctrace-metadata-writer-missing");
  const CtfMetadataModel model(CtfTestSupport::testUuid(), CtfTestSupport::legacyTopology(1U));
  EXPECT_THROW(CtfMetadataWriter::write(path.path(), model), std::runtime_error);
}

TEST(CtraceUnitTests, testCtfMetadataWriterIdentifiesLegacyClockWithoutChangingEventLayout)
{
  const TemporaryTestPath path("ctrace-legacy-clock-uuid");
  path.createDirectory();
  auto topology = CtfTestSupport::legacyTopology(1000000U);
  topology.clockDomains.front().uuid = CtfTestSupport::testUuid(1U);
  const CtfMetadataModel model(CtfTestSupport::testUuid(), std::move(topology));
  ASSERT_TRUE(model.isLegacySingleStreamLayout());
  CtfMetadataWriter::write(path.path(), model);
  const auto metadata = readTestTextFile(path.path() / "metadata");
  EXPECT_NE(metadata.find("name = swo_clock;\n    uuid = \"" + CtfTestSupport::testUuid(1U).toString() + "\";"),
            std::string::npos);
  EXPECT_NE(metadata.find("uint8_t cmsis_trace_bus_id;"), std::string::npos);
  EXPECT_EQ(metadata.find("ctrace_route"), std::string::npos);
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
          {CtfStreamClassId{111U}, second, std::string("second"), CtfClockDomainId{19U}},
          {CtfStreamClassId{1U}, first, std::string("first"), CtfClockDomainId{3U}},
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
  for (const auto streamId : {1U, 111U}) {
    EXPECT_NE(metadata.find("id = 10;\n    name = \"PC_SAMPLE_PROHIBITED\";\n    stream_id = " +
                            std::to_string(streamId) + ";\n    fields := struct {\n"
                            "        uint8_t cmsis_sample_flags;\n        uint32_t cmsis_overflow_count;\n    };"),
              std::string::npos);
  }
}

TEST(CtraceUnitTests, testCtfMetadataWriterUsesStreamClassIdLabelForUnboundRoute)
{
  const TemporaryTestPath path("ctrace-unbound-route-metadata-writer");
  path.createDirectory();
  const TraceRouteIdentity route{TraceRouteId{8U}, 7U};
  CtfMetadataTopology topology{
      {{CtfClockDomainId{3U}, "clock_three", CtfTestSupport::testUuid(3U), 240000000U, false}},
      {{CtfStreamClassId{7U}, route, std::nullopt, CtfClockDomainId{3U}}},
      {},
  };
  CtfMetadataWriter::write(path.path(), CtfMetadataModel(CtfTestSupport::testUuid(), std::move(topology)));

  const auto metadata = readTestTextFile(path.path() / "metadata");
  EXPECT_NE(metadata.find("\"7\" = 7,\n} := cmsis_stream_7_route_t;"), std::string::npos);
  EXPECT_NE(metadata.find("uint8_t cmsis_trace_bus_id;\n        cmsis_stream_7_route_t ctrace_route;"),
            std::string::npos);
  EXPECT_EQ(metadata.find("cmsis_stream_7_processor_name"), std::string::npos);
}


TEST(CtraceUnitTests, testCtfMetadataWriterReportsDeviceWriteFailures)
{
  if (!TestPlatform::supports(TestPlatformCapability::LinuxSpecialFiles)) {
    GTEST_SKIP();
  }
  const TemporaryTestPath path("ctrace-metadata-device-failure");
  path.createDirectory();
  std::filesystem::create_symlink(TestPlatform::writeFailurePath(), path.path() / "metadata");
  const CtfMetadataModel model(CtfTestSupport::testUuid(), CtfTestSupport::legacyTopology(1U));
  EXPECT_THROW(CtfMetadataWriter::write(path.path(), model), std::runtime_error);
}
