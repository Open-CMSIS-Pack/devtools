/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CtraceRunMeta.hpp"
#include "TestPath.hpp"
#include "TestSupport.hpp"
#include "YmlTraceRunConfigReader.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>

namespace {

const CtraceRunSourceMeta* findSource(const CtraceRunMeta& meta, const std::string& type, std::uint32_t source)
{
  for (const auto& candidate : meta.sources()) {
    if (candidate.type == type && candidate.source == source) {
      return &candidate;
    }
  }
  return nullptr;
}

class TemporaryTraceRunFile {
public:
  explicit TemporaryTraceRunFile(const std::string& name) : root_(name), path_(root_.path() / "Board.ctrace-run.yml") {}

  const std::filesystem::path& path() const
  {
    return path_;
  }

private:
  TemporaryTestPath root_;
  std::filesystem::path path_;
};

} // namespace

TEST(CtraceUnitTests, TraceRunReaderConsumesOnlyRelevantFields)
{
  TemporaryTraceRunFile file("ctrace-run-reader-relevant-fields-test");
  writeTestFile(file.path(), R"yml(format-version: ignored
producer-data:
  malformed: [but, irrelevant]
ctrace-run:
  generated-by: ignored
  created-by: ignored
  vendor-extension: { arbitrary: [content] }
  ctrace-setup:
    - pname: core0
      timestamps:
        clock: 400000000
        itm-prescaler: 4
        timestamp-extension: [ignored]
      itm:
        enable: 0x00000006
        privileged: [ignored]
      data:
        - location: ignored
          symbol-file: ignored
          type: [obsolete-and-ignored]
          size: malformed-but-ignored
          symbol-type: signed int
          symbol-size: 1
          data-extension: { ignored: true }
      exceptions: [ignored]
      pcsampling: { ignored: true }
      synchronization: malformed-but-ignored
    - unknown-setup: [ignored]
  ctrace-refs:
    - ctrace-ref: core0/itm
      pname: core0
      type: itm
      stream: 2
      source: 1
      label: Console
      regs: [ignored]
    - ctrace-ref: core0/data#0
      pname: core0
      type: dwt
      stream: 2
      source: 0
      symbol-address: 0x20000100
      label: Current
      reference-extension: [ignored]
    - { ctrace-ref: ignored/etm, type: etm, source: [invalid] }
    - { ctrace-ref: ignored/exception, type: exception, source: [invalid] }
    - { ctrace-ref: ignored/missing-type, source: [invalid] }
    - malformed-entry-is-ignored
)yml");

  const auto config = YmlTraceRunConfigReader().read(file.path().string());
  ASSERT_EQ(config.references.size(), 2U);
  ASSERT_EQ(config.setups.size(), 1U);

  const auto meta = CtraceRunMeta::fromConfig(config);
  EXPECT_EQ(meta.processorCount(), 1U);
  EXPECT_EQ(meta.timestampClockHz(), std::optional<std::uint64_t>(400000000U));
  EXPECT_EQ(meta.timestampPrescaler(), std::optional<std::uint32_t>(4U));
  ASSERT_EQ(meta.itmEnableMasksByTraceBusId().at(2U), 0x00000006U);

  const auto* itm = findSource(meta, "itm", 1U);
  ASSERT_NE(itm, nullptr);
  EXPECT_EQ(itm->label, std::optional<std::string>("Console"));

  const auto* dwt = findSource(meta, "dwt", 0U);
  ASSERT_NE(dwt, nullptr);
  EXPECT_EQ(dwt->valueType, "signed int");
  EXPECT_EQ(dwt->valueSize, 1U);
  EXPECT_EQ(dwt->symbolAddress, std::optional<std::uint64_t>(0x20000100U));
  EXPECT_EQ(dwt->label, std::optional<std::string>("Current"));

  writeTestFile(file.path(), R"yml(unrelated-root: [ignored]
ctrace-run:
  generated-by: ignored
  unsupported-content: { malformed: [but, irrelevant] }
)yml");
  const auto empty = YmlTraceRunConfigReader().read(file.path().string());
  EXPECT_TRUE(empty.references.empty());
  EXPECT_TRUE(empty.setups.empty());
}

TEST(CtraceUnitTests, TraceRunReaderValidatesConsumedFields)
{
  TemporaryTraceRunFile file("ctrace-run-reader-consumed-fields-test");
  writeTestFile(file.path(), R"yml(ctrace-run:
  ctrace-refs:
    - ctrace-ref: relevant/itm
      type: itm
      source: [1]
)yml");

  EXPECT_THROW((void)YmlTraceRunConfigReader().read(file.path().string()), std::runtime_error);
}
