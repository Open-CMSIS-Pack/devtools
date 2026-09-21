/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_DECODE_OPENCSDFORMATTEDITMSESSION_H
#define CTRACE_SRC_DECODE_OPENCSDFORMATTEDITMSESSION_H

#include "OpenCsdItmSession.h"
#include "TraceEvent.h"
#include "TraceRoute.h"
#include "opencsd/ocsd_if_types.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

class ITraceErrorLog;
class ITrcGenElemIn;
class ItmTrcPacket;

/** @brief Receives raw ITM packets with the normalized route omitted by OpenCSD's monitor API. */
class OpenCsdFormattedItmPacketSink {
public:
  /** @brief Destroys a routed packet sink through its interface. */
  virtual ~OpenCsdFormattedItmPacketSink() = default;

  /**
   * @brief Receives one raw ITM packet callback for its configured route.
   * @param route Normalized route bound to the decoder producing the callback.
   * @param operation OpenCSD data-path operation.
   * @param index Raw formatted-input offset associated with the packet.
   * @param packet Expanded ITM packet, or null for a control operation.
   * @param size Number of raw protocol bytes associated with the packet.
   * @param data Raw protocol bytes, or null when no bytes are supplied.
   */
  virtual void rawPacketForRoute(const TraceRouteIdentity& route, ocsd_datapath_op_t operation, ocsd_trc_index_t index,
                                 const ItmTrcPacket* packet, std::uint32_t size, const std::uint8_t* data) = 0;

  /**
   * @brief Observes received deformatted bytes before delivery to a configured ITM route.
   *
   * The index identifies the raw formatter group, not an exact payload-byte offset.
   * The size counts protocol bytes, including potential ITM synchronization bytes.
   * Packet sinks that do not need input accounting may leave this callback unimplemented.
   */
  virtual void formattedDataForRoute(const TraceRouteIdentity&, ocsd_trc_index_t, std::uint32_t)
  {
  }
};

/** @brief Reports one normal formatter source ID for which no protocol route is configured. */
using OpenCsdUnsupportedTraceIdSink = std::function<void(std::uint8_t, ocsd_trc_index_t)>;

/** @brief Reports skipped deformatted bytes with their reason and first formatter-group offset. */
using OpenCsdSkippedBytesSink = std::function<void(const TraceByteSkip&)>;

/**
 * @brief Owns one memory-aligned formatted OpenCSD tree with routed ITM decoders.
 *
 * Feed, response, transaction, and recovery policy remains outside this low-level
 * session. Callback exceptions are never allowed to unwind through OpenCSD.
 */
class OpenCsdFormattedItmSession final : public OpenCsdItmSessionInterface {
public:
  /**
   * @brief Creates one full ITM decoder for every supplied formatted route.
   * @param routes Normalized routes, each with one unique Trace Bus ID in 1..111.
   * @param elementOutput Tree-wide generic-element output receiving OpenCSD channel IDs.
   * @param errorLogger Error logger kept active for the complete tree lifetime.
   * @param packetSink Routed raw-packet callback target shared by all decoder adapters.
   * @param unsupportedTraceIdSink Optional callback invoked once per observed unconfigured normal ID.
   * @param skippedBytesSink Optional callback reporting skipped payload bytes aggregated by reason and source ID.
   * @throws OpenCsdItmSessionError If route validation or external session setup fails.
   */
  OpenCsdFormattedItmSession(std::vector<TraceRouteIdentity> routes, ITrcGenElemIn& elementOutput,
                             ITraceErrorLog& errorLogger, OpenCsdFormattedItmPacketSink& packetSink,
                             OpenCsdUnsupportedTraceIdSink unsupportedTraceIdSink = {},
                             OpenCsdSkippedBytesSink skippedBytesSink = {});
  /** @brief Disconnects callbacks and destroys the formatted DecodeTree without throwing. */
  ~OpenCsdFormattedItmSession() noexcept;

  /** @brief Disables copying because a session owns external decoder state. */
  OpenCsdFormattedItmSession(const OpenCsdFormattedItmSession&) = delete;
  /** @brief Disables copy assignment because a session owns external decoder state. */
  OpenCsdFormattedItmSession& operator=(const OpenCsdFormattedItmSession&) = delete;

  /** @brief Pushes complete memory-aligned formatter frames into OpenCSD. */
  ocsd_datapath_resp_t pushData(ocsd_trc_index_t index, std::uint32_t size, const std::uint8_t* data,
                                std::uint32_t& processed) override;
  /** @brief Flushes pending decoder and deformatter work. */
  ocsd_datapath_resp_t flush() override;
  /** @brief Resets one ITM decoder pair while preserving deformatter state. */
  ocsd_datapath_resp_t resetRoute(std::uint8_t channel, ocsd_trc_index_t index) override;
  /** @brief Signals end of trace to every configured decoder. */
  ocsd_datapath_resp_t endOfTrace() override;

private:
  class CallbackErrorState;
  class GenericElementAdapter;
  class RoutePacketMonitor;
  class RawFrameMonitor;

  /** @brief Validates route IDs before any OpenCSD process-global state is acquired. */
  static std::vector<TraceRouteIdentity> validateRoutes(std::vector<TraceRouteIdentity>&& routes);
  /** @brief Rethrows callback failures and publishes observations after one tree operation. */
  ocsd_datapath_resp_t completeOperation(ocsd_datapath_resp_t response, bool endOfTrace = false);

  // Declaration order is intentional: the tree is destroyed before every
  // callback object whose address was installed in it.
  std::vector<TraceRouteIdentity> m_routes;
  OpenCsdUnsupportedTraceIdSink m_unsupportedTraceIdSink;
  OpenCsdSkippedBytesSink m_skippedBytesSink;
  std::unique_ptr<CallbackErrorState> m_callbackErrors;
  std::unique_ptr<GenericElementAdapter> m_elementAdapter;
  std::vector<std::unique_ptr<RoutePacketMonitor>> m_packetMonitors;
  std::unique_ptr<RawFrameMonitor> m_rawFrameMonitor;
  OpenCsdTreeSession m_treeSession;
  bool m_receivedInput = false;
};

#endif // CTRACE_SRC_DECODE_OPENCSDFORMATTEDITMSESSION_H
