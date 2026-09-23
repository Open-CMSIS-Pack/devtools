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
#include <vector>

/** @brief Extracts decoder identities while retaining post-decoder route configuration. */
static std::vector<TraceRouteIdentity> routeIdentities(const std::vector<CortexMDecodeRoute>& routes)
{
  std::vector<TraceRouteIdentity> identities;
  identities.reserve(routes.size());
  for (const auto& route : routes) {
    identities.push_back(route.identity);
  }
  return identities;
}

DecodePipeline::DecodePipeline(std::vector<CortexMDecodeRoute> routes, OpenCsdItmInputMode inputMode,
                               TraceEventSink& eventSink, OpenCsdUnsupportedTraceIdObserver unsupportedTraceIdSink)
  : m_streamDecoder(routes, eventSink),
    m_decoder(routeIdentities(routes), inputMode, m_streamDecoder, std::move(unsupportedTraceIdSink),
              [this, &eventSink](const TraceByteSkip& skipped) {
                ++m_byteSkipCount;
                eventSink.appendByteSkip(skipped);
              })
{
}

DecodePipeline::DecodePipeline(std::vector<CortexMDecodeRoute> routes, OpenCsdItmInputMode inputMode,
                               TraceEventSink& eventSink, const OpenCsdItmSessionFactory& sessionFactory)
  : m_streamDecoder(routes, eventSink),
    m_decoder(routeIdentities(routes), inputMode, m_streamDecoder, sessionFactory)
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
      m_streamDecoder.eventCount() + m_byteSkipCount,
  };
}
