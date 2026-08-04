/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CortexMStreamDecoder.h"

#include "CortexMPostDecoder.h"
#include "OpenCsdTraceElement.h"
#include "SaturatingArithmetic.h"
#include "TraceEvent.h"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

CortexMStreamDecoder::CortexMStreamDecoder(ItmTimestampPrescalers prescalers, TraceEventSink& eventSink)
  : m_prescalers(std::move(prescalers)), m_eventSink(eventSink)
{
}

CortexMStreamDecoder::~CortexMStreamDecoder() = default;

void CortexMStreamDecoder::append(OpenCsdTraceElement element)
{
  if (element.kind == OpenCsdTraceElement::Kind::LocalTimestamp && element.tcyc.has_value()) {
    element.tcyc = SaturatingArithmetic::multiply(*element.tcyc, prescaler(element.traceBusId));
  }
  decoder(element.traceBusId).append(std::move(element));
}

void CortexMStreamDecoder::finish()
{
  for (auto& [stream, decoder] : m_decoders) {
    (void)stream;
    decoder->finish();
  }
}

std::uint64_t CortexMStreamDecoder::eventCount() const
{
  std::uint64_t count = 0U;
  for (const auto& [stream, decoder] : m_decoders) {
    (void)stream;
    count += decoder->eventCount();
  }
  return count;
}

std::uint32_t CortexMStreamDecoder::prescaler(std::uint8_t traceBusId) const
{
  const auto found = m_prescalers.byTraceBusId.find(traceBusId);
  if (found != m_prescalers.byTraceBusId.end()) {
    return found->second;
  }
  if (m_prescalers.fallback.has_value()) {
    return *m_prescalers.fallback;
  }
  if (traceBusId == 0U) {
    throw std::runtime_error("ITM timestamps with Trace Bus ID 0 cannot be assigned to "
                             "different processor prescalers; a formatted source ID is required");
  }
  throw std::runtime_error("CoreSight Trace Bus ID " + std::to_string(traceBusId) +
                           " has no unambiguous processor timestamp prescaler");
}

CortexMPostDecoder& CortexMStreamDecoder::decoder(std::uint8_t traceBusId)
{
  auto& result = m_decoders[traceBusId];
  if (!result) {
    result = std::make_unique<CortexMPostDecoder>(m_eventSink);
  }
  return *result;
}
