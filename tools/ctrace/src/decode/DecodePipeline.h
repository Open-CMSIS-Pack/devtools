/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_DECODE_DECODEPIPELINE_H
#define CTRACE_SRC_DECODE_DECODEPIPELINE_H

#include "CortexMStreamDecoder.h"
#include "TraceEvent.h"
#include "OpenCsdItmDecoder.h"

#include <cstddef>
#include <cstdint>

/** @brief Provides a non-owning view of one raw trace byte chunk. */
struct RawByteView {
  const std::uint8_t* data = nullptr;
  std::size_t size = 0;

  /** @brief Reports whether the view contains no bytes. */
  constexpr bool empty() const
  {
    return size == 0U;
  }
};

/** @brief Summarizes raw bytes consumed and semantic packets produced. */
struct DecodeResult {
  std::uint64_t bytesIn = 0;
  std::uint64_t packetsOut = 0;
};

/** @brief Connects raw OpenCSD decoding with Cortex-M semantic post-decoding. */
class DecodePipeline final {
public:
  /** @brief Creates a pipeline using one timestamp prescaler for every stream. */
  DecodePipeline(std::uint32_t timestampPrescaler, TraceEventSink& eventSink);
  /** @brief Creates a pipeline with stream-specific timestamp prescalers. */
  DecodePipeline(ItmTimestampPrescalers timestampPrescalers, TraceEventSink& eventSink);
  /** @brief Creates a pipeline with an injected OpenCSD session factory. */
  DecodePipeline(ItmTimestampPrescalers timestampPrescalers, TraceEventSink& eventSink,
                 const OpenCsdItmSessionFactory& sessionFactory);

  /** @brief Pushes the next contiguous chunk of raw trace bytes. */
  void push(RawByteView bytes);
  /** @brief Finalizes decoding and returns aggregate counters. */
  DecodeResult finish();

private:
  CortexMStreamDecoder streamDecoder_;
  OpenCsdItmDecoder decoder_;
};

#endif  // CTRACE_SRC_DECODE_DECODEPIPELINE_H
