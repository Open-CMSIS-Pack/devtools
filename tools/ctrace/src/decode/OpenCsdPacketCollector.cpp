/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "OpenCsdPacketCollector.h"

#include "TraceEvent.h"
#include "OpenCsdTraceElement.h"
#include "TraceRoute.h"
#include "TraceStreamId.h"
#include "common/trc_gen_elem.h"
#include "opencsd/itm/trc_pkt_elem_itm.h"
#include "opencsd/itm/trc_pkt_types_itm.h"
#include "opencsd/ocsd_if_types.h"
#include "opencsd/trc_gen_elem_types.h"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

OpenCsdPacketCollector::OpenCsdPacketCollector(TraceRouteIdentity route, OpenCsdTraceElementSink& elementSink)
  : m_singleRoute(std::move(route)),
    m_elementSink(elementSink)
{
}

OpenCsdPacketCollector::OpenCsdPacketCollector(std::vector<TraceRouteIdentity> routes,
                                               OpenCsdTraceElementSink& elementSink)
  : m_elementSink(elementSink)
{
  if (routes.empty()) {
    throw std::invalid_argument("formatted OpenCSD packet collection requires at least one normalized route");
  }
  for (auto& route : routes) {
    if (!route.traceBusId.has_value() || !CoreSight::isAtbTraceId(*route.traceBusId)) {
      throw std::invalid_argument("formatted OpenCSD packet route requires a Trace Bus ID between 1 and 111");
    }
    const auto channel = *route.traceBusId;
    if (!m_routesByChannel.emplace(channel, std::move(route)).second) {
      throw std::invalid_argument("duplicate Trace Bus ID in formatted OpenCSD packet routes");
    }
  }
}

void OpenCsdPacketCollector::beginTransaction()
{
  m_transactionActive = true;
  m_transactionElements.clear();
}

void OpenCsdPacketCollector::commitTransaction()
{
  for (auto& element : m_transactionElements) {
    appendCommitted(std::move(element));
  }
  m_transactionElements.clear();
  m_transactionActive = false;
}

std::size_t OpenCsdPacketCollector::commitTransactionErrors(TraceIssueCode issueCode)
{
  std::vector<OpenCsdTraceElement> retained;
  for (auto& element : m_transactionElements) {
    if (element.kind == OpenCsdTraceElement::Kind::Error && element.issueSeverity == TraceIssueSeverity::Error &&
        element.issueCode == issueCode) {
      retained.push_back(std::move(element));
    }
  }
  m_transactionElements.clear();
  m_transactionActive = false;
  for (auto& element : retained) {
    appendCommitted(std::move(element));
  }
  return retained.size();
}

void OpenCsdPacketCollector::commitTransactionBefore(std::uint64_t sourceOffset)
{
  for (auto& element : m_transactionElements) {
    const auto elementOffset = element.sourceIndex;
    if (elementOffset < sourceOffset && element.kind != OpenCsdTraceElement::Kind::Error) {
      appendCommitted(std::move(element));
    }
  }
  m_transactionElements.clear();
  m_transactionActive = false;
}

void OpenCsdPacketCollector::rollbackTransaction()
{
  m_transactionElements.clear();
  m_transactionActive = false;
}

void OpenCsdPacketCollector::rethrowOutputError()
{
  if (!m_outputError) {
    return;
  }
  auto error = m_outputError;
  m_outputError = nullptr;
  std::rethrow_exception(error);
}

std::size_t OpenCsdPacketCollector::transactionElementCount() const
{
  return m_transactionElements.size();
}

bool OpenCsdPacketCollector::transactionHasError() const
{
  for (const auto& element : m_transactionElements) {
    if (element.kind == OpenCsdTraceElement::Kind::Error && element.issueSeverity == TraceIssueSeverity::Error) {
      return true;
    }
  }
  return false;
}

std::optional<std::uint64_t> OpenCsdPacketCollector::transactionFirstSourceOffset() const
{
  std::optional<std::uint64_t> firstOffset;
  for (const auto& element : m_transactionElements) {
    const auto offset = element.sourceIndex;
    if (!firstOffset.has_value() || offset < *firstOffset) {
      firstOffset = offset;
    }
  }
  return firstOffset;
}

void OpenCsdPacketCollector::appendDecodeError(ocsd_trc_index_t index, const std::string& message,
                                               TraceIssueCode issueCode, bool discontinuity,
                                               TraceIssueSeverity severity)
{
  const auto& route = defaultRoute();
  OpenCsdTraceElement element;
  element.kind = OpenCsdTraceElement::Kind::Error;
  element.sourceIndex = static_cast<std::uint64_t>(index);
  element.discontinuity = discontinuity;
  element.issueCode = issueCode;
  element.issueSeverity = severity;
  element.errorMessage = message;
  appendElement(std::move(element), route);
}

void OpenCsdPacketCollector::prependDiscontinuity(ocsd_trc_index_t index, const std::string& message,
                                                  TraceIssueCode issueCode,
                                                  std::optional<std::uint64_t> rawBytesConsumed)
{
  const auto& route = defaultRoute();
  OpenCsdTraceElement element;
  element.kind = OpenCsdTraceElement::Kind::Discontinuity;
  element.sourceIndex = static_cast<std::uint64_t>(index);
  element.discontinuity = true;
  element.issueCode = issueCode;
  element.errorMessage = message;
  element.rawBytesConsumed = rawBytesConsumed;
  element.route = route;
  if (m_transactionActive) {
    m_transactionElements.insert(m_transactionElements.begin(), std::move(element));
    return;
  }
  appendElement(std::move(element), route);
}

void OpenCsdPacketCollector::prependDataLossError(ocsd_trc_index_t index, const std::string& message,
                                                  std::uint64_t rawBytesConsumed)
{
  const auto& route = defaultRoute();
  OpenCsdTraceElement element;
  element.kind = OpenCsdTraceElement::Kind::Error;
  element.sourceIndex = static_cast<std::uint64_t>(index);
  element.issueCode = TraceIssueCode::DataLoss;
  element.errorMessage = message;
  element.rawBytesConsumed = rawBytesConsumed;
  element.awaitingResumeTimestamp = true;
  element.route = route;
  if (m_transactionActive) {
    m_transactionElements.insert(m_transactionElements.begin(), std::move(element));
    return;
  }
  appendElement(std::move(element), route);
}

ocsd_datapath_resp_t OpenCsdPacketCollector::TraceElemIn(const ocsd_trc_index_t index_sop,
                                                         const std::uint8_t trc_chan_id, const OcsdTraceElement& elem)
{
  try {
    const auto* route = routeForChannel(trc_chan_id);
    if (route == nullptr) {
      return OCSD_RESP_CONT;
    }
    if (elem.getType() != OCSD_GEN_TRC_ELEM_ITMTRACE) {
      return OCSD_RESP_CONT;
    }

    const auto& info = elem.swt_itm;
    switch (info.pkt_type) {
    case SWIT_PAYLOAD:
      appendSoftware(index_sop, elem, *route);
      break;
    case DWT_PAYLOAD:
      appendDwt(index_sop, elem, *route);
      break;
    case TS_SYNC:
    case TS_DELAY:
    case TS_PKT_DELAY:
    case TS_PKT_TS_DELAY:
      appendTimestamp(index_sop, elem, *route);
      break;
    case TS_GLOBAL:
      appendGlobalTimestamp(index_sop, elem, *route);
      break;
    }
  } catch (...) {
    if (!m_outputError) {
      m_outputError = std::current_exception();
    }
    return OCSD_RESP_FATAL_SYS_ERR;
  }
  return OCSD_RESP_CONT;
}

void OpenCsdPacketCollector::RawPacketDataMon(const ocsd_datapath_op_t op, const ocsd_trc_index_t index_sop,
                                              const ItmTrcPacket* pkt, const std::uint32_t size,
                                              const std::uint8_t* data)
{
  const auto* route = singleRoute();
  if (route == nullptr) {
    return;
  }
  rawPacketForRoute(*route, op, index_sop, pkt, size, data);
}

void OpenCsdPacketCollector::rawPacketForRoute(const TraceRouteIdentity& route, const ocsd_datapath_op_t op,
                                               const ocsd_trc_index_t index_sop, const ItmTrcPacket* pkt,
                                               const std::uint32_t, const std::uint8_t*)
{
  try {
    if (!containsRoute(route)) {
      throw std::invalid_argument("raw OpenCSD packet references an unknown normalized route");
    }
    appendRawPacket(route, op, index_sop, pkt);
  } catch (...) {
    if (!m_outputError) {
      m_outputError = std::current_exception();
    }
  }
}

const TraceRouteIdentity* OpenCsdPacketCollector::singleRoute() const noexcept
{
  return m_singleRoute.has_value() ? &*m_singleRoute : nullptr;
}

const TraceRouteIdentity& OpenCsdPacketCollector::defaultRoute() const noexcept
{
  if (const auto* route = singleRoute()) {
    return *route;
  }
  return m_routesByChannel.begin()->second;
}

const TraceRouteIdentity* OpenCsdPacketCollector::routeForChannel(std::uint8_t channel) const noexcept
{
  if (const auto* route = singleRoute()) {
    return route;
  }
  const auto found = m_routesByChannel.find(channel);
  return found != m_routesByChannel.end() ? &found->second : nullptr;
}

bool OpenCsdPacketCollector::containsRoute(const TraceRouteIdentity& route) const noexcept
{
  if (const auto* boundRoute = singleRoute()) {
    return *boundRoute == route;
  }
  if (!route.traceBusId.has_value()) {
    return false;
  }
  const auto found = m_routesByChannel.find(*route.traceBusId);
  return found != m_routesByChannel.end() && found->second == route;
}

void OpenCsdPacketCollector::appendRawPacket(const TraceRouteIdentity& route, const ocsd_datapath_op_t op,
                                             const ocsd_trc_index_t index_sop, const ItmTrcPacket* pkt)
{
  if (pkt == nullptr) {
    return;
  }

  // OpenCSD publishes the incomplete packet through the raw monitor as DATA
  // while processing EOT; its following EOT monitor notification has no packet.
  if (pkt->getPktType() == ITM_PKT_INCOMPLETE_EOT) {
    OpenCsdTraceElement element;
    element.kind = OpenCsdTraceElement::Kind::Error;
    element.sourceIndex = static_cast<std::uint64_t>(index_sop);
    element.discontinuity = true;
    element.issueCode = TraceIssueCode::OpenCsdIncompleteTail;
    element.issueSeverity = TraceIssueSeverity::Error;
    element.errorMessage = "incomplete ITM packet at end of input";
    appendElement(std::move(element), route);
    return;
  }
  if (op != OCSD_OP_DATA) {
    return;
  }

  switch (pkt->getPktType()) {
  case ITM_PKT_ASYNC:
    appendSync(index_sop, route);
    break;
  case ITM_PKT_OVERFLOW:
    appendOverflow(index_sop, route);
    break;
  case ITM_PKT_TS_GLOBAL_1:
  case ITM_PKT_TS_GLOBAL_2:
    // The ITM decoder combines GTS1/GTS2 and publishes the complete
    // 64-bit value as a generic TS_GLOBAL element. Raw fragments are
    // intentionally not forwarded as independent timestamps.
    break;
  case ITM_PKT_BAD_SEQUENCE:
  case ITM_PKT_RESERVED:
    appendError(index_sop, *pkt, route);
    break;
  default:
    break;
  }
}

void OpenCsdPacketCollector::appendSync(ocsd_trc_index_t index, const TraceRouteIdentity& route)
{
  OpenCsdTraceElement element;
  element.kind = OpenCsdTraceElement::Kind::Sync;
  element.sourceIndex = static_cast<std::uint64_t>(index);
  appendElement(std::move(element), route);
}

void OpenCsdPacketCollector::appendOverflow(ocsd_trc_index_t index, const TraceRouteIdentity& route)
{
  OpenCsdTraceElement element;
  element.kind = OpenCsdTraceElement::Kind::Overflow;
  element.sourceIndex = static_cast<std::uint64_t>(index);
  appendElement(std::move(element), route);
}

void OpenCsdPacketCollector::appendGlobalTimestamp(ocsd_trc_index_t index, const OcsdTraceElement& elem,
                                                   const TraceRouteIdentity& route)
{
  OpenCsdTraceElement element;
  element.kind = OpenCsdTraceElement::Kind::GlobalTimestamp;
  element.sourceIndex = static_cast<std::uint64_t>(index);
  element.timestampValue = elem.timestamp;
  element.clockChange = elem.cpu_freq_change != 0U;
  appendElement(std::move(element), route);
}

void OpenCsdPacketCollector::appendError(ocsd_trc_index_t index, const ItmTrcPacket& pkt,
                                         const TraceRouteIdentity& route)
{
  OpenCsdTraceElement element;
  element.kind = OpenCsdTraceElement::Kind::Error;
  element.sourceIndex = static_cast<std::uint64_t>(index);
  element.issueCode = TraceIssueCode::OpenCsdDecodeError;
  element.errorMessage = pkt.getPktType() == ITM_PKT_RESERVED ? "Reserved ITM packet" : "Bad ITM packet sequence";
  appendElement(std::move(element), route);
}

void OpenCsdPacketCollector::appendSoftware(ocsd_trc_index_t index, const OcsdTraceElement& elem,
                                            const TraceRouteIdentity& route)
{
  const auto& info = elem.swt_itm;
  OpenCsdTraceElement element;
  element.kind = OpenCsdTraceElement::Kind::Software;
  element.sourceIndex = static_cast<std::uint64_t>(index);
  element.channel = info.payload_src_id;
  element.size = info.payload_size;
  element.value = info.value;
  element.overflow = info.overflow;
  appendElement(std::move(element), route);
}

void OpenCsdPacketCollector::appendDwt(ocsd_trc_index_t index, const OcsdTraceElement& elem,
                                       const TraceRouteIdentity& route)
{
  const auto& info = elem.swt_itm;
  OpenCsdTraceElement element;
  element.kind = OpenCsdTraceElement::Kind::Hardware;
  element.sourceIndex = static_cast<std::uint64_t>(index);
  element.discriminator = info.payload_src_id;
  element.size = info.payload_size;
  element.value = info.value;
  element.overflow = info.overflow;
  appendElement(std::move(element), route);
}

void OpenCsdPacketCollector::appendTimestamp(ocsd_trc_index_t index, const OcsdTraceElement& elem,
                                             const TraceRouteIdentity& route)
{
  const auto& info = elem.swt_itm;
  OpenCsdTraceElement element;
  element.kind = OpenCsdTraceElement::Kind::LocalTimestamp;
  element.sourceIndex = static_cast<std::uint64_t>(index);
  element.timestampRelation = timestampRelation(info.pkt_type);
  element.tcyc = elem.timestamp;
  element.overflow = info.overflow;
  appendElement(std::move(element), route);
}

LocalTimestampRelation OpenCsdPacketCollector::timestampRelation(swt_itm_type type)
{
  if (type == TS_DELAY) {
    return LocalTimestampRelation::TimestampDelayed;
  }
  if (type == TS_PKT_DELAY) {
    return LocalTimestampRelation::PayloadDelayed;
  }
  if (type == TS_PKT_TS_DELAY) {
    return LocalTimestampRelation::TimestampAndPayloadDelayed;
  }
  return LocalTimestampRelation::Synchronous;
}

void OpenCsdPacketCollector::appendElement(OpenCsdTraceElement element, const TraceRouteIdentity& route)
{
  element.route = route;
  if (m_transactionActive) {
    m_transactionElements.push_back(std::move(element));
    return;
  }
  appendCommitted(std::move(element));
}

void OpenCsdPacketCollector::appendCommitted(OpenCsdTraceElement element)
{
  m_elementSink.append(std::move(element));
}
