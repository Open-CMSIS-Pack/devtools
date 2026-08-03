/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "DecodePipeline.h"

#include "CortexMStreamDecoder.h"
#include "TraceEvent.h"

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

DecodePipeline::DecodePipeline(std::uint32_t timestampPrescaler, TraceEventSink& eventSink)
  : DecodePipeline(ItmTimestampPrescalers{timestampPrescaler, {}}, eventSink)
{
}

DecodePipeline::DecodePipeline(ItmTimestampPrescalers timestampPrescalers, TraceEventSink& eventSink)
  : streamDecoder_(std::move(timestampPrescalers), eventSink), decoder_(streamDecoder_)
{
}

DecodePipeline::DecodePipeline(ItmTimestampPrescalers timestampPrescalers, TraceEventSink& eventSink,
                               const OpenCsdItmSessionFactory& sessionFactory)
  : streamDecoder_(std::move(timestampPrescalers), eventSink), decoder_(streamDecoder_, sessionFactory)
{
}

void DecodePipeline::push(RawByteView bytes)
{
  if (bytes.empty()) {
    return;
  }
  if (bytes.size > std::numeric_limits<std::uint32_t>::max()) {
    throw std::runtime_error("raw decode chunk is too large");
  }
  decoder_.push(bytes.data, static_cast<std::uint32_t>(bytes.size));
}

DecodeResult DecodePipeline::finish()
{
  const auto result = decoder_.finish();
  streamDecoder_.finish();
  return {
      result.bytesIn,
      streamDecoder_.eventCount(),
  };
}
