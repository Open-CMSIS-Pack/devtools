/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TestPath.hpp"

#include <gtest/gtest.h>

#include "ctf/CtfStreamWriter.hpp"

#include <cstddef>
#include <cstdint>
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
  EXPECT_THROW(writer.writeRecord(1U, 1U, 1U, 1U,
                                  [](CtfStreamWriter::Record& record) { record.writeU16(0x1234U); }),
               std::logic_error);
  writer.abort();
}

TEST(CtraceUnitTests, testCtfStreamWriterHoldsRegressingTimestamps)
{
  const TemporaryTestPath path("ctrace-monotonic-stream");
  CtfStreamWriter writer;
  writer.open(path.path(), 7U);
  writer.writeRecord(1U, 100U, 1U, 1U, [](CtfStreamWriter::Record& record) { record.writeU8(1U); });
  writer.writeRecord(1U, 50U, 1U, 8U, [](CtfStreamWriter::Record& record) { record.writeU64(2U); });
  EXPECT_NO_THROW(writer.close());
}

#if defined(__linux__)
TEST(CtraceUnitTests, testCtfStreamWriterReportsDeviceWriteFailures)
{
  CtfStreamWriter writer;
  writer.open("/dev/full", 7U);
  writer.writeRecord(1U, 1U, 1U, 1U, [](CtfStreamWriter::Record& record) { record.writeU8(1U); });
  EXPECT_THROW(writer.close(), std::runtime_error);
}
#endif
