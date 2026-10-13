/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CtfTestSupport.h"
#include "TestPath.h"
#include "TestPlatform.h"

#include <gtest/gtest.h>

#include "ctf/CtfSchema.h"
#include "ctf/CtfStreamWriter.h"

#include <algorithm>
#include <cstddef>
#include <stdexcept>

TEST(CtraceUnitTests, testCtfStreamWriterHandlesInactiveAndEmptyStreams)
{
  CtfStreamWriter writer;
  EXPECT_NO_THROW(writer.close());
  EXPECT_NO_THROW(writer.writeRecord(1U, 1U, 1U, 0U, [](CtfStreamWriter::Record&) {}));

  const TemporaryTestPath path("ctrace-empty-stream");
  writer.open(path.path(), CtfStreamClassId{7U}, CtfTestSupport::testUuid());
  EXPECT_NO_THROW(writer.close());
}

TEST(CtraceUnitTests, testCtfStreamWriterValidatesDeclaredPayloadSize)
{
  const TemporaryTestPath path("ctrace-invalid-record-stream");
  CtfStreamWriter writer;
  writer.open(path.path(), CtfStreamClassId{7U}, CtfTestSupport::testUuid());

  EXPECT_THROW(writer.writeRecord(1U, 1U, 1U, 65536U, [](CtfStreamWriter::Record&) {}), std::invalid_argument);
  EXPECT_THROW(writer.writeRecord(1U, 1U, 1U, 1U, [](CtfStreamWriter::Record&) {}), std::logic_error);
  EXPECT_THROW(writer.writeRecord(1U, 1U, 1U, 1U, [](CtfStreamWriter::Record& record) { record.writeU16(0x1234U); }),
               std::logic_error);
  writer.abort();
}

TEST(CtraceUnitTests, testCtfStreamWriterHoldsRegressingTimestamps)
{
  const TemporaryTestPath path("ctrace-monotonic-stream");
  CtfStreamWriter writer;
  const auto traceUuid = CtfTestSupport::testUuid(7U);
  writer.open(path.path(), CtfStreamClassId{7U}, traceUuid, CtfStreamWriter::EventContextLayout::RouteLabeled);
  const auto eventId = CtfSchema::value(CtfSchema::EventId::TraceStatus);
  const auto writePayload = [](CtfStreamWriter::Record& record) {
    record.writeU8(CtfSchema::value(CtfSchema::TraceStatusReason::DecodeError));
    record.writeU32(1U);
  };
  writer.writeRecord(eventId, 100U, 1U, 5U, writePayload);
  writer.writeRecord(eventId, 50U, 1U, 5U, writePayload);
  writer.close();

  const auto records = CtfTestSupport::readCtfRecords(path.path(), CtfStreamWriter::EventContextLayout::RouteLabeled);
  ASSERT_EQ(records.size(), 2U);
  EXPECT_EQ(records[0].timestamp, 100U);
  EXPECT_EQ(records[1].timestamp, 100U);
  EXPECT_EQ(records[0].traceBusId, 1U);
  EXPECT_EQ(records[0].routeLabelId, std::optional<std::uint8_t>{1U});
  EXPECT_EQ(records[1].routeLabelId, std::optional<std::uint8_t>{1U});
  const auto bytes = readTestBinaryFile(path.path());
  EXPECT_TRUE(std::equal(traceUuid.bytes().begin(), traceUuid.bytes().end(), bytes.begin() + 4U));
  EXPECT_EQ(CtfTestSupport::readLe32(bytes, 20U), 7U);
}

TEST(CtraceUnitTests, testCtfStreamWriterEventContextLayoutIsIndependentOfStreamClassId)
{
  const TemporaryTestPath path("ctrace-route-labeled-stream-zero");
  CtfStreamWriter writer;
  writer.open(path.path(), CtfStreamClassId{0U}, CtfTestSupport::testUuid(),
              CtfStreamWriter::EventContextLayout::RouteLabeled);
  writer.writeRecord(CtfSchema::value(CtfSchema::EventId::TraceStatus), 17U, 0U, 5U,
                     [](CtfStreamWriter::Record& record) {
                       record.writeU8(CtfSchema::value(CtfSchema::TraceStatusReason::TraceStart));
                       record.writeU32(0U);
                     });
  writer.close();

  const auto records = CtfTestSupport::readCtfRecords(path.path(), CtfStreamWriter::EventContextLayout::RouteLabeled);
  ASSERT_EQ(records.size(), 1U);
  EXPECT_EQ(records.front().timestamp, 17U);
  EXPECT_EQ(records.front().traceBusId, 0U);
  EXPECT_EQ(records.front().routeLabelId, std::optional<std::uint8_t>{0U});
  EXPECT_EQ(records.front().payload.size(), 5U);
}

TEST(CtraceUnitTests, testCtfStreamWriterReportsDeviceWriteFailures)
{
  if (!TestPlatform::supports(TestPlatformCapability::LinuxSpecialFiles)) {
    GTEST_SKIP();
  }
  CtfStreamWriter writer;
  writer.open(TestPlatform::writeFailurePath(), CtfStreamClassId{7U}, CtfTestSupport::testUuid());
  writer.writeRecord(1U, 1U, 1U, 1U, [](CtfStreamWriter::Record& record) { record.writeU8(1U); });
  EXPECT_THROW(writer.close(), std::runtime_error);
}
