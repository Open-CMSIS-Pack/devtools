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
#include "TraceRoute.h"
#include "TraceStreamId.h"

#include <cstdint>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

CortexMStreamDecoder::CortexMStreamDecoder(std::vector<CortexMDecodeRoute> routes, TraceEventSink& eventSink)
{
  if (routes.empty()) {
    throw std::invalid_argument("Cortex-M stream decoding requires at least one normalized route");
  }

  std::set<std::uint8_t> traceBusIds;
  for (const auto& route : routes) {
    if (route.timestampPrescaler == 0U) {
      throw std::invalid_argument("ITM timestamp prescaler must be greater than zero");
    }
    if (route.identity.traceBusId.has_value() && !CoreSight::isAtbTraceId(*route.identity.traceBusId)) {
      throw std::invalid_argument("formatted Cortex-M route requires a CoreSight ATB trace ID between 1 and 111");
    }
    if (route.identity.traceBusId.has_value() && !traceBusIds.insert(*route.identity.traceBusId).second) {
      throw std::invalid_argument("duplicate CoreSight ATB trace ID in Cortex-M route configuration");
    }
    RouteDecoder state;
    state.identity = route.identity;
    state.timestampPrescaler = route.timestampPrescaler;
    state.decoder = std::make_unique<CortexMPostDecoder>(route.identity, eventSink);
    if (!m_decoders.emplace(route.identity.id, std::move(state)).second) {
      throw std::invalid_argument("duplicate normalized route ID in Cortex-M route configuration");
    }
  }
}

CortexMStreamDecoder::~CortexMStreamDecoder() = default;

void CortexMStreamDecoder::append(OpenCsdTraceElement element)
{
  const auto found = m_decoders.find(element.route.id);
  if (found == m_decoders.end()) {
    throw std::runtime_error("OpenCSD element references unknown normalized route " +
                             std::to_string(element.route.id.value()));
  }
  auto& route = found->second;
  if (element.route != route.identity) {
    throw std::runtime_error("OpenCSD element route identity does not match normalized route catalogue");
  }
  if (element.kind == OpenCsdTraceElement::Kind::LocalTimestamp && element.tcyc.has_value()) {
    element.tcyc = SaturatingArithmetic::multiply(*element.tcyc, route.timestampPrescaler);
  }
  route.decoder->append(std::move(element));
}

void CortexMStreamDecoder::finish()
{
  for (auto& [routeId, route] : m_decoders) {
    (void)routeId;
    route.decoder->finish();
  }
}

std::uint64_t CortexMStreamDecoder::eventCount() const
{
  std::uint64_t count = 0U;
  for (const auto& [routeId, route] : m_decoders) {
    (void)routeId;
    count += route.decoder->eventCount();
  }
  return count;
}
