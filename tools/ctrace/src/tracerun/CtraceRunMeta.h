/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_TRACERUN_CTRACERUNMETA_H
#define CTRACE_SRC_TRACERUN_CTRACERUNMETA_H

#include "TraceRoute.h"

#include <cstddef>
#include <cstdint>
#include <map>
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

/** @brief Stores normalized timestamp metadata for one processor stream. */
struct CtraceRunTimestampMeta {
  std::optional<std::string> processorName;
  std::optional<std::uint64_t> clockHz;
  std::optional<std::string> clockError;
};

/** @brief Describes one non-fatal inconsistency between ctrace-setup and ctrace-refs. */
struct CtraceRunWarning {
  std::string message;
  std::vector<std::pair<std::string, std::string>> context;
};

/** @brief Identifies a protocol carried by one normalized trace route. */
enum class CtraceRunProtocol {
  Itm,
};

/** @brief Retains one producer diagnostic attached to a ctrace reference. */
struct CtraceRunReferenceDiagnostic {
  enum class Severity {
    Info,
    Warning,
    Error,
  };

  Severity severity = Severity::Info;
  std::string message;
  std::string ctraceRef;
  std::optional<std::string> processorName;
  std::optional<std::uint32_t> stream;
  std::size_t line = 0U;
};

/** @brief Describes one normalized protocol route and its processor metadata. */
struct CtraceRunRoute {
  CtraceRunProtocol protocol = CtraceRunProtocol::Itm;
  TraceRouteIdentity identity;
  std::optional<std::string> processorName;
  bool timestampsConfigured = false;
  std::optional<std::uint64_t> timestampClockHz;
  std::optional<std::string> timestampClockError;
  std::uint32_t timestampPrescaler = 1U;
  std::optional<std::uint32_t> itmEnableMask;
  std::optional<std::string> itmEnableError;
  std::vector<CtraceRunSourceMeta> sources;
  std::vector<CtraceRunReferenceDiagnostic> referenceDiagnostics;
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
   * @return Metadata indexed for decoder and output consumption.
   *
   * Ambiguous processor-wide values remain absent while per-stream values and
   * validation errors are retained for diagnostics and output planning.
   */
  static CtraceRunMeta fromConfig(const TraceRunConfig& config);

  /** @brief Returns the source trace-run configuration path. */
  const std::string& configPath() const;
  /** @brief Returns the optional global byte-format declaration used during normalization. */
  const std::optional<TraceRunFormat>& traceFormat() const;
  /** @brief Returns the unambiguous timestamp clock, if available. */
  const std::optional<std::uint64_t>& timestampClockHz() const;
  /** @brief Returns timestamp metadata indexed by Trace Bus ID. */
  const std::map<std::uint8_t, CtraceRunTimestampMeta>& timestampsByTraceBusId() const;
  /** @brief Returns the unambiguous ITM timestamp prescaler, if available. */
  const std::optional<std::uint32_t>& timestampPrescaler() const;
  /** @brief Returns timestamp prescalers indexed by Trace Bus ID. */
  const std::map<std::uint8_t, std::uint32_t>& timestampPrescalersByTraceBusId() const;
  /** @brief Returns the unambiguous ITM stimulus enable mask, if available. */
  const std::optional<std::uint32_t>& itmEnableMask() const;
  /** @brief Returns ITM stimulus enable masks indexed by Trace Bus ID. */
  const std::map<std::uint8_t, std::uint32_t>& itmEnableMasksByTraceBusId() const;
  /** @brief Returns clock validation errors retained for output planning. */
  const std::vector<std::string>& timestampClockErrors() const;
  /** @brief Reports whether processor-specific timestamp prescalers differ. */
  bool hasDistinctProcessorPrescalers() const;
  /** @brief Returns the number of processors represented by the configuration. */
  std::size_t processorCount() const;
  /** @brief Returns all normalized source routes. */
  const std::vector<CtraceRunSourceMeta>& sources() const;
  /** @brief Returns the normalized protocol-route catalogue. */
  const std::vector<CtraceRunRoute>& routes() const;
  /** @brief Returns all producer diagnostics retained from consumed references. */
  const std::vector<CtraceRunReferenceDiagnostic>& referenceDiagnostics() const;
  /** @brief Returns non-fatal inconsistencies ignored during normalization. */
  const std::vector<CtraceRunWarning>& warnings() const;

private:
  /** @brief Restricts construction to normalized instances returned by fromConfig(). */
  CtraceRunMeta() = default;

  std::string m_configPath;
  std::optional<TraceRunFormat> m_traceFormat;
  std::optional<std::uint64_t> m_timestampClockHz;
  std::map<std::uint8_t, CtraceRunTimestampMeta> m_timestampsByTraceBusId;
  std::optional<std::uint32_t> m_timestampPrescaler;
  std::map<std::uint8_t, std::uint32_t> m_timestampPrescalersByTraceBusId;
  std::optional<std::uint32_t> m_itmEnableMask;
  std::map<std::uint8_t, std::uint32_t> m_itmEnableMasksByTraceBusId;
  std::vector<std::string> m_timestampClockErrors;
  std::size_t m_processorCount = 0;
  bool m_distinctProcessorPrescalers = false;
  std::vector<CtraceRunSourceMeta> m_sources;
  std::vector<CtraceRunRoute> m_routes;
  std::vector<CtraceRunReferenceDiagnostic> m_referenceDiagnostics;
  std::vector<CtraceRunWarning> m_warnings;
};

#endif // CTRACE_SRC_TRACERUN_CTRACERUNMETA_H
