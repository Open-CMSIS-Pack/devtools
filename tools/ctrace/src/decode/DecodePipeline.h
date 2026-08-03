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

struct RawByteView {
  const std::uint8_t* data = nullptr;
  std::size_t size = 0;

  constexpr bool empty() const
  {
    return size == 0U;
  }
};

struct DecodeResult {
  std::uint64_t bytesIn = 0;
  std::uint64_t packetsOut = 0;
};

class DecodePipeline final {
public:
  DecodePipeline(std::uint32_t timestampPrescaler, TraceEventSink& eventSink);
  DecodePipeline(ItmTimestampPrescalers timestampPrescalers, TraceEventSink& eventSink);
  DecodePipeline(ItmTimestampPrescalers timestampPrescalers, TraceEventSink& eventSink,
                 const OpenCsdItmSessionFactory& sessionFactory);

  void push(RawByteView bytes);
  DecodeResult finish();

private:
  CortexMStreamDecoder streamDecoder_;
  OpenCsdItmDecoder decoder_;
};

#endif  // CTRACE_SRC_DECODE_DECODEPIPELINE_H
