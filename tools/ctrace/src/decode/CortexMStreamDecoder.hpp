/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#pragma once

#include "OpenCsdTraceElement.hpp"
#include "TraceEvent.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>

class CortexMPostDecoder;

struct ItmTimestampPrescalers {
  std::optional<std::uint32_t> fallback;
  std::map<std::uint8_t, std::uint32_t> byTraceBusId;
};

class CortexMStreamDecoder final : public OpenCsdTraceElementSink {
public:
  CortexMStreamDecoder(ItmTimestampPrescalers prescalers, TraceEventSink& eventSink);
  ~CortexMStreamDecoder();

  CortexMStreamDecoder(const CortexMStreamDecoder&) = delete;
  CortexMStreamDecoder& operator=(const CortexMStreamDecoder&) = delete;

  void append(OpenCsdTraceElement element) override;
  void finish();
  std::uint64_t eventCount() const;

private:
  std::uint32_t prescaler(std::uint8_t traceBusId) const;
  CortexMPostDecoder& decoder(std::uint8_t traceBusId);

  ItmTimestampPrescalers prescalers_;
  TraceEventSink& eventSink_;
  std::map<std::uint8_t, std::unique_ptr<CortexMPostDecoder>> decoders_;
};
