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
#include <cstdint>
#include <exception>
#include <optional>
#include <string>
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

/** @brief Observes deformatter IDs that have no configured protocol decoder. */
class OpenCsdFormattedItmSession::RawFrameMonitor final : public ITrcRawFrameIn {
public:
  /** @brief Indexes all configured normal source IDs. */
  explicit RawFrameMonitor(const std::vector<TraceRouteIdentity>& routes)
  {
    for (const auto& route : routes) {
      m_configured[*route.traceBusId] = true;
    }
  }

  /** @brief Records unsupported or unassigned deformatter output without external calls. */
  ocsd_err_t TraceRawFrameIn(ocsd_datapath_op_t operation, ocsd_trc_index_t index, ocsd_rawframe_elem_t frameElement,
                             int dataSize, const std::uint8_t*, std::uint8_t traceId) noexcept override
  {
    if (operation != OCSD_OP_DATA || frameElement != OCSD_FRM_ID_DATA || dataSize <= 0) {
      return OCSD_OK;
    }
    if (traceId == OCSD_BAD_CS_SRC_ID) {
      if (!m_unassignedIndex.has_value()) {
        m_unassignedIndex = index;
      }
      return OCSD_OK;
    }
    if (!OCSD_IS_VALID_CS_SRC_ID(traceId) || m_configured[traceId] || m_reported[traceId] ||
        m_pendingIndex[traceId].has_value()) {
      return OCSD_OK;
    }
    m_pendingIndex[traceId] = index;
    return OCSD_OK;
  }

  /** @brief Publishes new unsupported IDs and rejects data with no preceding source ID. */
  void completeOperation(const OpenCsdUnsupportedTraceIdSink& unsupportedTraceIdSink)
  {
    if (m_unassignedIndex.has_value()) {
      const auto index = *std::exchange(m_unassignedIndex, std::nullopt);
      throw OpenCsdFormattedInputError("formatted trace data has no source ID", static_cast<std::uint64_t>(index));
    }
    for (std::uint8_t traceId = CoreSight::kMinAtbTraceId; traceId <= CoreSight::kMaxAtbTraceId; ++traceId) {
      if (!m_pendingIndex[traceId].has_value()) {
        continue;
      }
      const auto index = *std::exchange(m_pendingIndex[traceId], std::nullopt);
      m_reported[traceId] = true;
      if (unsupportedTraceIdSink) {
        unsupportedTraceIdSink(traceId, index);
      }
    }
  }

private:
  std::array<bool, 128U> m_configured{};
  std::array<bool, 128U> m_reported{};
  std::array<std::optional<ocsd_trc_index_t>, 128U> m_pendingIndex{};
  std::optional<ocsd_trc_index_t> m_unassignedIndex;
};

std::vector<TraceRouteIdentity> OpenCsdFormattedItmSession::validateRoutes(std::vector<TraceRouteIdentity> routes)
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
  return routes;
}

OpenCsdFormattedItmSession::OpenCsdFormattedItmSession(std::vector<TraceRouteIdentity> routes,
                                                       ITrcGenElemIn& elementOutput, ITraceErrorLog& errorLogger,
                                                       OpenCsdFormattedItmPacketSink& packetSink,
                                                       OpenCsdUnsupportedTraceIdSink unsupportedTraceIdSink)
  : m_routes(validateRoutes(std::move(routes))),
    m_unsupportedTraceIdSink(std::move(unsupportedTraceIdSink)),
    m_callbackErrors(std::make_unique<CallbackErrorState>()),
    m_elementAdapter(std::make_unique<GenericElementAdapter>(elementOutput, *m_callbackErrors)),
    m_rawFrameMonitor(std::make_unique<RawFrameMonitor>(m_routes)),
    m_treeSession(OCSD_TRC_SRC_FRAME_FORMATTED, kFormattedTreeFlags, errorLogger, *m_elementAdapter)
{
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
  return completeOperation(m_treeSession.traceDataIn(OCSD_OP_DATA, index, size, data, &processed));
}

ocsd_datapath_resp_t OpenCsdFormattedItmSession::flush()
{
  return completeOperation(m_treeSession.traceDataIn(OCSD_OP_FLUSH, 0, 0, nullptr, nullptr));
}

ocsd_datapath_resp_t OpenCsdFormattedItmSession::reset()
{
  return completeOperation(m_treeSession.traceDataIn(OCSD_OP_RESET, 0, 0, nullptr, nullptr));
}

ocsd_datapath_resp_t OpenCsdFormattedItmSession::resetRoute(std::uint8_t channel, ocsd_trc_index_t index)
{
  return completeOperation(m_treeSession.resetDecoder(channel, index));
}

ocsd_datapath_resp_t OpenCsdFormattedItmSession::endOfTrace()
{
  return completeOperation(m_treeSession.traceDataIn(OCSD_OP_EOT, 0, 0, nullptr, nullptr));
}

ocsd_datapath_resp_t OpenCsdFormattedItmSession::completeOperation(ocsd_datapath_resp_t response)
{
  m_callbackErrors->rethrow();
  m_rawFrameMonitor->completeOperation(m_unsupportedTraceIdSink);
  return response;
}
