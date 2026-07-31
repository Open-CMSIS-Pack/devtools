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
#include <string_view>
#include <vector>

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

std::string readError(const std::filesystem::path& path)
{
  try {
    (void)YmlTraceRunConfigReader().read(path.string());
  } catch (const std::runtime_error& error) {
    return error.what();
  }
  return {};
}

void expectReadError(TemporaryTraceRunFile& file, std::string_view yaml, std::string_view expected)
{
  writeTestFile(file.path(), std::string(yaml));
  const auto error = readError(file.path());
  EXPECT_NE(error.find(expected), std::string::npos) << "unexpected result for:\n" << yaml << "\nerror: " << error;
}

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

TEST(CtraceUnitTests, TraceRunReaderReportsDocumentErrors)
{
  TemporaryTraceRunFile file("ctrace-run-reader-document-errors-test");
  EXPECT_EQ(readError({}), "trace-run configuration path is empty");
  EXPECT_NE(readError(file.path()).find("failed to parse trace-run configuration"), std::string::npos);

  expectReadError(file, "ctrace-run: [\n", "failed to parse trace-run configuration");
  expectReadError(file, "ctrace-run: {}\n---\nctrace-run: {}\n", "expected exactly one YAML document");
  expectReadError(file, "[]\n", "expected a YAML map containing 'ctrace-run'");
  expectReadError(file, "unrelated: {}\n", "missing top-level 'ctrace-run' node");
  expectReadError(file, "ctrace-run: {}\nctrace-run: {}\n", "map keys must be unique");
  expectReadError(file, "ctrace-run: []\n", "top-level 'ctrace-run' node must be a map");
}

TEST(CtraceUnitTests, TraceRunReaderRejectsMalformedReferenceContainers)
{
  TemporaryTraceRunFile file("ctrace-run-reader-reference-container-errors-test");
  expectReadError(file, "ctrace-run:\n  ctrace-refs: {}\n", "'ctrace-refs' must be an array");
  expectReadError(file, "ctrace-run:\n  ctrace-refs: []\n  ctrace-refs: []\n", "map keys must be unique");
  expectReadError(file, "ctrace-run:\n  ctrace-refs:\n    - type: itm\n      source: 1\n",
                  "missing required 'ctrace-ref' scalar");
  expectReadError(file, "ctrace-run:\n  ctrace-refs:\n    - { type: itm, ctrace-ref: [], source: 1 }\n",
                  "missing required 'ctrace-ref' scalar");
  expectReadError(file, "ctrace-run:\n  ctrace-refs:\n    - { type: itm, ctrace-ref: '', source: 1 }\n",
                  "missing required 'ctrace-ref' scalar");
}

TEST(CtraceUnitTests, TraceRunReaderRejectsMalformedReferenceRoutes)
{
  TemporaryTraceRunFile file("ctrace-run-reader-reference-route-errors-test");
  struct Case {
    const char* fields;
    const char* error;
  };
  constexpr Case cases[] = {
      {"pname: []\n      source: 1", "'pname' must be a scalar string"},
      {"stream: []\n      source: 1", "'stream' must be a scalar unsigned integer"},
      {"source: {}", "'source' must be an array"},
      {"source: null", "'source' must be an unsigned integer"},
      {"source: 0x", "'source' must be an unsigned integer"},
      {"source: -1", "'source' must be an unsigned integer in range"},
      {"source: 4294967296", "'source' must be an unsigned integer in range"},
      {"source: [1, {}]", "each 'source' entry must be an unsigned integer"},
      {"source: [1, '']", "each 'source' entry must be an unsigned integer"},
      {"source: [1, 2]", "ITM 'source' must be a single channel number"},
      {"source: 1\n      source: 2", "map keys must be unique"},
      {"source: 1\n      info: []", "'info' must be a scalar message"},
      {"source: 1\n      warning: {}", "'warning' must be a scalar message"},
      {"source: 1\n      error: []", "'error' must be a scalar message"},
  };
  for (const auto& testCase : cases) {
    const auto yaml = std::string("ctrace-run:\n  ctrace-refs:\n    - type: itm\n      ctrace-ref: core/itm\n      ") +
                      testCase.fields + "\n";
    expectReadError(file, yaml, testCase.error);
  }
}

TEST(CtraceUnitTests, TraceRunReaderPreservesDiagnosticReferences)
{
  TemporaryTraceRunFile file("ctrace-run-reader-diagnostic-references-test");
  writeTestFile(file.path(), R"yml(ctrace-run:
  ctrace-refs:
    - { type: event, ctrace-ref: core/event, pname: core0, source: [invalid], info: note }
    - { type: pmu, ctrace-ref: core/pmu, pname: [], warning: warning }
    - { type: pcsample, ctrace-ref: core/pc, pname: null, error: unavailable }
    - { type: dwt, ctrace-ref: core/data#, source: 0, symbol-address: invalid, error: diagnostic }
    - { type: dwt, ctrace-ref: core/data#x, stream: [], source: 0, error: malformed }
    - { type: dwt, ctrace-ref: core/notdata#2, error: unrouted }
    - { type: itm, ctrace-ref: core/itm0, source: 0, error: disabled }
    - { type: itm, ctrace-ref: core/itm1, source: 1, error: usable, label: null }
    - { type: ignored, ctrace-ref: ignored }
    - { ctrace-ref: ignored/missing-type }
    - { type: [], ctrace-ref: ignored }
    - { type: '', ctrace-ref: ignored }
)yml");

  const auto config = YmlTraceRunConfigReader().read(file.path().string());
  ASSERT_EQ(config.references.size(), 8U);
  EXPECT_EQ(config.references[0].processorName, std::optional<std::string>("core0"));
  EXPECT_FALSE(config.references[1].processorName.has_value());
  EXPECT_FALSE(config.references[2].processorName.has_value());
  EXPECT_FALSE(config.references[3].symbolAddress.has_value());
  EXPECT_FALSE(config.references[3].dataSetupIndex.has_value());
  EXPECT_FALSE(config.references[4].stream.has_value());
  EXPECT_FALSE(config.references[4].dataSetupIndex.has_value());
  EXPECT_TRUE(config.references[6].sources == std::vector<std::uint32_t>{0U});
  EXPECT_EQ(config.references[7].label, std::optional<std::string>(""));
}

TEST(CtraceUnitTests, TraceRunReaderParsesTimestampSetupVariants)
{
  TemporaryTraceRunFile file("ctrace-run-reader-timestamp-variants-test");
  writeTestFile(file.path(), R"yml(ctrace-run:
  ctrace-setup:
    - { timestamps: null }
    - { timestamps: invalid }
    - { timestamps: [] }
    - timestamps: { clock: [] }
    - timestamps: { clock: invalid }
    - timestamps: { clock: null }
    - timestamps: { clock: 0X10, itm-prescaler: 0x4 }
)yml");

  const auto config = YmlTraceRunConfigReader().read(file.path().string());
  ASSERT_EQ(config.setups.size(), 7U);
  EXPECT_FALSE(config.setups[0].timestamps->clockError.has_value());
  EXPECT_EQ(config.setups[1].timestamps->clockError, std::optional<std::string>("'timestamps' must be empty or a map"));
  EXPECT_EQ(config.setups[2].timestamps->clockError, std::optional<std::string>("'timestamps' must be empty or a map"));
  EXPECT_EQ(config.setups[3].timestamps->clockError,
            std::optional<std::string>("'timestamps.clock' must be a scalar unsigned integer"));
  EXPECT_TRUE(config.setups[4].timestamps->clockError.has_value());
  EXPECT_TRUE(config.setups[5].timestamps->clockError.has_value());
  EXPECT_EQ(config.setups[6].timestamps->clockHz, std::optional<std::uint64_t>(16U));
  EXPECT_EQ(config.setups[6].timestamps->timestampPrescaler, std::optional<std::uint32_t>(4U));

  expectReadError(file, "ctrace-run:\n  ctrace-setup:\n    - timestamps: { clock: 1, clock: 2 }\n",
                  "map keys must be unique");
  expectReadError(file, "ctrace-run:\n  ctrace-setup:\n    - timestamps: {}\n      timestamps: {}\n",
                  "map keys must be unique");
}

TEST(CtraceUnitTests, TraceRunReaderRejectsMalformedConsumedSetups)
{
  TemporaryTraceRunFile file("ctrace-run-reader-setup-errors-test");
  constexpr std::string_view prefix = "ctrace-run:\n  ctrace-setup:\n    - ";
  struct Case {
    const char* setup;
    const char* error;
  };
  constexpr Case cases[] = {
      {"timestamps: { itm-prescaler: [] }", "'timestamps.itm-prescaler' must be a scalar unsigned integer"},
      {"timestamps: { itm-prescaler: invalid }", "'itm-prescaler' must be an unsigned integer in range"},
      {"itm: []", "'itm' must be a map containing 'enable'"},
      {"itm: {}", "'itm.enable' is required"},
      {"itm: { enable: [] }", "'itm.enable' is required"},
      {"itm: { enable: '' }", "'itm.enable' is required"},
      {"itm: { enable: invalid }", "'itm.enable' must be an unsigned integer in range"},
      {"itm: { enable: 1, enable: 2 }", "map keys must be unique"},
  };
  for (const auto& testCase : cases) {
    expectReadError(file, std::string(prefix) + testCase.setup + "\n", testCase.error);
  }

  expectReadError(file, R"yml(ctrace-run:
  ctrace-refs:
    - { type: dwt, ctrace-ref: core/data#0, source: 0 }
  ctrace-setup:
    - pname: []
      data: [{}]
)yml",
                  "'pname' must be a scalar string");
}

TEST(CtraceUnitTests, TraceRunReaderParsesReferencedDataVariants)
{
  TemporaryTraceRunFile file("ctrace-run-reader-data-variants-test");
  writeTestFile(file.path(), R"yml(ctrace-run:
  ctrace-refs:
    - { type: dwt, ctrace-ref: core0/data#1, pname: core0, source: 0 }
    - { type: dwt, ctrace-ref: core0/data#2, pname: core0, source: 1 }
    - { type: dwt, ctrace-ref: core0/data#3, pname: core0, source: 2 }
    - { type: dwt, ctrace-ref: core0/data#4, pname: core0, source: 3 }
    - { type: dwt, ctrace-ref: core0/data#5, pname: core0, source: 4 }
    - { type: dwt, ctrace-ref: core1/data#0, pname: core1, source: 0 }
    - { type: dwt, ctrace-ref: core2/data#9, pname: core2, source: 0 }
    - { type: dwt, ctrace-ref: data#0, source: 6 }
    - { type: dwt, ctrace-ref: data#9, source: 7 }
  ctrace-setup:
    - pname: core0
      data:
        - ignored
        - not-a-map
        - { symbol-type: [] }
        - { symbol-size: [] }
        - { symbol-type: null, symbol-size: null }
        - { symbol-size: invalid }
    - pname: core1
      data: not-an-array
    - pname: core2
      data: [{}]
    - data:
        - { symbol-type: null, symbol-size: null }
    - data: [{}]
)yml");

  const auto config = YmlTraceRunConfigReader().read(file.path().string());
  ASSERT_EQ(config.setups.size(), 4U);
  ASSERT_EQ(config.setups[0].data.size(), 6U);
  EXPECT_FALSE(config.setups[0].data[1].symbolType.has_value());
  EXPECT_EQ(config.setups[0].data[2].symbolTypeError,
            std::optional<std::string>("'data.symbol-type' must be a scalar string"));
  EXPECT_EQ(config.setups[0].data[3].symbolSizeError,
            std::optional<std::string>("'data.symbol-size' must be a scalar unsigned integer"));
  EXPECT_FALSE(config.setups[0].data[4].symbolType.has_value());
  EXPECT_FALSE(config.setups[0].data[4].symbolSize.has_value());
  EXPECT_TRUE(config.setups[0].data[5].symbolSizeError.has_value());
  EXPECT_TRUE(config.setups[2].data[0].symbolType == std::nullopt);
  EXPECT_TRUE(config.setups[2].data[0].symbolSize == std::nullopt);

  expectReadError(file, R"yml(ctrace-run:
  ctrace-refs: [{ type: dwt, ctrace-ref: data#0, source: 0 }]
  ctrace-setup:
    - data: [{ symbol-type: one, symbol-type: two }]
)yml",
                  "map keys must be unique");
}

TEST(CtraceUnitTests, TraceRunReaderIgnoresUnconsumedSetups)
{
  TemporaryTraceRunFile file("ctrace-run-reader-unconsumed-setups-test");
  writeTestFile(file.path(), R"yml(ctrace-run:
  ctrace-refs:
    - { type: dwt, ctrace-ref: core0/data#4, pname: core0, source: 0 }
  ctrace-setup:
    - ignored-scalar
    - { unrelated: true }
    - { pname: other, data: [{}] }
    - { pname: core0, data: [{}] }
    - { pname: null, data: [{}], disable: false }
    - { timestamps: {}, disable: true }
    - { itm: { enable: 1 }, disable: true }
)yml");
  EXPECT_TRUE(YmlTraceRunConfigReader().read(file.path().string()).setups.empty());

  writeTestFile(file.path(), "ctrace-run:\n  ctrace-setup: {}\n");
  EXPECT_TRUE(YmlTraceRunConfigReader().read(file.path().string()).setups.empty());
  writeTestFile(file.path(), "ctrace-run:\n  ctrace-setup: []\n  ctrace-setup: []\n");
  EXPECT_NE(readError(file.path()).find("map keys must be unique"), std::string::npos);
}
