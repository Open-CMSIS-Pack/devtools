/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CtraceRunMeta.h"
#include "TestPath.h"
#include "TestSupport.h"
#include "YmlTraceRunConfigReader.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

/** @brief Owns one temporary trace-run file used by YAML reader tests. */
class TraceRunFixture {
public:
  /** @brief Creates a fixture with a process-specific file path. */
  explicit TraceRunFixture(const std::string& name)
    : m_root(name),
      m_path(m_root.path() / "Board.ctrace-run.yml")
  {
  }

  /** @brief Reads the current file content. */
  TraceRunConfig read() const
  {
    return YmlTraceRunConfigReader().read(m_path.string());
  }

  /** @brief Writes and reads the supplied YAML document. */
  TraceRunConfig read(std::string_view yaml)
  {
    write(yaml);
    return read();
  }

  /** @brief Returns the error produced by reading the current file. */
  std::string error() const
  {
    return captureExceptionMessage([this] { (void)read(); }).value_or("");
  }

  /** @brief Writes YAML and returns its reader error. */
  std::string error(std::string_view yaml)
  {
    write(yaml);
    return error();
  }

private:
  /** @brief Writes YAML content to the fixture file. */
  void write(std::string_view yaml)
  {
    writeTestFile(m_path, std::string(yaml));
  }

  TemporaryTestPath m_root;
  std::filesystem::path m_path;
};

/** @brief Requires YAML input to produce expected reader error text. */
static void expectReadError(TraceRunFixture& file, std::string_view yaml, std::string_view expected)
{
  const auto error = file.error(yaml);
  EXPECT_NE(error.find(expected), std::string::npos) << "unexpected result for:\n" << yaml << "\nerror: " << error;
}

TEST(CtraceUnitTests, TraceRunReaderConsumesOnlyRelevantFields)
{
  TraceRunFixture file("ctrace-run-reader-relevant-fields-test");
  const auto config = file.read(R"yml(format-version: ignored
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
  ASSERT_EQ(config.references.size(), 2U);
  ASSERT_EQ(config.setups.size(), 1U);

  const auto meta = CtraceRunMeta::fromConfig(config);
  EXPECT_EQ(meta.processorCount(), 1U);
  EXPECT_EQ(meta.timestampClockHz(), std::optional<std::uint64_t>(400000000U));
  EXPECT_EQ(meta.timestampPrescaler(), std::optional<std::uint32_t>(4U));
  ASSERT_EQ(meta.itmEnableMasksByTraceBusId().at(2U), 0x00000006U);

  ASSERT_EQ(meta.sources().size(), 2U);
  const auto& itm = meta.sources()[0];
  EXPECT_EQ(itm.type, "itm");
  EXPECT_EQ(itm.traceBusId, 2U);
  EXPECT_EQ(itm.source, 1U);
  EXPECT_EQ(itm.label, std::optional<std::string>("Console"));

  const auto& dwt = meta.sources()[1];
  EXPECT_EQ(dwt.type, "dwt");
  EXPECT_EQ(dwt.traceBusId, 2U);
  EXPECT_EQ(dwt.source, 0U);
  EXPECT_EQ(dwt.valueType, "signed int");
  EXPECT_EQ(dwt.valueSize, 1U);
  EXPECT_EQ(dwt.symbolAddress, std::optional<std::uint64_t>(0x20000100U));
  EXPECT_EQ(dwt.label, std::optional<std::string>("Current"));

  const auto empty = file.read(R"yml(unrelated-root: [ignored]
ctrace-run:
  generated-by: ignored
  unsupported-content: { malformed: [but, irrelevant] }
)yml");
  EXPECT_TRUE(empty.references.empty());
  EXPECT_TRUE(empty.setups.empty());
}

TEST(CtraceUnitTests, TraceRunReaderAcceptsScalarAndArraySourceNotation)
{
  TraceRunFixture file("ctrace-run-reader-consumed-fields-test");
  const auto config = file.read(R"yml(ctrace-run:
  ctrace-refs:
    - ctrace-ref: relevant/itm
      type: itm
      source: [1, 2]
    - ctrace-ref: relevant/dwt-scalar
      type: dwt
      source: 3
    - ctrace-ref: relevant/dwt-array
      type: dwt
      source: [4, 5]
)yml");
  ASSERT_EQ(config.references.size(), 3U);
  EXPECT_TRUE(config.references[0].sources == std::vector<std::uint32_t>({1U, 2U}));
  EXPECT_TRUE(config.references[1].sources == std::vector<std::uint32_t>({3U}));
  EXPECT_TRUE(config.references[2].sources == std::vector<std::uint32_t>({4U, 5U}));
}

TEST(CtraceUnitTests, TraceRunReaderAcceptsProcessorItmReferenceWithoutEnabledChannels)
{
  TraceRunFixture file("ctrace-run-reader-empty-itm-reference-test");
  const auto config = file.read(R"yml(ctrace-run:
  ctrace-setup:
    - pname: core0
      itm:
        enable: 0
  ctrace-refs:
    - ctrace-ref: core0/itm
      pname: core0
      type: itm
      stream: 2
)yml");

  ASSERT_EQ(config.references.size(), 1U);
  EXPECT_TRUE(config.references.front().sources.empty());

  const auto meta = CtraceRunMeta::fromConfig(config);
  EXPECT_EQ(meta.processorCount(), 1U);
  EXPECT_TRUE(meta.sources().empty());
  EXPECT_EQ(meta.itmEnableMask(), std::optional<std::uint32_t>(0U));
  ASSERT_EQ(meta.itmEnableMasksByTraceBusId().size(), 1U);
  EXPECT_EQ(meta.itmEnableMasksByTraceBusId().at(2U), 0U);
}

TEST(CtraceUnitTests, TraceRunReaderReportsDocumentErrors)
{
  TraceRunFixture file("ctrace-run-reader-document-errors-test");
  EXPECT_EQ(captureExceptionMessage([] { (void)YmlTraceRunConfigReader().read(""); }),
            std::optional<std::string>("trace-run configuration path is empty"));
  EXPECT_NE(file.error().find("failed to parse trace-run configuration"), std::string::npos);

  expectReadError(file, "ctrace-run: [\n", "failed to parse trace-run configuration");
  expectReadError(file, "ctrace-run: {}\n---\nctrace-run: {}\n", "expected exactly one YAML document");
  expectReadError(file, "[]\n", "expected a YAML map containing 'ctrace-run'");
  expectReadError(file, "unrelated: {}\n", "missing top-level 'ctrace-run' node");
  expectReadError(file, "ctrace-run: {}\nctrace-run: {}\n", "map keys must be unique");
  expectReadError(file, "ctrace-run: []\n", "top-level 'ctrace-run' node must be a map");
}

TEST(CtraceUnitTests, TraceRunReaderRejectsMalformedReferenceContainers)
{
  TraceRunFixture file("ctrace-run-reader-reference-container-errors-test");
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
  TraceRunFixture file("ctrace-run-reader-reference-route-errors-test");
  /** @brief Describes malformed route fields and their expected errors. */
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
  TraceRunFixture file("ctrace-run-reader-diagnostic-references-test");
  const auto config = file.read(R"yml(ctrace-run:
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
  TraceRunFixture file("ctrace-run-reader-timestamp-variants-test");
  const auto config = file.read(R"yml(ctrace-run:
  ctrace-setup:
    - { timestamps: null }
    - { timestamps: invalid }
    - { timestamps: [] }
    - timestamps: { clock: [] }
    - timestamps: { clock: invalid }
    - timestamps: { clock: null }
    - timestamps: { clock: 0X10, itm-prescaler: 0x4 }
)yml");
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
  TraceRunFixture file("ctrace-run-reader-setup-errors-test");
  constexpr std::string_view prefix = "ctrace-run:\n  ctrace-setup:\n    - ";
  /** @brief Describes malformed setup fields and their expected errors. */
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
  TraceRunFixture file("ctrace-run-reader-data-variants-test");
  const auto config = file.read(R"yml(ctrace-run:
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
  TraceRunFixture file("ctrace-run-reader-unconsumed-setups-test");
  EXPECT_TRUE(file.read(R"yml(ctrace-run:
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
)yml")
                  .setups.empty());

  EXPECT_TRUE(file.read("ctrace-run:\n  ctrace-setup: {}\n").setups.empty());
  EXPECT_NE(file.error("ctrace-run:\n  ctrace-setup: []\n  ctrace-setup: []\n").find("map keys must be unique"),
            std::string::npos);
}
