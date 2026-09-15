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
#include "TraceRoute.h"

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

class CortexMPostDecoder;

/** @brief Configures semantic state and timestamp scaling for one normalized route. */
struct CortexMDecodeRoute {
  TraceRouteIdentity identity;
  std::uint32_t timestampPrescaler = 1U;
};

/** @brief Routes OpenCSD elements to per-stream Cortex-M post-decoders. */
class CortexMStreamDecoder final : public OpenCsdTraceElementSink {
public:
  /** @brief Creates a stream router with timestamp scaling configuration. */
  CortexMStreamDecoder(const std::vector<CortexMDecodeRoute>& routes, TraceEventSink& eventSink);
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
  /** @brief Owns semantic state and timestamp scaling for one normalized route. */
  struct RouteDecoder {
    TraceRouteIdentity identity;
    std::uint32_t timestampPrescaler = 1U;
    std::unique_ptr<CortexMPostDecoder> decoder;
  };

  std::map<TraceRouteId, RouteDecoder> m_decoders;
};

#endif  // CTRACE_SRC_DECODE_CORTEXMSTREAMDECODER_H
