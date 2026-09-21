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

#include <algorithm>
#include <array>
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
                                               OpenCsdTraceElementSink& elementSink,
                                               OpenCsdSkippedBytesSink skippedBytesSink)
  : m_elementSink(elementSink),
    m_skippedBytesSink(std::move(skippedBytesSink))
{
  if (routes.empty()) {
    throw std::invalid_argument("formatted OpenCSD packet collection requires at least one normalized route");
  }
  for (auto& route : routes) {
    if (!route.traceBusId.has_value() || !CoreSight::isAtbTraceId(*route.traceBusId)) {
      throw std::invalid_argument("formatted OpenCSD packet route requires a Trace Bus ID between 1 and 111");
    }
    const auto channel = *route.traceBusId;
    const auto duplicateRouteId = std::any_of(m_routesByChannel.begin(), m_routesByChannel.end(),
                                              [&](const auto& item) { return item.second.id == route.id; });
    if (duplicateRouteId) {
      throw std::invalid_argument("duplicate normalized route ID in formatted OpenCSD packet routes");
    }
    if (!m_routesByChannel.emplace(channel, route).second) {
      throw std::invalid_argument("duplicate Trace Bus ID in formatted OpenCSD packet routes");
    }
    m_formattedDataByRoute.emplace(route.id, FormattedRouteData{std::move(route), {}, {}});
  }
}

void OpenCsdPacketCollector::beginTransaction()
{
  m_transactionActive = true;
  m_nextTransactionOrder = 1U;
  m_transactionElements.clear();
  m_packetErrorContexts.clear();
}

std::optional<std::uint64_t> OpenCsdPacketCollector::reserveTransactionOrder() noexcept
{
  if (!m_transactionActive) {
    return std::nullopt;
  }
  return m_nextTransactionOrder++;
}

void OpenCsdPacketCollector::commitTransaction()
{
  for (auto& buffered : m_transactionElements) {
    appendCommitted(std::move(buffered.element));
  }
  m_transactionElements.clear();
  m_transactionActive = false;
}

void OpenCsdPacketCollector::commitTransactionForRouteFailures(
    const std::map<TraceRouteId, std::uint64_t>& sourceOffsetsByRoute)
{
  for (const auto& routeCutoff : sourceOffsetsByRoute) {
    const auto routeId = routeCutoff.first;
    const auto knownConfiguredRoute = (m_singleRoute.has_value() && m_singleRoute->id == routeId) ||
                                      std::any_of(m_routesByChannel.begin(), m_routesByChannel.end(),
                                                  [routeId](const auto& item) { return item.second.id == routeId; });
    if (!knownConfiguredRoute) {
      throw std::invalid_argument("route-aware OpenCSD transaction cutoff references an unknown normalized route");
    }
  }

  for (auto& buffered : m_transactionElements) {
    auto& element = buffered.element;
    const auto cutoff = sourceOffsetsByRoute.find(element.route.id);
    const auto affected = cutoff != sourceOffsetsByRoute.end();
    const auto safeBeforeFailure = !affected || element.sourceIndex < cutoff->second;
    const auto reportedDiagnostic = affected && buffered.reportedDiagnostic;
    if (safeBeforeFailure || reportedDiagnostic) {
      appendCommitted(std::move(element));
    }
  }
  m_transactionElements.clear();
  m_transactionActive = false;
}

std::size_t OpenCsdPacketCollector::commitTransactionErrors(TraceIssueCode issueCode)
{
  std::vector<OpenCsdTraceElement> retained;
  for (auto& buffered : m_transactionElements) {
    auto& element = buffered.element;
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
  for (auto& buffered : m_transactionElements) {
    auto& element = buffered.element;
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

std::string OpenCsdPacketCollector::packetErrorContext(std::optional<std::uint8_t> channel,
                                                        std::uint64_t index) const
{
  const auto* route = channel.has_value() ? routeForChannel(*channel) : singleRoute();
  if (route == nullptr) {
    return {};
  }
  const auto found = m_packetErrorContexts.find({route->id, index});
  return found == m_packetErrorContexts.end() ? std::string{} : found->second;
}

void OpenCsdPacketCollector::clearPacketErrorContexts() noexcept
{
  m_packetErrorContexts.clear();
}

std::size_t OpenCsdPacketCollector::transactionElementCount() const
{
  return m_transactionElements.size();
}

bool OpenCsdPacketCollector::transactionHasUnmatchedError(
    const std::map<TraceRouteId, std::uint64_t>& sourceOffsetsByRoute) const
{
  return std::any_of(m_transactionElements.begin(), m_transactionElements.end(), [&](const auto& buffered) {
    const auto& element = buffered.element;
    if (element.kind != OpenCsdTraceElement::Kind::Error || element.issueSeverity != TraceIssueSeverity::Error) {
      return false;
    }
    if (element.issueCode == TraceIssueCode::OpenCsdIncompleteTail) {
      return true;
    }
    if (element.issueCode == TraceIssueCode::DataLoss) {
      return false;
    }
    const auto cutoff = sourceOffsetsByRoute.find(element.route.id);
    return cutoff == sourceOffsetsByRoute.end() || element.sourceIndex < cutoff->second;
  });
}

std::optional<std::uint64_t> OpenCsdPacketCollector::transactionFirstSourceOffset() const
{
  std::optional<std::uint64_t> firstOffset;
  for (const auto& buffered : m_transactionElements) {
    const auto& element = buffered.element;
    const auto offset = element.sourceIndex;
    if (!firstOffset.has_value() || offset < *firstOffset) {
      firstOffset = offset;
    }
  }
  return firstOffset;
}

std::optional<std::uint64_t>
OpenCsdPacketCollector::transactionFirstSyncOffset(const TraceRouteIdentity& route,
                                                   std::optional<std::uint64_t> beforeOffset) const
{
  const auto sync = std::find_if(m_transactionElements.begin(), m_transactionElements.end(), [&](const auto& buffered) {
    return buffered.element.route == route && buffered.element.kind == OpenCsdTraceElement::Kind::Sync &&
           (!beforeOffset.has_value() || buffered.element.sourceIndex < *beforeOffset);
  });
  return sync == m_transactionElements.end() ? std::nullopt : std::optional<std::uint64_t>(sync->element.sourceIndex);
}

void OpenCsdPacketCollector::appendDecodeError(ocsd_trc_index_t index, const std::string& message,
                                               TraceIssueCode issueCode, bool discontinuity,
                                               TraceIssueSeverity severity)
{
  appendDecodeError(defaultRoute(), index, message, issueCode, discontinuity, severity);
}

void OpenCsdPacketCollector::appendDecodeError(const TraceRouteIdentity& route, ocsd_trc_index_t index,
                                               const std::string& message, TraceIssueCode issueCode, bool discontinuity,
                                               TraceIssueSeverity severity)
{
  appendDecodeErrorImpl(route, index, message, issueCode, discontinuity, severity, std::nullopt, false);
}

void OpenCsdPacketCollector::appendReportedDecodeError(ocsd_trc_index_t index, const std::string& message,
                                                       std::optional<std::uint64_t> callbackOrder,
                                                       TraceIssueCode issueCode, bool discontinuity,
                                                       TraceIssueSeverity severity)
{
  appendReportedDecodeError(defaultRoute(), index, message, callbackOrder, issueCode, discontinuity, severity);
}

void OpenCsdPacketCollector::appendReportedDecodeError(const TraceRouteIdentity& route, ocsd_trc_index_t index,
                                                       const std::string& message,
                                                       std::optional<std::uint64_t> callbackOrder,
                                                       TraceIssueCode issueCode, bool discontinuity,
                                                       TraceIssueSeverity severity)
{
  appendDecodeErrorImpl(route, index, message, issueCode, discontinuity, severity, callbackOrder, true);
}

void OpenCsdPacketCollector::appendDecodeErrorImpl(const TraceRouteIdentity& route, ocsd_trc_index_t index,
                                                   const std::string& message, TraceIssueCode issueCode,
                                                   bool discontinuity, TraceIssueSeverity severity,
                                                   std::optional<std::uint64_t> callbackOrder, bool reportedDiagnostic)
{
  if (!containsRoute(route)) {
    throw std::invalid_argument("OpenCSD diagnostic references an unknown normalized route");
  }
  OpenCsdTraceElement element;
  element.kind = OpenCsdTraceElement::Kind::Error;
  element.sourceIndex = static_cast<std::uint64_t>(index);
  element.discontinuity = discontinuity;
  element.issueCode = issueCode;
  element.issueSeverity = severity;
  element.errorMessage = message;
  element.route = route;
  if (!m_transactionActive || !callbackOrder.has_value()) {
    if (m_transactionActive) {
      const auto order = reserveTransactionOrder().value();
      m_transactionElements.push_back(BufferedElement{std::move(element), order, reportedDiagnostic});
    } else {
      appendCommitted(std::move(element));
    }
    return;
  }

  const auto position =
      std::lower_bound(m_transactionElements.begin(), m_transactionElements.end(), *callbackOrder,
                       [](const auto& buffered, std::uint64_t order) { return buffered.callbackOrder < order; });
  m_transactionElements.insert(position, BufferedElement{std::move(element), *callbackOrder, reportedDiagnostic});
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
    m_transactionElements.insert(m_transactionElements.begin(), BufferedElement{std::move(element), 0U, false});
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
    m_transactionElements.insert(m_transactionElements.begin(), BufferedElement{std::move(element), 0U, false});
    return;
  }
  appendElement(std::move(element), route);
}

void OpenCsdPacketCollector::appendDataLossError(const TraceRouteIdentity& route, ocsd_trc_index_t index,
                                                 const std::string& message, std::uint64_t rawBytesConsumed)
{
  if (!containsRoute(route)) {
    throw std::invalid_argument("OpenCSD data-loss interval references an unknown normalized route");
  }
  OpenCsdTraceElement element;
  element.kind = OpenCsdTraceElement::Kind::Error;
  element.sourceIndex = static_cast<std::uint64_t>(index);
  element.issueCode = TraceIssueCode::DataLoss;
  element.errorMessage = message;
  element.rawBytesConsumed = rawBytesConsumed;
  element.awaitingResumeTimestamp = true;
  appendElement(std::move(element), route);
}

bool OpenCsdPacketCollector::insertDataLossBeforeSync(const TraceRouteIdentity& route, ocsd_trc_index_t index,
                                                      const std::string& message, std::uint64_t rawBytesConsumed,
                                                      std::optional<std::uint64_t> beforeOffset)
{
  if (!containsRoute(route)) {
    throw std::invalid_argument("OpenCSD data-loss interval references an unknown normalized route");
  }
  if (!m_transactionActive) {
    return false;
  }
  const auto sync = std::find_if(m_transactionElements.begin(), m_transactionElements.end(), [&](const auto& buffered) {
    return buffered.element.route == route && buffered.element.kind == OpenCsdTraceElement::Kind::Sync &&
           (!beforeOffset.has_value() || buffered.element.sourceIndex < *beforeOffset);
  });
  if (sync == m_transactionElements.end()) {
    return false;
  }

  OpenCsdTraceElement element;
  element.kind = OpenCsdTraceElement::Kind::Error;
  element.sourceIndex = static_cast<std::uint64_t>(index);
  element.issueCode = TraceIssueCode::DataLoss;
  element.errorMessage = message;
  element.rawBytesConsumed = rawBytesConsumed;
  element.awaitingResumeTimestamp = true;
  element.route = route;
  m_transactionElements.insert(sync, BufferedElement{std::move(element), sync->callbackOrder, false});
  return true;
}

ocsd_datapath_resp_t OpenCsdPacketCollector::TraceElemIn(const ocsd_trc_index_t index_sop,
                                                         const std::uint8_t trc_chan_id, const OcsdTraceElement& elem)
{
  try {
    const auto* route = callbackRouteForChannel(trc_chan_id);
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
                                               const std::uint32_t size, const std::uint8_t* data)
{
  try {
    if (!containsRoute(route)) {
      throw std::invalid_argument("raw OpenCSD packet references an unknown normalized route");
    }
    appendRawPacket(route, op, index_sop, pkt, size, data);
  } catch (...) {
    if (!m_outputError) {
      m_outputError = std::current_exception();
    }
  }
}

void OpenCsdPacketCollector::formattedDataForRoute(const TraceRouteIdentity& route, ocsd_trc_index_t index,
                                                   std::uint32_t size)
{
  const auto found = m_formattedDataByRoute.find(route.id);
  if (found == m_formattedDataByRoute.end() || found->second.route != route) {
    throw std::invalid_argument("formatted data references an unknown normalized route");
  }
  auto& state = found->second;
  if (size == 0U || state.synchronized || state.lastFormatterOffset == index) {
    return;
  }
  if (!state.firstFormatterOffset.has_value()) {
    state.firstFormatterOffset = index;
  }
  state.lastFormatterOffset = index;
  state.byteCount += size;
}

void OpenCsdPacketCollector::reportUnsynchronizedFormattedRoutes()
{
  for (auto& [routeId, state] : m_formattedDataByRoute) {
    (void)routeId;
    if (!state.firstFormatterOffset.has_value() || state.synchronized || state.diagnosed) {
      continue;
    }
    state.diagnosed = true;
    if (m_skippedBytesSink) {
      m_skippedBytesSink({*state.firstFormatterOffset, state.byteCount, TraceByteSkipReason::MissingSync,
                          state.route.traceBusId});
    }
    const auto message = "no hardware ITM SYNC before end of input; "
                         "first formatter group at raw offset " + std::to_string(*state.firstFormatterOffset);
    appendDecodeError(state.route, *state.firstFormatterOffset, message, TraceIssueCode::OpenCsdMissingSync, true);
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
    return channel == 0U ? route : nullptr;
  }
  const auto found = m_routesByChannel.find(channel);
  return found != m_routesByChannel.end() ? &found->second : nullptr;
}

const TraceRouteIdentity* OpenCsdPacketCollector::callbackRouteForChannel(std::uint8_t channel) const noexcept
{
  if (const auto* route = singleRoute()) {
    return route;
  }
  return routeForChannel(channel);
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
                                             const ocsd_trc_index_t index_sop, const ItmTrcPacket* pkt,
                                             std::uint32_t size, const std::uint8_t* data)
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
    element.errorMessage = "incomplete ITM packet at end of input at raw offset " + std::to_string(index_sop) +
                             "; " + capturePacketErrorContext(route, index_sop, *pkt, size, data);
    appendElement(std::move(element), route);
    return;
  }
  if (op != OCSD_OP_DATA) {
    return;
  }

  switch (pkt->getPktType()) {
  case ITM_PKT_NOTSYNC:
    recordUnsynchronizedBytes(route, size);
    break;
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
    appendError(index_sop, *pkt, route, capturePacketErrorContext(route, index_sop, *pkt, size, data));
    break;
  default:
    break;
  }
}

std::string OpenCsdPacketCollector::capturePacketErrorContext(const TraceRouteIdentity& route, ocsd_trc_index_t index,
                                                               const ItmTrcPacket& packet, std::uint32_t size,
                                                               const std::uint8_t* data)
{
  const auto type = packet.getPktType();
  std::string name = type == ITM_PKT_RESERVED ? "RESERVED"
                    : type == ITM_PKT_INCOMPLETE_EOT ? "INCOMPLETE_EOT"
                                                   : "BAD_SEQUENCE";
  if (type != ITM_PKT_RESERVED && packet.err_type >= ITM_PKT_ASYNC && packet.err_type <= ITM_PKT_EXTENSION) {
    // OpenCSD's valid packet types form this contiguous range. Error packets
    // retain their original type in err_type, independently of payload fields.
    constexpr std::array<const char*, 8U> names{
        "ASYNC", "OVERFLOW", "SWIT", "DWT", "TS_LOCAL", "TS_GLOBAL_1", "TS_GLOBAL_2", "EXTENSION"};
    name = names[static_cast<std::size_t>(packet.err_type - ITM_PKT_ASYNC)];
  }
  std::string context = "packet=" + name + ", size=" + std::to_string(size) +
                        (size == 1U ? " byte, bytes=" : " bytes, bytes=");
  if (data == nullptr && size != 0U) {
    context += "unavailable";
  } else {
    constexpr std::uint32_t maxPrefixBytes = 16U;
    constexpr char hexDigits[] = "0123456789abcdef";
    context += '[';
    const auto shown = std::min(size, maxPrefixBytes);
    for (std::uint32_t offset = 0U; offset < shown; ++offset) {
      if (offset != 0U) {
        context += ' ';
      }
      context += hexDigits[data[offset] >> 4U];
      context += hexDigits[data[offset] & 0x0fU];
    }
    if (shown < size) {
      context += " ... (truncated)";
    }
    context += ']';
  }
  m_packetErrorContexts.insert_or_assign({route.id, static_cast<std::uint64_t>(index)}, context);
  return context;
}

void OpenCsdPacketCollector::recordUnsynchronizedBytes(const TraceRouteIdentity& route, std::uint32_t size)
{
  const auto found = m_formattedDataByRoute.find(route.id);
  if (found != m_formattedDataByRoute.end() && !found->second.synchronized && !found->second.diagnosed) {
    // Packet monitor sizes count discarded protocol bytes, not formatter bytes or sync packets.
    // The same packet index can occur on several flushes, so it cannot be used for deduplication.
    found->second.unsynchronizedByteCount += size;
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
                                         const TraceRouteIdentity& route, const std::string& context)
{
  OpenCsdTraceElement element;
  element.kind = OpenCsdTraceElement::Kind::Error;
  element.sourceIndex = static_cast<std::uint64_t>(index);
  element.issueCode = TraceIssueCode::OpenCsdDecodeError;
  element.errorMessage = pkt.getPktType() == ITM_PKT_RESERVED ? "Reserved ITM packet" : "Bad ITM packet sequence";
  element.errorMessage += "; " + context;
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
    const auto order = reserveTransactionOrder().value();
    m_transactionElements.push_back(BufferedElement{std::move(element), order, false});
    return;
  }
  appendCommitted(std::move(element));
}

void OpenCsdPacketCollector::appendCommitted(OpenCsdTraceElement element)
{
  accountFormattedCommit(element);
  m_elementSink.append(std::move(element));
}

void OpenCsdPacketCollector::accountFormattedCommit(const OpenCsdTraceElement& element)
{
  const auto found = m_formattedDataByRoute.find(element.route.id);
  if (found == m_formattedDataByRoute.end()) {
    return;
  }
  auto& state = found->second;
  if (element.kind == OpenCsdTraceElement::Kind::Error && element.issueSeverity == TraceIssueSeverity::Error) {
    state.diagnosed = true;
  }
  if (element.kind != OpenCsdTraceElement::Kind::Sync || state.synchronized) {
    return;
  }
  state.synchronized = true;
  if (state.unsynchronizedByteCount == 0U || state.diagnosed || !state.firstFormatterOffset.has_value() ||
      !m_skippedBytesSink) {
    return;
  }
  // Already committing: the input annotation bypasses the transaction being iterated.
  m_skippedBytesSink({*state.firstFormatterOffset, state.unsynchronizedByteCount, TraceByteSkipReason::MissingSync,
                      state.route.traceBusId});
}
