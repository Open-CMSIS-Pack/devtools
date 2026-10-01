/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CortexMStreamDecoder.h"

#include "CortexMPostDecoder.h"
#include "DiagnosticMessages.h"
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

CortexMStreamDecoder::CortexMStreamDecoder(const std::vector<CortexMDecodeRoute>& routes, TraceEventSink& eventSink)
{
  if (routes.empty()) {
    throw std::invalid_argument(formatMessage(MessageId::CortexRoutesRequired));
  }

  std::set<std::uint8_t> traceBusIds;
  for (const auto& route : routes) {
    if (route.timestampPrescaler == 0U) {
      throw std::invalid_argument(formatMessage(MessageId::TimestampPrescalerPositive));
    }
    if (route.identity.traceBusId.has_value() && !CoreSight::isAtbTraceId(*route.identity.traceBusId)) {
      throw std::invalid_argument(formatMessage(MessageId::CortexTraceBusIdRange));
    }
    if (route.identity.traceBusId.has_value() && !traceBusIds.insert(*route.identity.traceBusId).second) {
      throw std::invalid_argument(formatMessage(MessageId::CortexDuplicateTraceBusId));
    }
    RouteDecoder state;
    state.identity = route.identity;
    state.timestampPrescaler = route.timestampPrescaler;
    state.decoder = std::make_unique<CortexMPostDecoder>(route.identity, eventSink);
    if (!m_decoders.emplace(route.identity.id, std::move(state)).second) {
      throw std::invalid_argument(formatMessage(MessageId::CortexDuplicateRouteId));
    }
  }
}

CortexMStreamDecoder::~CortexMStreamDecoder() = default;

void CortexMStreamDecoder::append(OpenCsdTraceElement element)
{
  const auto found = m_decoders.find(element.route.id);
  if (found == m_decoders.end()) {
    throw std::runtime_error(unknownNormalizedRouteMessage(element.route.id.value()));
  }
  auto& route = found->second;
  if (element.route != route.identity) {
    throw std::runtime_error(formatMessage(MessageId::CortexRouteIdentityMismatch));
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
