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
#include <vector>

struct TraceRunConfig;

struct CtraceRunSourceMeta {
  std::string type;
  std::optional<std::string> processorName;
  std::uint8_t traceBusId = 0U;
  std::uint32_t source = 0;
  std::optional<std::string> label;
  std::optional<std::uint64_t> symbolAddress;
  std::string valueType = "unsigned int";
  std::uint64_t valueSize = 4U;
  std::optional<std::string> symbolTypeError;
  std::optional<std::string> symbolSizeError;
};

struct CtraceRunTimestampMeta {
  std::optional<std::string> processorName;
  std::optional<std::uint64_t> clockHz;
  std::optional<std::string> clockError;
};

class CtraceRunMeta {
public:
  static CtraceRunMeta fromConfig(const TraceRunConfig& config);

  const std::string& configPath() const;
  const std::optional<std::uint64_t>& timestampClockHz() const;
  const std::map<std::uint8_t, CtraceRunTimestampMeta>& timestampsByTraceBusId() const;
  const std::optional<std::uint32_t>& timestampPrescaler() const;
  const std::map<std::uint8_t, std::uint32_t>& timestampPrescalersByTraceBusId() const;
  const std::optional<std::uint32_t>& itmEnableMask() const;
  const std::map<std::uint8_t, std::uint32_t>& itmEnableMasksByTraceBusId() const;
  const std::vector<std::string>& timestampClockErrors() const;
  bool hasDistinctProcessorPrescalers() const;
  std::size_t processorCount() const;
  const std::vector<CtraceRunSourceMeta>& sources() const;

private:
  std::string configPath_;
  std::optional<std::uint64_t> timestampClockHz_;
  std::map<std::uint8_t, CtraceRunTimestampMeta> timestampsByTraceBusId_;
  std::optional<std::uint32_t> timestampPrescaler_;
  std::map<std::uint8_t, std::uint32_t> timestampPrescalersByTraceBusId_;
  std::optional<std::uint32_t> itmEnableMask_;
  std::map<std::uint8_t, std::uint32_t> itmEnableMasksByTraceBusId_;
  std::vector<std::string> timestampClockErrors_;
  std::size_t processorCount_ = 0;
  bool distinctProcessorPrescalers_ = false;
  std::vector<CtraceRunSourceMeta> sources_;
};

#endif  // CTRACE_SRC_TRACERUN_CTRACERUNMETA_H
