/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_DECODE_CORTEXMSTREAMDECODER_H
#define CTRACE_SRC_DECODE_CORTEXMSTREAMDECODER_H

#include "OpenCsdTraceElement.h"
#include "TraceEvent.h"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>

class CortexMPostDecoder;

/** @brief Stores fallback and stream-specific ITM timestamp prescalers. */
struct ItmTimestampPrescalers {
  std::optional<std::uint32_t> fallback;
  std::map<std::uint8_t, std::uint32_t> byTraceBusId;
};

/** @brief Routes OpenCSD elements to per-stream Cortex-M post-decoders. */
class CortexMStreamDecoder final : public OpenCsdTraceElementSink {
public:
  /** @brief Creates a stream router with timestamp scaling configuration. */
  CortexMStreamDecoder(ItmTimestampPrescalers prescalers, TraceEventSink& eventSink);
  /** @brief Destroys all per-stream post-decoders. */
  ~CortexMStreamDecoder();

  /** @brief Disables copying because stream decoders own unique state. */
  CortexMStreamDecoder(const CortexMStreamDecoder&) = delete;
  /** @brief Disables copy assignment because stream decoders own unique state. */
  CortexMStreamDecoder& operator=(const CortexMStreamDecoder&) = delete;

  /** @brief Routes one element to its Trace Bus ID stream. */
  void append(OpenCsdTraceElement element) override;
  /** @brief Flushes all active stream decoders. */
  void finish();
  /** @brief Returns the combined event count of all streams. */
  std::uint64_t eventCount() const;

private:
  /** @brief Resolves the configured timestamp prescaler for one Trace Bus ID. */
  std::uint32_t prescaler(std::uint8_t traceBusId) const;
  /** @brief Returns or lazily creates the post-decoder for one Trace Bus ID. */
  CortexMPostDecoder& decoder(std::uint8_t traceBusId);

  ItmTimestampPrescalers m_prescalers;
  TraceEventSink& m_eventSink;
  std::map<std::uint8_t, std::unique_ptr<CortexMPostDecoder>> m_decoders;
};

#endif  // CTRACE_SRC_DECODE_CORTEXMSTREAMDECODER_H
