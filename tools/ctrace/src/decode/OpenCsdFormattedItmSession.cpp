/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "OpenCsdFormattedItmSession.h"

#include "OpenCsdTreeSession.h"
#include "TraceStreamId.h"
#include "common/trc_gen_elem.h"
#include "interfaces/trc_data_rawframe_in_i.h"
#include "interfaces/trc_error_log_i.h"
#include "interfaces/trc_gen_elem_in_i.h"
#include "interfaces/trc_pkt_raw_in_i.h"
#include "opencsd/itm/trc_cmp_cfg_itm.h"
#include "opencsd/itm/trc_pkt_elem_itm.h"
#include "opencsd/itm/trc_pkt_types_itm.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <optional>
#include <utility>
#include <vector>

namespace {

constexpr std::uint32_t kItmTcrSwoEnable = 1U << 4U;
constexpr ocsd_itm_cfg kItmConfig{kItmTcrSwoEnable};
constexpr std::uint32_t kFormattedTreeFlags = OCSD_DFRMTR_FRAME_MEM_ALIGN | OCSD_DFRMTR_UNPACKED_RAW_OUT;

} // namespace

/** @brief Retains the first exception raised while OpenCSD owns the call stack. */
class OpenCsdFormattedItmSession::CallbackErrorState final {
public:
  /** @brief Captures the active exception without allowing another exception to escape. */
  void captureCurrent() noexcept
  {
    if (!m_error) {
      m_error = std::current_exception();
    }
  }

  /** @brief Rethrows and clears the first captured callback exception. */
  void rethrow()
  {
    if (!m_error) {
      return;
    }
    auto error = std::exchange(m_error, nullptr);
    std::rethrow_exception(error);
  }

private:
  std::exception_ptr m_error;
};

/** @brief Prevents generic-output exceptions from unwinding through OpenCSD. */
class OpenCsdFormattedItmSession::GenericElementAdapter final : public ITrcGenElemIn {
public:
  /** @brief Binds the external generic-element output and shared error state. */
  GenericElementAdapter(ITrcGenElemIn& output, CallbackErrorState& errors)
    : m_output(output),
      m_errors(errors)
  {
  }

  /** @brief Forwards one element while preserving its OpenCSD channel ID. */
  ocsd_datapath_resp_t TraceElemIn(ocsd_trc_index_t index, std::uint8_t channel,
                                   const OcsdTraceElement& element) noexcept override
  {
    try {
      return m_output.TraceElemIn(index, channel, element);
    } catch (...) {
      m_errors.captureCurrent();
      return OCSD_RESP_FATAL_SYS_ERR;
    }
  }

private:
  ITrcGenElemIn& m_output;
  CallbackErrorState& m_errors;
};

/** @brief Restores route identity omitted by OpenCSD's raw-packet monitor interface. */
class OpenCsdFormattedItmSession::RoutePacketMonitor final : public IPktRawDataMon<ItmTrcPacket> {
public:
  /** @brief Binds one decoder route to the shared packet sink. */
  RoutePacketMonitor(TraceRouteIdentity route, OpenCsdFormattedItmPacketSink& sink, CallbackErrorState& errors)
    : m_route(std::move(route)),
      m_sink(sink),
      m_errors(errors)
  {
  }

  /** @brief Forwards one packet with its bound route without throwing through OpenCSD. */
  void RawPacketDataMon(ocsd_datapath_op_t operation, ocsd_trc_index_t index, const ItmTrcPacket* packet,
                        std::uint32_t size, const std::uint8_t* data) noexcept override
  {
    try {
      m_sink.rawPacketForRoute(m_route, operation, index, packet, size, data);
    } catch (...) {
      m_errors.captureCurrent();
    }
  }

private:
  TraceRouteIdentity m_route;
  OpenCsdFormattedItmPacketSink& m_sink;
  CallbackErrorState& m_errors;
};

/** @brief Accounts for routed input and payload skipped by the formatter. */
class OpenCsdFormattedItmSession::RawFrameMonitor final : public ITrcRawFrameIn {
public:
  /** @brief Indexes all configured normal source IDs. */
  RawFrameMonitor(const std::vector<TraceRouteIdentity>& routes, OpenCsdFormattedItmPacketSink& sink,
                    CallbackErrorState& errors)
    : m_sink(sink),
      m_errors(errors)
  {
    for (const auto& route : routes) {
      m_routes[*route.traceBusId] = &route;
    }
  }

  /** @brief Observes deformatter output without allowing callback exceptions to escape into OpenCSD. */
  ocsd_err_t TraceRawFrameIn(ocsd_datapath_op_t operation, ocsd_trc_index_t index,
                             ocsd_rawframe_elem_t frameElement, int dataSize, const std::uint8_t*,
                             std::uint8_t traceId) noexcept override
  {
    if (operation != OCSD_OP_DATA || frameElement != OCSD_FRM_ID_DATA || dataSize <= 0) {
      return OCSD_OK;
    }
    if (traceId == OCSD_BAD_CS_SRC_ID) {
      // No frontend reset or reset-on-FSYNC: an unknown ID can only precede the first assigned ID.
      m_prefix.record(index, static_cast<std::uint32_t>(dataSize));
      return OCSD_OK;
    }
    m_hasAssignedData = true;
    if (!OCSD_IS_VALID_CS_SRC_ID(traceId)) {
      recordSkippedSource(traceId, index, static_cast<std::uint32_t>(dataSize));
      return OCSD_OK;
    }
    if (m_routes[traceId] != nullptr) {
      try {
        m_sink.formattedDataForRoute(*m_routes[traceId], index, static_cast<std::uint32_t>(dataSize));
      } catch (...) {
        m_errors.captureCurrent();
      }
    } else {
      recordSkippedSource(traceId, index, static_cast<std::uint32_t>(dataSize));
      if (!m_reported[traceId] && !m_pendingIndex[traceId].has_value()) {
        m_pendingIndex[traceId] = index;
      }
    }
    return OCSD_OK;
  }

  /** @brief Publishes observations outside OpenCSD after each operation. */
  void completeOperation(const OpenCsdUnsupportedTraceIdSink& unsupportedTraceIdSink,
                           const OpenCsdSkippedBytesSink& skippedBytesSink, bool endOfTrace)
  {
    if (m_hasAssignedData || endOfTrace) {
      m_prefix.publish(TraceByteSkipReason::NoSourceId, std::nullopt, skippedBytesSink);
    }
    publishUnsupportedIds(unsupportedTraceIdSink);
    if (endOfTrace) {
      publishSkippedSources(skippedBytesSink);
    }
  }

private:
  /** @brief Retains bounded accounting without storing protocol bytes. */
  struct SkippedDataCounter {
    std::optional<ocsd_trc_index_t> firstFormatterOffset;
    std::uint64_t byteCount = 0U;

    /** @brief Counts a skipped group; OpenCSD advances these unconnected groups without retrying them. */
    void record(ocsd_trc_index_t index, std::uint32_t size) noexcept
    {
      if (!firstFormatterOffset.has_value()) {
        firstFormatterOffset = index;
      }
      byteCount += size;
    }

    /** @brief Publishes one aggregate without repeating it if the observer throws or EOT repeats. */
    void publish(TraceByteSkipReason reason, std::optional<std::uint8_t> traceId,
                   const OpenCsdSkippedBytesSink& sink)
    {
      if (!firstFormatterOffset.has_value()) {
        return;
      }
      const TraceByteSkip skipped{static_cast<std::uint64_t>(*firstFormatterOffset), byteCount, reason, traceId};
      firstFormatterOffset.reset();
      byteCount = 0U;
      if (sink) {
        sink(skipped);
      }
    }
  };

  /** @brief Publishes the existing once-per-ID compatibility warning outside the OpenCSD call stack. */
  void publishUnsupportedIds(const OpenCsdUnsupportedTraceIdSink& sink)
  {
    for (std::uint8_t traceId = CoreSight::kMinAtbTraceId; traceId <= CoreSight::kMaxAtbTraceId; ++traceId) {
      if (!m_pendingIndex[traceId].has_value()) {
        continue;
      }
      const auto index = *std::exchange(m_pendingIndex[traceId], std::nullopt);
      m_reported[traceId] = true;
      if (sink) {
        sink(traceId, index);
      }
    }
  }

  /** @brief Counts NULL, reserved, or unconfigured source payload without formatter-control bytes. */
  void recordSkippedSource(std::uint8_t traceId, ocsd_trc_index_t index, std::uint32_t size) noexcept
  {
    if (traceId < m_skippedById.size()) {
      m_skippedById[traceId].record(index, size);
    }
  }

  /** @brief Reports every observed skipped source once at end-of-input. */
  void publishSkippedSources(const OpenCsdSkippedBytesSink& sink)
  {
    for (std::size_t traceId = 0U; traceId < m_skippedById.size(); ++traceId) {
      const auto reason = traceId == 0U ? TraceByteSkipReason::NullSourceId
                          : OCSD_IS_VALID_CS_SRC_ID(traceId) ? TraceByteSkipReason::UnconfiguredSourceId
                                                           : TraceByteSkipReason::ReservedSourceId;
      m_skippedById[traceId].publish(reason, static_cast<std::uint8_t>(traceId), sink);
    }
  }

  OpenCsdFormattedItmPacketSink& m_sink;
  CallbackErrorState& m_errors;
  std::array<const TraceRouteIdentity*, 128U> m_routes{};
  std::array<bool, 128U> m_reported{};
  std::array<std::optional<ocsd_trc_index_t>, 128U> m_pendingIndex{};
  std::array<SkippedDataCounter, 128U> m_skippedById{};
  SkippedDataCounter m_prefix;
  bool m_hasAssignedData = false;
};

std::vector<TraceRouteIdentity>
OpenCsdFormattedItmSession::validateRoutes(std::vector<TraceRouteIdentity>&& routes)
{
  if (routes.empty()) {
    throw OpenCsdItmSessionError("formatted OpenCSD ITM session requires at least one route");
  }
  std::array<bool, 128U> configured{};
  for (const auto& route : routes) {
    if (!route.traceBusId.has_value() || !CoreSight::isAtbTraceId(*route.traceBusId)) {
      throw OpenCsdItmSessionError("formatted OpenCSD ITM route requires a Trace Bus ID between 1 and 111");
    }
    if (configured[*route.traceBusId]) {
      throw OpenCsdItmSessionError("formatted OpenCSD ITM routes require unique Trace Bus IDs");
    }
    configured[*route.traceBusId] = true;
  }
  return std::move(routes);
}

OpenCsdFormattedItmSession::OpenCsdFormattedItmSession(std::vector<TraceRouteIdentity> routes,
                                                       ITrcGenElemIn& elementOutput, ITraceErrorLog& errorLogger,
                                                       OpenCsdFormattedItmPacketSink& packetSink,
                                                       OpenCsdUnsupportedTraceIdSink unsupportedTraceIdSink,
                                                       OpenCsdSkippedBytesSink skippedBytesSink)
  : m_routes(validateRoutes(std::move(routes))),
    m_unsupportedTraceIdSink(std::move(unsupportedTraceIdSink)),
    m_skippedBytesSink(std::move(skippedBytesSink)),
    m_callbackErrors(std::make_unique<CallbackErrorState>()),
    m_elementAdapter(std::make_unique<GenericElementAdapter>(elementOutput, *m_callbackErrors)),
    m_rawFrameMonitor(std::make_unique<RawFrameMonitor>(m_routes, packetSink, *m_callbackErrors)),
    m_treeSession(OCSD_TRC_SRC_FRAME_FORMATTED, kFormattedTreeFlags, errorLogger, *m_elementAdapter)
{
  // OpenCSD reaches these overrides only through interfaces implemented in the
  // external library. Retain the concrete callback identities at the binding site.
  const auto packetCallback = &RoutePacketMonitor::RawPacketDataMon;
  const auto rawFrameCallback = &RawFrameMonitor::TraceRawFrameIn;
  (void)packetCallback;
  (void)rawFrameCallback;

  m_treeSession.attachRawFrameMonitor(*m_rawFrameMonitor);
  m_packetMonitors.reserve(m_routes.size());
  for (const auto& route : m_routes) {
    m_packetMonitors.push_back(std::make_unique<RoutePacketMonitor>(route, packetSink, *m_callbackErrors));
    ITMConfig config(&kItmConfig);
    config.setTraceID(*route.traceBusId);
    m_treeSession.createDecoder(OCSD_BUILTIN_DCD_ITM, OCSD_CREATE_FLG_FULL_DECODER, config);
    m_treeSession.attachDecoderCallbacks(*route.traceBusId, *m_packetMonitors.back());
  }
}

OpenCsdFormattedItmSession::~OpenCsdFormattedItmSession() noexcept = default;

ocsd_datapath_resp_t OpenCsdFormattedItmSession::pushData(ocsd_trc_index_t index, std::uint32_t size,
                                                          const std::uint8_t* data, std::uint32_t& processed)
{
  const auto response = m_treeSession.traceDataIn(OCSD_OP_DATA, index, size, data, &processed);
  m_receivedInput = m_receivedInput || processed > 0U;
  return completeOperation(response);
}

ocsd_datapath_resp_t OpenCsdFormattedItmSession::flush()
{
  // OpenCSD's formatted frontend has no initialized frame to flush until it
  // has consumed input. Avoid entering that external undefined state.
  if (!m_receivedInput) {
    return OCSD_RESP_CONT;
  }
  return completeOperation(m_treeSession.traceDataIn(OCSD_OP_FLUSH, 0, 0, nullptr, nullptr));
}

ocsd_datapath_resp_t OpenCsdFormattedItmSession::resetRoute(std::uint8_t channel, ocsd_trc_index_t index)
{
  return completeOperation(m_treeSession.resetDecoder(channel, index));
}

ocsd_datapath_resp_t OpenCsdFormattedItmSession::endOfTrace()
{
  return completeOperation(m_treeSession.traceDataIn(OCSD_OP_EOT, 0, 0, nullptr, nullptr), true);
}

ocsd_datapath_resp_t OpenCsdFormattedItmSession::completeOperation(ocsd_datapath_resp_t response, bool endOfTrace)
{
  m_callbackErrors->rethrow();
  m_rawFrameMonitor->completeOperation(m_unsupportedTraceIdSink, m_skippedBytesSink, endOfTrace);
  return response;
}
