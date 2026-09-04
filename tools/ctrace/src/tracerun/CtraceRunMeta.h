/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_TRACERUN_CTRACERUNMETA_H
#define CTRACE_SRC_TRACERUN_CTRACERUNMETA_H

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

struct TraceRunConfig;

/** @brief Stores normalized metadata for one trace source route. */
struct CtraceRunSourceMeta {
  std::string type;
  std::optional<std::string> processorName;
  std::uint8_t traceBusId = 0U;
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

/** @brief Provides validated trace-run metadata consumed by decoding and output. */
class CtraceRunMeta {
public:
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
  /** @brief Returns complete DWT comparator values indexed by Trace Bus ID and comparator. */
  const std::map<std::uint8_t, std::map<std::uint32_t, std::uint32_t>>& dwtComparatorValuesByTraceBusId() const;
  /** @brief Returns clock validation errors retained for output planning. */
  const std::vector<std::string>& timestampClockErrors() const;
  /** @brief Reports whether processor-specific timestamp prescalers differ. */
  bool hasDistinctProcessorPrescalers() const;
  /** @brief Returns the number of processors represented by the configuration. */
  std::size_t processorCount() const;
  /** @brief Returns all normalized source routes. */
  const std::vector<CtraceRunSourceMeta>& sources() const;
  /** @brief Returns non-fatal inconsistencies ignored during normalization. */
  const std::vector<CtraceRunWarning>& warnings() const;

private:
  std::string m_configPath;
  std::optional<std::uint64_t> m_timestampClockHz;
  std::map<std::uint8_t, CtraceRunTimestampMeta> m_timestampsByTraceBusId;
  std::optional<std::uint32_t> m_timestampPrescaler;
  std::map<std::uint8_t, std::uint32_t> m_timestampPrescalersByTraceBusId;
  std::optional<std::uint32_t> m_itmEnableMask;
  std::map<std::uint8_t, std::uint32_t> m_itmEnableMasksByTraceBusId;
  std::map<std::uint8_t, std::map<std::uint32_t, std::uint32_t>> m_dwtComparatorValuesByTraceBusId;
  std::vector<std::string> m_timestampClockErrors;
  std::size_t m_processorCount = 0;
  bool m_distinctProcessorPrescalers = false;
  std::vector<CtraceRunSourceMeta> m_sources;
  std::vector<CtraceRunWarning> m_warnings;
};

#endif // CTRACE_SRC_TRACERUN_CTRACERUNMETA_H
