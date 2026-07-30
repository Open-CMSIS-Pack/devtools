/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#pragma once

#include "CortexMStreamDecoder.hpp"
#include "TraceEvent.hpp"
#include "OpenCsdItmDecoder.hpp"

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

  void push(RawByteView bytes);
  DecodeResult finish();

private:
  CortexMStreamDecoder streamDecoder_;
  OpenCsdItmDecoder decoder_;
};
