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
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

/** @brief Counts source metadata directly from canonical normalized routes. */
static std::size_t sourceCount(const CtraceRunMeta& meta)
{
  std::size_t count = 0U;
  for (const auto& route : meta.routes()) {
    count += route.sources.size();
  }
  return count;
}

/** @brief Returns one source in deterministic route/source order. */
static const CtraceRunSourceMeta& sourceAt(const CtraceRunMeta& meta, std::size_t index)
{
  for (const auto& route : meta.routes()) {
    if (index < route.sources.size()) {
      return route.sources[index];
    }
    index -= route.sources.size();
  }
  throw std::out_of_range("source index exceeds normalized route sources");
}

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

TEST(CtraceUnitTests, TraceRunReaderParsesConsumedFields)
{
  TraceRunFixture file("ctrace-run-reader-consumed-fields-test");
  const auto config = file.read(R"yml(ctrace-run:
  trace-format: formatted
  ctrace-setup:
    - pname: core0
      timestamps:
        clock: 400000000
        itm-prescaler: 4
      itm:
        enable: 0x00000006
      data:
        - size: 4
  ctrace-refs:
    - ctrace-ref: core0/itm
      pname: core0
      type: itm
      stream: 2
      source: 1
      label: Console
    - ctrace-ref: core0/data#0
      pname: core0
      type: dwt
      stream: 2
      source: 0
      address: 0x20000100
      data-type: signed
      size: 1
      label: Current
)yml");
  ASSERT_EQ(config.references.size(), 2U);
  ASSERT_EQ(config.setups.size(), 1U);

  const auto meta = CtraceRunMeta::fromConfig(config);
  ASSERT_EQ(meta.routes().size(), 1U);
  EXPECT_EQ(meta.routes().front().timestampClockHz, std::optional<std::uint64_t>(400000000U));
  EXPECT_EQ(meta.routes().front().timestampPrescaler, 4U);
  ASSERT_EQ(meta.routes().front().itmEnableMask, 0x00000006U);

  ASSERT_EQ(sourceCount(meta), 2U);
  const auto& itm = sourceAt(meta, 0);
  EXPECT_EQ(itm.type, "itm");
  EXPECT_EQ(itm.route.traceBusId, 2U);
  EXPECT_EQ(itm.source, 1U);
  EXPECT_EQ(itm.label, std::optional<std::string>("Console"));

  const auto& dwt = sourceAt(meta, 1);
  EXPECT_EQ(dwt.type, "dwt");
  EXPECT_EQ(dwt.route.traceBusId, 2U);
  EXPECT_EQ(dwt.source, 0U);
  EXPECT_EQ(dwt.dataType, "signed");
  EXPECT_EQ(dwt.dataSize, 1U);
  EXPECT_EQ(dwt.address, std::optional<std::uint64_t>(0x20000100U));
  EXPECT_EQ(dwt.label, std::optional<std::string>("Current"));
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
    - ctrace-ref: ignored/unsupported
      type: unsupported
)yml");
  ASSERT_EQ(config.references.size(), 3U);
  EXPECT_TRUE(config.references[0].sources == std::vector<std::uint32_t>({1U, 2U}));
  EXPECT_TRUE(config.references[1].sources == std::vector<std::uint32_t>({3U}));
  EXPECT_TRUE(config.references[2].sources == std::vector<std::uint32_t>({4U, 5U}));
}

TEST(CtraceUnitTests, TraceRunReaderParsesTraceFormatDeclaration)
{
  TraceRunFixture file("ctrace-run-reader-trace-format-test");
  const auto absent = file.read("ctrace-run:\n  ctrace-refs: []\n");
  EXPECT_FALSE(absent.traceFormat.has_value());
  EXPECT_EQ(TraceRunSchema::effectiveTraceFormat(absent.traceFormat), TraceRunFormat::Unformatted);

  const auto nullValue = file.read("ctrace-run:\n  trace-format: null\n  ctrace-refs: []\n");
  EXPECT_FALSE(nullValue.traceFormat.has_value());
  EXPECT_EQ(TraceRunSchema::effectiveTraceFormat(nullValue.traceFormat), TraceRunFormat::Unformatted);

  const auto unformatted = file.read("ctrace-run:\n  trace-format: unformatted\n  ctrace-refs: []\n");
  EXPECT_EQ(unformatted.traceFormat, std::optional<TraceRunFormat>(TraceRunFormat::Unformatted));

  const auto formatted = file.read("ctrace-run:\n  trace-format: formatted\n  ctrace-refs: []\n");
  EXPECT_EQ(formatted.traceFormat, std::optional<TraceRunFormat>(TraceRunFormat::Formatted));
}

TEST(CtraceUnitTests, TraceRunReaderRejectsInvalidTraceFormatDeclaration)
{
  TraceRunFixture file("ctrace-run-reader-trace-format-errors-test");
  expectReadError(file, "ctrace-run:\n  trace-format: unknown\n  ctrace-refs: []\n",
                  "'trace-format' must be 'unformatted' or 'formatted'");
  expectReadError(file, "ctrace-run:\n  trace-format: ''\n  ctrace-refs: []\n",
                  "'trace-format' must be 'unformatted' or 'formatted'");
  expectReadError(file, "ctrace-run:\n  trace-format: []\n  ctrace-refs: []\n",
                  "'trace-format' must be a scalar value");
  expectReadError(file, "ctrace-run:\n  trace-format: {}\n  ctrace-refs: []\n",
                  "'trace-format' must be a scalar value");
}

TEST(CtraceUnitTests, TraceRunReaderIgnoresUnspecifiedTraceFramingField)
{
  TraceRunFixture file("ctrace-run-reader-ignored-trace-framing-test");
  const auto config = file.read(R"yml(ctrace-run:
  trace-format: formatted
  trace-framing:
    unsupported: [fsync, hsync]
  ctrace-refs: []
)yml");

  EXPECT_EQ(config.traceFormat, std::optional<TraceRunFormat>(TraceRunFormat::Formatted));
}

TEST(CtraceUnitTests, TraceRunReaderIgnoresCopiedItmAtbidForRouting)
{
  TraceRunFixture file("ctrace-run-reader-itm-atbid-test");
  const auto config = file.read(R"yml(ctrace-run:
  trace-format: formatted
  ctrace-setup:
    - pname: core
      itm: { enable: 3, atbid: [127] }
  ctrace-refs:
    - { type: itm, ctrace-ref: core/itm, pname: core, stream: 1 }
)yml");

  ASSERT_EQ(config.setups.size(), 1U);
  ASSERT_TRUE(config.setups.front().itm.has_value());
  EXPECT_EQ(config.setups.front().itm->enableMask, 3U);
  const auto meta = CtraceRunMeta::fromConfig(config);
  ASSERT_EQ(meta.routes().size(), 1U);
  EXPECT_EQ(meta.routes().front().identity.traceBusId, std::optional<std::uint8_t>(1U));
}

TEST(CtraceUnitTests, TraceRunReaderUsesReferencedSetupSizeAsFallback)
{
  TraceRunFixture file("ctrace-run-reader-setup-size-test");
  const auto config = file.read(R"yml(ctrace-run:
  ctrace-setup:
    - data:
        - size: 2
  ctrace-refs:
    - ctrace-ref: data#0
      type: dwt
      source: 0
      data-type: signed
)yml");

  const auto meta = CtraceRunMeta::fromConfig(config);
  ASSERT_EQ(sourceCount(meta), 1U);
  EXPECT_FALSE(sourceAt(meta, 0).address.has_value());
  EXPECT_EQ(sourceAt(meta, 0).dataType, "signed");
  EXPECT_EQ(sourceAt(meta, 0).dataSize, 2U);
}

TEST(CtraceUnitTests, TraceRunReaderIgnoresUnsupportedMetadataNames)
{
  TraceRunFixture file("ctrace-run-reader-unsupported-data-metadata-test");
  const auto config = file.read(R"yml(ctrace-run:
  ctrace-setup:
    - data:
        - symbol-type: signed int
          symbol-size: 1
  ctrace-refs:
    - ctrace-ref: data#0
      type: dwt
      source: 0
      symbol-address: 0x20000100
)yml");

  const auto meta = CtraceRunMeta::fromConfig(config);
  ASSERT_EQ(sourceCount(meta), 1U);
  EXPECT_FALSE(sourceAt(meta, 0).address.has_value());
  EXPECT_EQ(sourceAt(meta, 0).dataType, "unsigned");
  EXPECT_EQ(sourceAt(meta, 0).dataSize, 4U);
}

TEST(CtraceUnitTests, TraceRunReaderDefersMalformedDwtMetadataToOutputPlanning)
{
  TraceRunFixture file("ctrace-run-reader-malformed-data-metadata-test");
  const auto config = file.read(R"yml(ctrace-run:
  ctrace-refs:
    - ctrace-ref: data#0
      type: dwt
      source: 0
      address: []
      data-type: []
      size: []
)yml");

  ASSERT_EQ(config.references.size(), 1U);
  EXPECT_TRUE(config.references[0].addressError.has_value());
  EXPECT_TRUE(config.references[0].dataTypeError.has_value());
  EXPECT_TRUE(config.references[0].dataSizeError.has_value());
}

TEST(CtraceUnitTests, TraceRunReaderAcceptsProcessorItmReferenceWithoutEnabledChannels)
{
  TraceRunFixture file("ctrace-run-reader-empty-itm-reference-test");
  const auto config = file.read(R"yml(ctrace-run:
  trace-format: formatted
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
  EXPECT_TRUE(sourceCount(meta) == 0U);
  ASSERT_EQ(meta.routes().size(), 1U);
  EXPECT_EQ(meta.routes().front().itmEnableMask, 0U);
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
  expectReadError(file, "{}\n", "missing top-level 'ctrace-run' node");
  expectReadError(file, "ctrace-run: {}\nctrace-run: {}\n", "map keys must be unique");
  expectReadError(file, "ctrace-run: []\n", "top-level 'ctrace-run' node must be a map");
}

TEST(CtraceUnitTests, TraceRunReaderRejectsMalformedReferenceContainers)
{
  TraceRunFixture file("ctrace-run-reader-reference-container-errors-test");
  expectReadError(file, "ctrace-run: {}\n", "missing required 'ctrace-refs' array");
  expectReadError(file, "ctrace-run:\n  ctrace-refs: null\n", "'ctrace-refs' must be an array");
  expectReadError(file, "ctrace-run:\n  ctrace-refs: {}\n", "'ctrace-refs' must be an array");
  expectReadError(file, "ctrace-run:\n  ctrace-refs: []\n  ctrace-refs: []\n", "map keys must be unique");
  expectReadError(file, "ctrace-run:\n  ctrace-refs: [invalid]\n", "each 'ctrace-refs' entry must be a map");
  expectReadError(file, "ctrace-run:\n  ctrace-refs:\n    - { ctrace-ref: core/itm, source: 1 }\n",
                  "missing required 'type' scalar");
  expectReadError(file, "ctrace-run:\n  ctrace-refs:\n    - { type: [], ctrace-ref: core/itm, source: 1 }\n",
                  "missing required 'type' scalar");
  expectReadError(file, "ctrace-run:\n  ctrace-refs:\n    - { type: null, ctrace-ref: core/itm, source: 1 }\n",
                  "missing required 'type' scalar");
  expectReadError(file, "ctrace-run:\n  ctrace-refs:\n    - { type: '', ctrace-ref: core/itm, source: 1 }\n",
                  "missing required 'type' scalar");
  expectReadError(file, "ctrace-run:\n  ctrace-refs:\n    - type: itm\n      source: 1\n",
                  "missing required 'ctrace-ref' scalar");
  expectReadError(file, "ctrace-run:\n  ctrace-refs:\n    - { type: itm, ctrace-ref: [], source: 1 }\n",
                  "missing required 'ctrace-ref' scalar");
  expectReadError(file, "ctrace-run:\n  ctrace-refs:\n    - { type: itm, ctrace-ref: null, source: 1 }\n",
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
      {"source: 0x", "'source' must be an unsigned integer"},
      {"source: -1", "'source' must be an unsigned integer in range"},
      {"source: 4294967296", "'source' must be an unsigned integer in range"},
      {"source: [1, {}]", "each 'source' entry must be an unsigned integer"},
      {"source: [1, '']", "each 'source' entry must be an unsigned integer"},
      {"source: 1\n      source: 2", "map keys must be unique"},
      {"source: 1\n      info: {}", "'info' must be a string or list of strings"},
      {"source: 1\n      warning: [valid, {}]", "each 'warning' entry must be a string"},
      {"source: 1\n      error: [valid, []]", "each 'error' entry must be a string"},
  };
  for (const auto& testCase : cases) {
    const auto yaml = std::string("ctrace-run:\n  ctrace-refs:\n    - type: itm\n      ctrace-ref: core/itm\n      ") +
                      testCase.fields + "\n";
    expectReadError(file, yaml, testCase.error);
  }

  expectReadError(file,
                  "ctrace-run:\n  ctrace-refs:\n    - { type: exception, ctrace-ref: core/exceptions, stream: [], "
                  "error: producer-error }\n",
                  "'stream' must be a scalar unsigned integer");
}

TEST(CtraceUnitTests, TraceRunReaderIgnoresNullAndNonScalarOptionalReferenceValues)
{
  TraceRunFixture file("ctrace-run-reader-null-optional-reference-test");
  const auto config = file.read(R"yml(ctrace-run:
  ctrace-setup: null
  ctrace-refs:
    - null
    - type: dwt
      ctrace-ref: data#0
      pname: null
      stream: null
      source: null
      address: null
      data-type: null
      size: null
      label: []
      info: null
      warning: null
      error: null
    - type: itm
      ctrace-ref: itm
      source: [1, null]
      label: null
      info: [note, null]
      warning: [null]
      error: [null]
)yml");

  EXPECT_TRUE(config.setups.empty());
  ASSERT_EQ(config.references.size(), 2U);
  const auto& emptyRoute = config.references[0];
  EXPECT_FALSE(emptyRoute.processorName.has_value());
  EXPECT_FALSE(emptyRoute.stream.has_value());
  EXPECT_TRUE(emptyRoute.sources.empty());
  EXPECT_FALSE(emptyRoute.address.has_value());
  EXPECT_FALSE(emptyRoute.addressError.has_value());
  EXPECT_FALSE(emptyRoute.dataType.has_value());
  EXPECT_FALSE(emptyRoute.dataTypeError.has_value());
  EXPECT_FALSE(emptyRoute.dataSize.has_value());
  EXPECT_FALSE(emptyRoute.dataSizeError.has_value());
  EXPECT_FALSE(emptyRoute.label.has_value());
  EXPECT_TRUE(emptyRoute.info.empty());
  EXPECT_TRUE(emptyRoute.warning.empty());
  EXPECT_TRUE(emptyRoute.error.empty());

  EXPECT_EQ(config.references[1].sources, (std::vector<std::uint32_t>{1U}));
  EXPECT_FALSE(config.references[1].label.has_value());
  EXPECT_EQ(config.references[1].info, (std::vector<std::string>{"note"}));
  EXPECT_TRUE(config.references[1].warning.empty());
  EXPECT_TRUE(config.references[1].error.empty());
}

TEST(CtraceUnitTests, TraceRunReaderPreservesDiagnosticReferences)
{
  TraceRunFixture file("ctrace-run-reader-diagnostic-references-test");
  const auto config = file.read(R"yml(ctrace-run:
  ctrace-refs:
    - { type: event, ctrace-ref: core/events#0, pname: core0, stream: 1, info: [note, detail], warning: [] }
    - { type: pmu, ctrace-ref: core/events#1, pname: core1, stream: 2, warning: [warning] }
    - { type: pcsample, ctrace-ref: core/pcsampling, pname: null, stream: 3, error: unavailable }
    - { type: dwt, ctrace-ref: core/data#, source: 0, address: invalid, error: [diagnostic, detail] }
    - { type: dwt, ctrace-ref: core/notdata#2, error: null }
    - { type: itm, ctrace-ref: core/itm0, source: 0, error: disabled }
    - { type: itm, ctrace-ref: core/itm1, source: 1, error: usable, label: null }
    - { type: exception, ctrace-ref: core/exceptions, pname: core0, stream: 4, error: exception-error }
    - { type: global_ts, ctrace-ref: core/timesync, pname: core0, stream: 5, warning: global-warning }
    - { type: overflow, ctrace-ref: core/overflow, pname: core0, stream: 6, info: overflow-info }
)yml");
  ASSERT_EQ(config.references.size(), 10U);
  EXPECT_EQ(config.references[0].processorName, std::optional<std::string>("core0"));
  EXPECT_EQ(config.references[0].stream, std::optional<std::uint32_t>(1U));
  EXPECT_EQ(config.references[0].info, (std::vector<std::string>{"note", "detail"}));
  EXPECT_TRUE(config.references[0].warning.empty());
  EXPECT_EQ(config.references[1].processorName, std::optional<std::string>("core1"));
  EXPECT_EQ(config.references[1].stream, std::optional<std::uint32_t>(2U));
  EXPECT_EQ(config.references[1].warning, (std::vector<std::string>{"warning"}));
  EXPECT_FALSE(config.references[2].processorName.has_value());
  EXPECT_EQ(config.references[2].stream, std::optional<std::uint32_t>(3U));
  EXPECT_EQ(config.references[2].error, (std::vector<std::string>{"unavailable"}));
  EXPECT_EQ(config.references[3].error, (std::vector<std::string>{"diagnostic", "detail"}));
  EXPECT_FALSE(config.references[3].address.has_value());
  EXPECT_TRUE(config.references[3].addressError.has_value());
  EXPECT_FALSE(config.references[3].dataSetupIndex.has_value());
  EXPECT_TRUE(config.references[4].error.empty());
  EXPECT_TRUE(config.references[5].sources == std::vector<std::uint32_t>{0U});
  EXPECT_FALSE(config.references[6].label.has_value());
  EXPECT_EQ(config.references[7].error, (std::vector<std::string>{"exception-error"}));
  EXPECT_EQ(config.references[7].stream, std::optional<std::uint32_t>(4U));
  EXPECT_EQ(config.references[8].warning, (std::vector<std::string>{"global-warning"}));
  EXPECT_EQ(config.references[8].stream, std::optional<std::uint32_t>(5U));
  EXPECT_EQ(config.references[9].info, (std::vector<std::string>{"overflow-info"}));
  EXPECT_EQ(config.references[9].stream, std::optional<std::uint32_t>(6U));
}

TEST(CtraceUnitTests, TraceRunReaderRetainsSourceLessBindingsWithProducerErrors)
{
  TraceRunFixture file("ctrace-run-reader-source-less-bindings-test");
  const auto config = file.read(R"yml(ctrace-run:
  trace-format: formatted
  ctrace-refs:
    - { type: itm, ctrace-ref: core/itm, pname: core, stream: 7, error: itm-error }
    - { type: itm, ctrace-ref: core/timestamps, pname: core, stream: 7, error: normative-error }
    - { type: dwt, ctrace-ref: core/timestamps, pname: core, stream: 7, error: transitional-error }
    - { type: itm, ctrace-ref: core/itm, pname: core, stream: 7, source: [invalid], error: source-error }
)yml");

  ASSERT_EQ(config.references.size(), 4U);
  for (const auto& reference : config.references) {
    EXPECT_EQ(reference.processorName, std::optional<std::string>("core"));
    EXPECT_EQ(reference.stream, std::optional<std::uint32_t>(7U));
    EXPECT_TRUE(reference.sources.empty());
    ASSERT_EQ(reference.error.size(), 1U);
  }
  const auto meta = CtraceRunMeta::fromConfig(config);
  ASSERT_EQ(meta.routes().size(), 1U);
  EXPECT_TRUE(meta.routes().front().sources.empty());

  auto unformatted = config;
  unformatted.traceFormat.reset();
  unformatted.references = {config.references.back()};
  unformatted.references.front().type = "dwt";
  unformatted.references.front().ctraceRef = "core/data#0";
  unformatted.references.front().dataSetupIndex = 0U;
  const auto singleMeta = CtraceRunMeta::fromConfig(unformatted);
  EXPECT_EQ(singleMeta.routes().front().processorName, std::optional<std::string>("core"));
  EXPECT_TRUE(singleMeta.routes().front().sources.empty());
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
  ctrace-refs: []
)yml");
  ASSERT_EQ(config.setups.size(), 7U);
  EXPECT_FALSE(config.setups[0].timestamps->clockError.has_value());
  EXPECT_EQ(config.setups[1].timestamps->clockError, std::optional<std::string>("'timestamps' must be empty or a map"));
  EXPECT_EQ(config.setups[2].timestamps->clockError, std::optional<std::string>("'timestamps' must be empty or a map"));
  EXPECT_EQ(config.setups[3].timestamps->clockError,
            std::optional<std::string>("'timestamps.clock' must be a scalar unsigned integer"));
  EXPECT_TRUE(config.setups[4].timestamps->clockError.has_value());
  EXPECT_FALSE(config.setups[5].timestamps->clockHz.has_value());
  EXPECT_FALSE(config.setups[5].timestamps->clockError.has_value());
  EXPECT_EQ(config.setups[6].timestamps->clockHz, std::optional<std::uint64_t>(16U));
  EXPECT_EQ(config.setups[6].timestamps->timestampPrescaler, std::optional<std::uint32_t>(4U));

  expectReadError(file, "ctrace-run:\n  ctrace-setup:\n    - timestamps: { clock: 1, clock: 2 }\n",
                  "map keys must be unique");
  expectReadError(file, "ctrace-run:\n  ctrace-setup:\n    - timestamps: {}\n      timestamps: {}\n",
                  "map keys must be unique");
}

TEST(CtraceUnitTests, TraceRunReaderTreatsNullOptionalSetupValuesAsAbsent)
{
  TraceRunFixture file("ctrace-run-reader-null-optional-setup-test");
  const auto config = file.read(R"yml(ctrace-run:
  ctrace-setup:
    - timestamps:
        clock: null
        itm-prescaler: null
      itm: null
  ctrace-refs: []
)yml");

  ASSERT_EQ(config.setups.size(), 1U);
  ASSERT_TRUE(config.setups[0].timestamps.has_value());
  EXPECT_FALSE(config.setups[0].timestamps->clockHz.has_value());
  EXPECT_FALSE(config.setups[0].timestamps->timestampPrescaler.has_value());
  EXPECT_FALSE(config.setups[0].timestamps->clockError.has_value());
  EXPECT_FALSE(config.setups[0].itm.has_value());

  const auto meta = CtraceRunMeta::fromConfig(config);
  EXPECT_FALSE(meta.routes().front().timestampClockHz.has_value());
  EXPECT_EQ(meta.routes().front().timestampPrescaler, TraceRunSchema::kDefaultTimestampPrescaler);

  const auto generatedSetup = file.read(R"yml(ctrace-run:
  ctrace-setup:
    - null
    - pname: itm-null-only
      itm: null
    - pname: null-enable
      timestamps: null
      itm:
        enable: null
        atbid: 1
    - pname: absent-enable
      timestamps: null
      itm:
        atbid: 2
  ctrace-refs: []
)yml");
  ASSERT_EQ(generatedSetup.setups.size(), 2U);
  EXPECT_FALSE(generatedSetup.setups[0].itm.has_value());
  EXPECT_FALSE(generatedSetup.setups[1].itm.has_value());
}

TEST(CtraceUnitTests, TraceRunReaderRejectsMalformedConsumedSetups)
{
  TraceRunFixture file("ctrace-run-reader-setup-errors-test");
  constexpr std::string_view prefix = "ctrace-run:\n  ctrace-refs: []\n  ctrace-setup:\n    - ";
  /** @brief Describes malformed setup fields and their expected errors. */
  struct Case {
    const char* setup;
    const char* error;
  };
  constexpr Case cases[] = {
      {"timestamps: { itm-prescaler: [] }", "'timestamps.itm-prescaler' must be a scalar unsigned integer"},
      {"timestamps: { itm-prescaler: invalid }", "'itm-prescaler' must be an unsigned integer in range"},
      {"itm: { enable: 1, enable: 2 }", "map keys must be unique"},
  };
  for (const auto& testCase : cases) {
    expectReadError(file, std::string(prefix) + testCase.setup + "\n", testCase.error);
  }

  expectReadError(file, "ctrace-run:\n  ctrace-refs: []\n  ctrace-setup: {}\n", "'ctrace-setup' must be an array");
  expectReadError(file, "ctrace-run:\n  ctrace-refs: []\n  ctrace-setup: [invalid]\n",
                  "each 'ctrace-setup' entry must be a map");

  expectReadError(file, R"yml(ctrace-run:
  ctrace-refs:
    - { type: dwt, ctrace-ref: core/data#0, source: 0 }
  ctrace-setup:
    - pname: []
      data: [{}]
)yml",
                  "'pname' must be a scalar string");
}

TEST(CtraceUnitTests, TraceRunReaderDefersMalformedItmSetupMetadata)
{
  TraceRunFixture file("ctrace-run-reader-deferred-itm-errors-test");
  constexpr std::string_view cases[] = {
      "itm: []",
      "itm: { enable: [] }",
      "itm: { enable: '' }",
      "itm: { enable: invalid }",
  };
  for (const auto setup : cases) {
    const auto yaml = std::string(R"yml(ctrace-run:
  trace-format: formatted
  ctrace-refs:
    - { type: itm, ctrace-ref: core/itm, pname: core, stream: 1 }
  ctrace-setup:
    - pname: core
      )yml") + std::string(setup) +
                      "\n";
    const auto config = file.read(yaml);
    ASSERT_EQ(config.setups.size(), 1U) << setup;
    ASSERT_TRUE(config.setups.front().itm.has_value()) << setup;
    EXPECT_FALSE(config.setups.front().itm->enableMask.has_value()) << setup;
    ASSERT_TRUE(config.setups.front().itm->enableError.has_value()) << setup;
    EXPECT_NE(config.setups.front().itm->enableError->find("(7):"), std::string::npos) << setup;

    EXPECT_THROW((void)CtraceRunMeta::fromConfig(config), std::runtime_error) << setup;

    auto unformattedConfig = config;
    unformattedConfig.traceFormat.reset();
    EXPECT_THROW((void)CtraceRunMeta::fromConfig(unformattedConfig), std::runtime_error) << setup;
  }
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
        - {}
        - not-a-map
        - { size: [] }
        - { size: null }
        - { size: invalid }
        - { size: 2 }
        - {}
    - pname: core1
      data: not-an-array
    - pname: core2
      data: [{}]
    - data:
        - { size: null }
    - data: [{}]
)yml");
  ASSERT_EQ(config.setups.size(), 5U);
  ASSERT_EQ(config.setups[0].data.size(), 7U);
  EXPECT_TRUE(config.setups[0].data[0].present);
  EXPECT_TRUE(config.setups[0].data[1].present);
  EXPECT_FALSE(config.setups[0].data[1].size.has_value());
  EXPECT_EQ(config.setups[0].data[1].sizeError, std::optional<std::string>("each 'data' entry must be a map"));
  EXPECT_EQ(config.setups[0].data[2].sizeError,
            std::optional<std::string>("'data.size' must be a scalar unsigned integer"));
  EXPECT_FALSE(config.setups[0].data[3].size.has_value());
  EXPECT_TRUE(config.setups[0].data[4].sizeError.has_value());
  EXPECT_EQ(config.setups[0].data[5].size, std::optional<std::uint64_t>(2U));
  EXPECT_TRUE(config.setups[1].data.empty());
  EXPECT_EQ(config.setups[1].dataError, std::optional<std::string>("'data' must be an array"));
  EXPECT_EQ(config.setups[1].featurePaths, (std::vector<std::string>{"core1/data"}));
  ASSERT_EQ(config.setups[2].data.size(), 1U);
  EXPECT_TRUE(config.setups[2].data[0].present);
  EXPECT_FALSE(config.setups[2].dataError.has_value());
  ASSERT_EQ(config.setups[3].data.size(), 1U);
  EXPECT_TRUE(config.setups[3].data[0].present);
  ASSERT_EQ(config.setups[4].data.size(), 1U);
  EXPECT_TRUE(config.setups[4].data[0].present);

  expectReadError(file, R"yml(ctrace-run:
  ctrace-refs: [{ type: dwt, ctrace-ref: data#0, source: 0 }]
  ctrace-setup:
    - data: [{ size: 1, size: 2 }]
)yml",
                  "map keys must be unique");
}

TEST(CtraceUnitTests, TraceRunReaderDefersMalformedDataContainerWithoutIndexSizedAllocation)
{
  TraceRunFixture file("ctrace-run-reader-deferred-data-container-error-test");
  const auto index = std::to_string(std::numeric_limits<std::size_t>::max());
  const auto config = file.read(std::string(R"yml(ctrace-run:
  ctrace-setup:
    - pname: core
      data: invalid
  ctrace-refs:
    - { type: dwt, ctrace-ref: core/data#)yml") +
                                index + ", pname: core, source: 0 }\n");

  ASSERT_EQ(config.setups.size(), 1U);
  EXPECT_TRUE(config.setups.front().data.empty());
  EXPECT_EQ(config.setups.front().dataError, std::optional<std::string>("'data' must be an array"));
  const auto meta = CtraceRunMeta::fromConfig(config);
  ASSERT_EQ(sourceCount(meta), 1U);
  EXPECT_EQ(sourceAt(meta, 0U).dataSize, TraceRunSchema::kDefaultDwtDataSize);
  EXPECT_EQ(sourceAt(meta, 0U).dataSizeError, std::optional<std::string>("'data' must be an array"));
}

TEST(CtraceUnitTests, TraceRunReaderPropagatesMalformedDataEntryToSourceMetadata)
{
  TraceRunFixture file("ctrace-run-reader-deferred-data-entry-error-test");
  const auto config = file.read(R"yml(ctrace-run:
  trace-format: formatted
  ctrace-setup:
    - pname: core
      data: [invalid]
  ctrace-refs:
    - { type: itm, ctrace-ref: core/itm, pname: core, stream: 1 }
    - { type: dwt, ctrace-ref: core/data#0, pname: core, stream: 1, source: 0 }
)yml");

  ASSERT_EQ(config.setups.size(), 1U);
  ASSERT_EQ(config.setups.front().data.size(), 1U);
  EXPECT_EQ(config.setups.front().data.front().sizeError,
            std::optional<std::string>("each 'data' entry must be a map"));
  const auto meta = CtraceRunMeta::fromConfig(config);
  ASSERT_EQ(sourceCount(meta), 1U);
  EXPECT_EQ(sourceAt(meta, 0U).dataSize, TraceRunSchema::kDefaultDwtDataSize);
  EXPECT_EQ(sourceAt(meta, 0U).dataSizeError, std::optional<std::string>("each 'data' entry must be a map"));
}

TEST(CtraceUnitTests, TraceRunReaderDoesNotCreateSetupMetadataFromNullDataEntries)
{
  TraceRunFixture file("ctrace-run-reader-null-data-entry-test");
  const auto config = file.read(R"yml(ctrace-run:
  trace-format: formatted
  ctrace-setup:
    - pname: core
      data: [null]
  ctrace-refs:
    - { type: dwt, ctrace-ref: core/data#0, pname: core, stream: 1, source: 0 }
)yml");

  EXPECT_TRUE(config.setups.empty());
  const auto meta = CtraceRunMeta::fromConfig(config);
  ASSERT_EQ(sourceCount(meta), 1U);
  EXPECT_EQ(sourceAt(meta, 0U).dataSize, TraceRunSchema::kDefaultDwtDataSize);
  EXPECT_FALSE(sourceAt(meta, 0U).dataSizeError.has_value());

  const auto otherProcessor = file.read(R"yml(ctrace-run:
  ctrace-setup:
    - pname: core
      data: [null]
    - pname: other
      timestamps: null
  ctrace-refs:
    - { type: dwt, ctrace-ref: core/data#0, pname: core, source: 0 }
)yml");
  ASSERT_EQ(otherProcessor.setups.size(), 1U);
  EXPECT_EQ(otherProcessor.setups.front().processorName, std::optional<std::string>("other"));
  const auto otherMeta = CtraceRunMeta::fromConfig(otherProcessor);
  EXPECT_TRUE(sourceCount(otherMeta) == 0U);
  EXPECT_EQ(otherMeta.routes().front().processorName, std::optional<std::string>("other"));

  const auto malformedIndex = file.read(R"yml(ctrace-run:
  ctrace-setup:
    - data: [{ size: 2 }]
  ctrace-refs:
    - { type: dwt, ctrace-ref: data#invalid, source: 0 }
)yml");
  ASSERT_EQ(malformedIndex.references.size(), 1U);
  EXPECT_FALSE(malformedIndex.references.front().dataSetupIndex.has_value());
  EXPECT_TRUE(malformedIndex.setups.empty());

  const auto foreignProcessor = file.read(R"yml(ctrace-run:
  ctrace-setup:
    - pname: core
      data: invalid-but-irrelevant
  ctrace-refs:
    - { type: dwt, ctrace-ref: other/data#0, source: 0 }
)yml");
  ASSERT_EQ(foreignProcessor.setups.size(), 1U);
  EXPECT_FALSE(foreignProcessor.setups.front().dataError.has_value());
  const auto foreignMeta = CtraceRunMeta::fromConfig(foreignProcessor);
  ASSERT_EQ(sourceCount(foreignMeta), 1U);
  EXPECT_EQ(sourceAt(foreignMeta, 0U).processorName, std::optional<std::string>("other"));
  EXPECT_EQ(sourceAt(foreignMeta, 0U).dataSize, TraceRunSchema::kDefaultDwtDataSize);
}

TEST(CtraceUnitTests, TraceRunReaderSkipsEmptyActiveSetupsAndNullDataEntries)
{
  TraceRunFixture file("ctrace-run-reader-empty-active-setup-test");
  const auto config = file.read(R"yml(ctrace-run:
  ctrace-setup:
    - {}
    - pname: unused
    - pname: []
      ignored-node: []
    - pname: unused-event
      events:
    - pname: core
      data: [null, {}, null, { size: 2 }]
  ctrace-refs:
    - { type: dwt, ctrace-ref: core/data#0, pname: core, source: 2 }
    - { type: dwt, ctrace-ref: core/data#1, pname: core, source: 0 }
    - { type: dwt, ctrace-ref: core/data#3, pname: core, source: 1 }
)yml");

  ASSERT_EQ(config.setups.size(), 1U);
  const auto& setup = config.setups.front();
  EXPECT_EQ(setup.ordinal, 4U);
  EXPECT_EQ(setup.processorName, std::optional<std::string>("core"));
  EXPECT_EQ(setup.featurePaths, (std::vector<std::string>{"core/data#1", "core/data#3"}));
  ASSERT_EQ(setup.data.size(), 4U);
  EXPECT_FALSE(setup.data[0].present);
  EXPECT_TRUE(setup.data[1].present);
  EXPECT_FALSE(setup.data[2].present);
  EXPECT_TRUE(setup.data[3].present);
  EXPECT_FALSE(setup.data[1].size.has_value());
  EXPECT_EQ(setup.data[3].size, std::optional<std::uint64_t>(2U));
}

TEST(CtraceUnitTests, TraceRunReaderPreservesDisabledSetupFragments)
{
  TraceRunFixture file("ctrace-run-reader-disabled-setup-test");
  const auto config = file.read(R"yml(ctrace-run:
  ctrace-setup:
    - null
    - pname: core
      disable: false
      timestamps:
        clock: []
      timesync:
      data: [{ size: [] }, null]
      exceptions:
      events: [{}, null, {}]
      itm: invalid
      pcsampling:
      synchronization:
      instructions:
      tracehalt:
    - pname: core
      timestamps:
        clock: 400000000
    - pname: []
      disable: []
      events: []
    - pname: null-disabled
      disable:
  ctrace-refs: []
)yml");

  ASSERT_EQ(config.setups.size(), 4U);
  const auto& disabled = config.setups[0];
  EXPECT_TRUE(disabled.disabled);
  EXPECT_EQ(disabled.ordinal, 1U);
  EXPECT_EQ(disabled.processorName, std::optional<std::string>("core"));
  EXPECT_FALSE(disabled.timestamps.has_value());
  EXPECT_FALSE(disabled.itm.has_value());
  EXPECT_TRUE(disabled.data.empty());
  EXPECT_EQ(disabled.featurePaths, (std::vector<std::string>{
                                       "core/timestamps",
                                       "core/timesync",
                                       "core/data#0",
                                       "core/exceptions",
                                       "core/events#0",
                                       "core/events#2",
                                       "core/itm",
                                       "core/pcsampling",
                                       "core/synchronization",
                                       "core/instructions",
                                       "core/tracehalt",
                                   }));

  EXPECT_FALSE(config.setups[1].disabled);
  EXPECT_EQ(config.setups[1].ordinal, 2U);
  EXPECT_EQ(config.setups[1].featurePaths, (std::vector<std::string>{"core/timestamps"}));
  ASSERT_TRUE(config.setups[1].timestamps.has_value());
  EXPECT_EQ(config.setups[1].timestamps->clockHz, std::optional<std::uint64_t>(400000000U));

  EXPECT_TRUE(config.setups[2].disabled);
  EXPECT_EQ(config.setups[2].ordinal, 3U);
  EXPECT_FALSE(config.setups[2].processorName.has_value());
  EXPECT_TRUE(config.setups[2].featurePaths.empty());

  EXPECT_TRUE(config.setups[3].disabled);
  EXPECT_EQ(config.setups[3].processorName, std::optional<std::string>("null-disabled"));
  EXPECT_TRUE(config.setups[3].featurePaths.empty());
}
