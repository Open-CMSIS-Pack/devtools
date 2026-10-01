/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_TRACERUN_CTRACERUNMETA_H
#define CTRACE_SRC_TRACERUN_CTRACERUNMETA_H

#include "TraceRoute.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

struct TraceRunConfig;
enum class TraceRunFormat;

/** @brief Stores normalized metadata for one trace source route. */
struct CtraceRunSourceMeta {
  std::string type;
  std::optional<std::string> processorName;
  TraceRouteIdentity route;
  std::uint32_t source = 0;
  std::optional<std::string> label;
  std::optional<std::uint64_t> address;
  std::string dataType = "unsigned";
  std::uint64_t dataSize = 4U;
  std::optional<std::string> addressError;
  std::optional<std::string> dataTypeError;
  std::optional<std::string> dataSizeError;
};

/** @brief Describes one non-fatal inconsistency between ctrace-setup and ctrace-refs. */
struct CtraceRunWarning {
  std::string message;
  std::vector<std::pair<std::string, std::string>> context;
};

/** @brief Describes one normalized ITM route and its processor metadata. */
struct CtraceRunRoute {
  TraceRouteIdentity identity;
  std::optional<std::string> processorName;
  std::optional<std::uint64_t> timestampClockHz;
  std::optional<std::string> timestampClockError;
  std::uint32_t timestampPrescaler = 1U;
  std::optional<std::uint32_t> itmEnableMask;
  std::vector<CtraceRunSourceMeta> sources;
};

/** @brief Provides validated trace-run metadata consumed by decoding and output. */
class CtraceRunMeta {
public:
  /** @brief Copies normalized trace-run metadata. */
  CtraceRunMeta(const CtraceRunMeta&) = default;
  /** @brief Moves normalized trace-run metadata. */
  CtraceRunMeta(CtraceRunMeta&&) = default;
  /** @brief Copies normalized trace-run metadata. */
  CtraceRunMeta& operator=(const CtraceRunMeta&) = default;
  /** @brief Moves normalized trace-run metadata. */
  CtraceRunMeta& operator=(CtraceRunMeta&&) = default;

  /**
   * @brief Normalizes a parsed trace-run configuration.
   * @param config Parsed trace-run configuration.
   * @return Metadata containing the canonical decoder and output routes.
   */
  static CtraceRunMeta fromConfig(const TraceRunConfig& config);

  /** @brief Returns the source trace-run configuration path. */
  const std::string& configPath() const;
  /** @brief Returns the global byte format used during normalization, resolved by discovery when available. */
  const std::optional<TraceRunFormat>& traceFormat() const;
  /** @brief Returns the normalized protocol-route catalogue. */
  const std::vector<CtraceRunRoute>& routes() const;
  /** @brief Returns non-fatal inconsistencies ignored during normalization. */
  const std::vector<CtraceRunWarning>& warnings() const;

private:
  /** @brief Restricts construction to normalized instances returned by fromConfig(). */
  CtraceRunMeta() = default;

  std::string m_configPath;
  std::optional<TraceRunFormat> m_traceFormat;
  std::vector<CtraceRunRoute> m_routes;
  std::vector<CtraceRunWarning> m_warnings;
};

#endif // CTRACE_SRC_TRACERUN_CTRACERUNMETA_H
