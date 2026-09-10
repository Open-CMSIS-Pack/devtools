/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "DecodePipeline.h"

#include "CortexMStreamDecoder.h"
#include "OpenCsdItmDecoder.h"
#include "TraceEvent.h"

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

DecodePipeline::DecodePipeline(CortexMDecodeRoute route, TraceEventSink& eventSink)
  : m_streamDecoder({route}, eventSink),
    m_decoder(std::move(route.identity), m_streamDecoder)
{
}

DecodePipeline::DecodePipeline(CortexMDecodeRoute route, TraceEventSink& eventSink,
                               const OpenCsdItmSessionFactory& sessionFactory)
  : m_streamDecoder({route}, eventSink),
    m_decoder(std::move(route.identity), m_streamDecoder, sessionFactory)
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
  m_decoder.push(bytes.data, static_cast<std::uint32_t>(bytes.size));
}

DecodeResult DecodePipeline::finish()
{
  const auto result = m_decoder.finish();
  m_streamDecoder.finish();
  return {
      result.bytesIn,
      m_streamDecoder.eventCount(),
  };
}
