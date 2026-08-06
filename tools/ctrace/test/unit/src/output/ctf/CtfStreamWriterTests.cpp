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

#include <cstddef>
#include <stdexcept>

TEST(CtraceUnitTests, testCtfStreamWriterHandlesInactiveAndEmptyStreams)
{
  CtfStreamWriter writer;
  EXPECT_NO_THROW(writer.close());
  EXPECT_NO_THROW(writer.writeRecord(1U, 1U, 1U, 0U, [](CtfStreamWriter::Record&) {}));

  const TemporaryTestPath path("ctrace-empty-stream");
  writer.open(path.path(), 7U);
  EXPECT_FALSE(writer.uuidString().empty());
  EXPECT_NO_THROW(writer.close());
}

TEST(CtraceUnitTests, testCtfStreamWriterValidatesDeclaredPayloadSize)
{
  const TemporaryTestPath path("ctrace-invalid-record-stream");
  CtfStreamWriter writer;
  writer.open(path.path(), 7U);

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
  writer.open(path.path(), 7U);
  const auto eventId = CtfSchema::value(CtfSchema::EventId::TraceStatus);
  const auto writePayload = [](CtfStreamWriter::Record& record) {
    record.writeU8(CtfSchema::value(CtfSchema::TraceStatusReason::DecodeError));
    record.writeU32(1U);
  };
  writer.writeRecord(eventId, 100U, 1U, 5U, writePayload);
  writer.writeRecord(eventId, 50U, 1U, 5U, writePayload);
  writer.close();

  const auto records = CtfTestSupport::readCtfRecords(path.path());
  ASSERT_EQ(records.size(), 2U);
  EXPECT_EQ(records[0].timestamp, 100U);
  EXPECT_EQ(records[1].timestamp, 100U);
}

TEST(CtraceUnitTests, testCtfStreamWriterReportsDeviceWriteFailures)
{
  if (!TestPlatform::supports(TestPlatformCapability::LinuxSpecialFiles)) {
    GTEST_SKIP();
  }
  CtfStreamWriter writer;
  writer.open(TestPlatform::writeFailurePath(), 7U);
  writer.writeRecord(1U, 1U, 1U, 1U, [](CtfStreamWriter::Record& record) { record.writeU8(1U); });
  EXPECT_THROW(writer.close(), std::runtime_error);
}
