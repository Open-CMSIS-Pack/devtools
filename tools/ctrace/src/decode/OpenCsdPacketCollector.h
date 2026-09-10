/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_DECODE_OPENCSDPACKETCOLLECTOR_H
#define CTRACE_SRC_DECODE_OPENCSDPACKETCOLLECTOR_H

#include "OpenCsdFormattedItmSession.h"
#include "TraceEvent.h"
#include "OpenCsdTraceElement.h"
#include "TraceRoute.h"
#include "common/trc_gen_elem.h"
#include "interfaces/trc_gen_elem_in_i.h"
#include "interfaces/trc_pkt_raw_in_i.h"
#include "opencsd/itm/trc_pkt_elem_itm.h"
#include "opencsd/ocsd_if_types.h"
#include "opencsd/trc_gen_elem_types.h"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <map>
#include <optional>
#include <string>
#include <vector>

/** @brief Collects OpenCSD callbacks into transactional ctrace elements. */
class OpenCsdPacketCollector : public ITrcGenElemIn,
                               public IPktRawDataMon<ItmTrcPacket>,
                               public OpenCsdFormattedItmPacketSink {
public:
  /**
   * @brief Creates a collector that emits committed elements to a sink.
   * @param route Normalized semantic route assigned to every collected element.
   * @param elementSink Sink receiving elements after transaction commit.
   */
  OpenCsdPacketCollector(TraceRouteIdentity route, OpenCsdTraceElementSink& elementSink);
  /**
   * @brief Creates a collector that routes formatted callbacks by Trace Bus ID.
   * @param routes Normalized routes, each carrying one unique architectural Trace Bus ID.
   * @param elementSink Sink receiving elements after transaction commit.
   * @throws std::invalid_argument If the route catalogue is empty, invalid, or ambiguous.
   */
  OpenCsdPacketCollector(std::vector<TraceRouteIdentity> routes, OpenCsdTraceElementSink& elementSink);

  /**
   * @brief Starts buffering elements for one recoverable decoder operation.
   *
   * Buffered elements become visible only after a commit, allowing the decoder
   * to discard callbacks produced by an invalid packet sequence.
   */
  void beginTransaction();
  /** @brief Commits all buffered elements. */
  void commitTransaction();
  /**
   * @brief Commits only matching failing issues and discards every other buffered element.
   * @param issueCode Issue code retained from the current transaction.
   * @return Number of retained issues.
   */
  std::size_t commitTransactionErrors(TraceIssueCode issueCode);
  /**
   * @brief Commits buffered elements before a raw source offset.
   * @param sourceOffset First raw offset that remains buffered.
   */
  void commitTransactionBefore(std::uint64_t sourceOffset);
  /** @brief Discards all buffered elements. */
  void rollbackTransaction();
  /** @brief Rethrows an exception captured from the downstream sink. */
  void rethrowOutputError();
  /** @brief Returns the number of currently buffered elements. */
  std::size_t transactionElementCount() const;
  /** @brief Tests whether the current transaction contains a failing diagnostic. */
  bool transactionHasError() const;
  /** @brief Returns the first buffered raw offset, if present. */
  std::optional<std::uint64_t> transactionFirstSourceOffset() const;
  /**
   * @brief Appends a decoder issue element.
   * @param index Raw source offset associated with the issue.
   * @param message Human-readable diagnostic text.
   * @param issueCode Decoder issue state.
   * @param discontinuity Whether the issue breaks semantic continuity.
   * @param severity Output severity assigned to the issue.
   */
  void appendDecodeError(ocsd_trc_index_t index, const std::string& message,
                         TraceIssueCode issueCode = TraceIssueCode::OpenCsdDecodeError, bool discontinuity = true,
                         TraceIssueSeverity severity = TraceIssueSeverity::Error);
  /**
   * @brief Prepends a discontinuity before buffered resumed events.
   * @param index Raw source offset at which decoding resumes.
   * @param message Human-readable recovery description.
   * @param issueCode Decoder issue state.
   * @param rawBytesConsumed Number of discarded bytes, when known.
   */
  void prependDiscontinuity(ocsd_trc_index_t index, const std::string& message, TraceIssueCode issueCode,
                            std::optional<std::uint64_t> rawBytesConsumed = std::nullopt);
  /**
   * @brief Prepends a data-loss error before buffered resumed events.
   * @param index Raw source offset at which decoding resumes.
   * @param message Human-readable data-loss description.
   * @param rawBytesConsumed Number of discarded raw bytes.
   */
  void prependDataLossError(ocsd_trc_index_t index, const std::string& message, std::uint64_t rawBytesConsumed);
  /** @brief Receives one generic element callback from OpenCSD. */
  ocsd_datapath_resp_t TraceElemIn(ocsd_trc_index_t index_sop, std::uint8_t trc_chan_id,
                                   const OcsdTraceElement& elem) override;
  /** @brief Receives one raw ITM packet callback from OpenCSD. */
  void RawPacketDataMon(ocsd_datapath_op_t op, ocsd_trc_index_t index_sop, const ItmTrcPacket* pkt, std::uint32_t size,
                        const std::uint8_t* data) override;
  /**
   * @brief Receives a raw ITM packet from a decoder adapter bound to one route.
   * @param route Exact normalized route bound to the decoder callback.
   * @param op OpenCSD data-path operation.
   * @param index_sop Raw input offset at the start of the packet.
   * @param pkt Decoded ITM packet, or null for an operation-only callback.
   * @param size Number of raw packet bytes.
   * @param data Raw packet bytes.
   */
  void rawPacketForRoute(const TraceRouteIdentity& route, ocsd_datapath_op_t op, ocsd_trc_index_t index_sop,
                         const ItmTrcPacket* pkt, std::uint32_t size, const std::uint8_t* data) override;

private:
  /** @brief Returns the SINGLE route, or null for a formatted collector. */
  const TraceRouteIdentity* singleRoute() const noexcept;
  /** @brief Returns the deterministic route for an input-wide diagnostic. */
  const TraceRouteIdentity& defaultRoute() const noexcept;
  /** @brief Resolves a generic callback channel to its normalized route. */
  const TraceRouteIdentity* routeForChannel(std::uint8_t channel) const noexcept;
  /** @brief Tests whether an explicit packet route belongs to this collector. */
  bool containsRoute(const TraceRouteIdentity& route) const noexcept;
  /** @brief Converts one raw packet callback after its route has been resolved. */
  void appendRawPacket(const TraceRouteIdentity& route, ocsd_datapath_op_t op, ocsd_trc_index_t index_sop,
                       const ItmTrcPacket* pkt);
  /** @brief Appends a hardware synchronization element. */
  void appendSync(ocsd_trc_index_t index, const TraceRouteIdentity& route);
  /** @brief Appends a hardware overflow element. */
  void appendOverflow(ocsd_trc_index_t index, const TraceRouteIdentity& route);
  /** @brief Converts an OpenCSD global timestamp callback. */
  void appendGlobalTimestamp(ocsd_trc_index_t index, const OcsdTraceElement& elem, const TraceRouteIdentity& route);
  /** @brief Converts an OpenCSD error packet callback. */
  void appendError(ocsd_trc_index_t index, const ItmTrcPacket& pkt, const TraceRouteIdentity& route);
  /** @brief Converts an ITM software packet callback. */
  void appendSoftware(ocsd_trc_index_t index, const OcsdTraceElement& elem, const TraceRouteIdentity& route);
  /** @brief Converts a DWT hardware packet callback. */
  void appendDwt(ocsd_trc_index_t index, const OcsdTraceElement& elem, const TraceRouteIdentity& route);
  /** @brief Converts an OpenCSD local timestamp callback. */
  void appendTimestamp(ocsd_trc_index_t index, const OcsdTraceElement& elem, const TraceRouteIdentity& route);
  /** @brief Maps the OpenCSD timestamp type to its semantic relation. */
  static LocalTimestampRelation timestampRelation(swt_itm_type type);
  /** @brief Buffers or commits one element according to transaction state. */
  void appendElement(OpenCsdTraceElement element, const TraceRouteIdentity& route);
  /** @brief Emits one committed element while deferring sink exceptions. */
  void appendCommitted(OpenCsdTraceElement element);

  std::optional<TraceRouteIdentity> m_singleRoute;
  std::map<std::uint8_t, TraceRouteIdentity> m_routesByChannel;
  OpenCsdTraceElementSink& m_elementSink;
  bool m_transactionActive = false;
  std::vector<OpenCsdTraceElement> m_transactionElements;
  std::exception_ptr m_outputError;
};

#endif // CTRACE_SRC_DECODE_OPENCSDPACKETCOLLECTOR_H
