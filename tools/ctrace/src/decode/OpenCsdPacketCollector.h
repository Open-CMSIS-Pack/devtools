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
#include <utility>
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
   * @param skippedBytesSink Optional observer for skipped unsynchronized ITM payload.
   * @throws std::invalid_argument If the route catalogue is empty, invalid, or ambiguous.
   */
  OpenCsdPacketCollector(std::vector<TraceRouteIdentity> routes, OpenCsdTraceElementSink& elementSink,
                          OpenCsdSkippedBytesSink skippedBytesSink = {});

  /**
   * @brief Starts buffering elements for one recoverable decoder operation.
   *
   * Buffered elements become visible only after a commit, allowing the decoder
   * to discard callbacks produced by an invalid packet sequence.
   */
  void beginTransaction();
  /** @brief Reserves the next callback position in the active transaction. */
  std::optional<std::uint64_t> reserveTransactionOrder() noexcept;
  /** @brief Commits all buffered elements. */
  void commitTransaction();
  /**
   * @brief Commits one operation while discarding unsafe elements from failing routes.
   * @param sourceOffsetsByRoute First unsafe raw offset for every failing route.
   *
   * Elements from unaffected routes and non-error elements before their route's
   * cutoff retain their original callback order. Error elements on a failing
   * route are replaced by the structured OpenCSD diagnostics emitted by the
   * recovery controller.
   */
  void commitTransactionForRouteFailures(const std::map<TraceRouteId, std::uint64_t>& sourceOffsetsByRoute);
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
  /**
   * @brief Returns bounded packet diagnostics from the most recent operation.
   * @param channel OpenCSD channel; absent or zero selects the bound SINGLE route.
   * @param index Exact OpenCSD packet index associated with the logger error.
   *
   * Context survives commit or rollback until the next beginTransaction(), so
   * logger errors can be enriched after unsafe trace elements are discarded.
   */
  std::string packetErrorContext(std::optional<std::uint8_t> channel, std::uint64_t index) const;
  /** @brief Clears packet observations before an operation without a new transaction. */
  void clearPacketErrorContexts() noexcept;
  /** @brief Returns the number of currently buffered elements. */
  std::size_t transactionElementCount() const;
  /**
   * @brief Tests for an error element not explained by the supplied route failures.
   * @param sourceOffsetsByRoute First unsafe raw offset for every failing route.
   *
   * An incomplete end-of-trace packet is always unmatched because it cannot be
   * recovered by resetting and resynchronizing the route.
   */
  bool transactionHasUnmatchedError(const std::map<TraceRouteId, std::uint64_t>& sourceOffsetsByRoute) const;
  /** @brief Returns the first buffered raw offset, if present. */
  std::optional<std::uint64_t> transactionFirstSourceOffset() const;
  /**
   * @brief Returns the first buffered hardware-sync offset for one route.
   * @param route Route whose synchronization is requested.
   * @param beforeOffset Optional exclusive failure boundary.
   */
  std::optional<std::uint64_t>
  transactionFirstSyncOffset(const TraceRouteIdentity& route,
                             std::optional<std::uint64_t> beforeOffset = std::nullopt) const;
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
   * @brief Appends a decoder issue element to one explicit normalized route.
   * @param route Route receiving the diagnostic.
   * @param index Raw source offset associated with the issue.
   * @param message Human-readable diagnostic text.
   * @param issueCode Decoder issue state.
   * @param discontinuity Whether the issue breaks semantic continuity.
   * @param severity Output severity assigned to the issue.
   */
  void appendDecodeError(const TraceRouteIdentity& route, ocsd_trc_index_t index, const std::string& message,
                         TraceIssueCode issueCode = TraceIssueCode::OpenCsdDecodeError, bool discontinuity = true,
                         TraceIssueSeverity severity = TraceIssueSeverity::Error);
  /**
   * @brief Appends an OpenCSD logger diagnostic at its original callback position.
   * @param callbackOrder Position reserved when the logger callback occurred.
   */
  void appendReportedDecodeError(ocsd_trc_index_t index, const std::string& message,
                                 std::optional<std::uint64_t> callbackOrder,
                                 TraceIssueCode issueCode = TraceIssueCode::OpenCsdDecodeError,
                                 bool discontinuity = true, TraceIssueSeverity severity = TraceIssueSeverity::Error);
  /**
   * @brief Appends a routed OpenCSD logger diagnostic at its original callback position.
   * @param route Route receiving the diagnostic.
   * @param callbackOrder Position reserved when the logger callback occurred.
   */
  void appendReportedDecodeError(const TraceRouteIdentity& route, ocsd_trc_index_t index, const std::string& message,
                                 std::optional<std::uint64_t> callbackOrder,
                                 TraceIssueCode issueCode = TraceIssueCode::OpenCsdDecodeError,
                                 bool discontinuity = true, TraceIssueSeverity severity = TraceIssueSeverity::Error);
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
  /**
   * @brief Appends one explicit route's data-loss interval.
   * @param route Route whose unresolved recovery interval is being closed.
   * @param index Raw source offset at which data loss started.
   * @param message Human-readable data-loss description.
   * @param rawBytesConsumed Raw input span covered by the interval.
   */
  void appendDataLossError(const TraceRouteIdentity& route, ocsd_trc_index_t index, const std::string& message,
                           std::uint64_t rawBytesConsumed);
  /**
   * @brief Inserts one route's data-loss interval immediately before its next buffered sync.
   * @param route Route whose recovery interval is being closed.
   * @param index Raw source offset at which data loss started.
   * @param message Human-readable data-loss description.
   * @param rawBytesConsumed Raw input span covered by the interval.
   * @param beforeOffset Optional exclusive failure boundary for an operation that fails again.
   * @return True when a matching retained sync was found and the issue was inserted.
   */
  bool insertDataLossBeforeSync(const TraceRouteIdentity& route, ocsd_trc_index_t index, const std::string& message,
                                std::uint64_t rawBytesConsumed,
                                std::optional<std::uint64_t> beforeOffset = std::nullopt);
  /**
   * @brief Resolves an OpenCSD transport channel to its exact normalized route.
   *
   * SINGLE exposes only its synthetic channel 0. Formatted input exposes only
   * configured architectural Trace Bus IDs.
   */
  const TraceRouteIdentity* routeForChannel(std::uint8_t channel) const noexcept;
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
  /** @brief Records received deformatted bytes independently of ITM packet synchronization. */
  void formattedDataForRoute(const TraceRouteIdentity& route, ocsd_trc_index_t index,
                              std::uint32_t size) override;
  /** @brief Diagnoses received formatted routes that never committed a hardware synchronization. */
  void reportUnsynchronizedFormattedRoutes();

private:
  /** @brief Returns the SINGLE route, or null for a formatted collector. */
  const TraceRouteIdentity* singleRoute() const noexcept;
  /** @brief Returns the deterministic route for an input-wide diagnostic. */
  const TraceRouteIdentity& defaultRoute() const noexcept;
  /** @brief Resolves a generic callback while retaining the fixed SINGLE binding. */
  const TraceRouteIdentity* callbackRouteForChannel(std::uint8_t channel) const noexcept;
  /** @brief Tests whether an explicit packet route belongs to this collector. */
  bool containsRoute(const TraceRouteIdentity& route) const noexcept;
  /** @brief Converts one raw packet callback after its route has been resolved. */
  void appendRawPacket(const TraceRouteIdentity& route, ocsd_datapath_op_t op, ocsd_trc_index_t index_sop,
                       const ItmTrcPacket* pkt, std::uint32_t size, const std::uint8_t* data);
  /** @brief Copies bounded error-packet context without retaining OpenCSD-owned bytes. */
  std::string capturePacketErrorContext(const TraceRouteIdentity& route, ocsd_trc_index_t index,
                                         const ItmTrcPacket& packet, std::uint32_t size, const std::uint8_t* data);
  /** @brief Counts only bytes explicitly discarded during initial formatted-route synchronization. */
  void recordUnsynchronizedBytes(const TraceRouteIdentity& route, std::uint32_t size);
  /** @brief Accounts for committed sync/errors and reports initial skipped bytes before a retained sync. */
  void accountFormattedCommit(const OpenCsdTraceElement& element);
  /** @brief Appends a hardware synchronization element. */
  void appendSync(ocsd_trc_index_t index, const TraceRouteIdentity& route);
  /** @brief Appends a hardware overflow element. */
  void appendOverflow(ocsd_trc_index_t index, const TraceRouteIdentity& route);
  /** @brief Converts an OpenCSD global timestamp callback. */
  void appendGlobalTimestamp(ocsd_trc_index_t index, const OcsdTraceElement& elem, const TraceRouteIdentity& route);
  /** @brief Converts an OpenCSD error packet callback. */
  void appendError(ocsd_trc_index_t index, const ItmTrcPacket& pkt, const TraceRouteIdentity& route,
                     const std::string& context);
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
  /** @brief Builds and appends one ordinary or logger-reported diagnostic. */
  void appendDecodeErrorImpl(const TraceRouteIdentity& route, ocsd_trc_index_t index, const std::string& message,
                             TraceIssueCode issueCode, bool discontinuity, TraceIssueSeverity severity,
                             std::optional<std::uint64_t> callbackOrder, bool reportedDiagnostic);
  /** @brief Emits one committed element while deferring sink exceptions. */
  void appendCommitted(OpenCsdTraceElement element);

  /** @brief Adds transaction-only ordering metadata without exposing it downstream. */
  struct BufferedElement {
    OpenCsdTraceElement element;
    std::uint64_t callbackOrder = 0U;
    bool reportedDiagnostic = false;
  };

  /** @brief Tracks initial stream synchronization without interpreting or retaining raw payload. */
  struct FormattedRouteData {
    TraceRouteIdentity route;
    std::optional<std::uint64_t> firstFormatterOffset;
    std::optional<std::uint64_t> lastFormatterOffset;
    std::uint64_t byteCount = 0U;
    std::uint64_t unsynchronizedByteCount = 0U;
    bool synchronized = false;
    bool diagnosed = false;
  };

  std::optional<TraceRouteIdentity> m_singleRoute;
  std::map<std::uint8_t, TraceRouteIdentity> m_routesByChannel;
  std::map<TraceRouteId, FormattedRouteData> m_formattedDataByRoute;
  OpenCsdTraceElementSink& m_elementSink;
  OpenCsdSkippedBytesSink m_skippedBytesSink;
  bool m_transactionActive = false;
  std::uint64_t m_nextTransactionOrder = 1U;
  std::vector<BufferedElement> m_transactionElements;
  std::map<std::pair<TraceRouteId, std::uint64_t>, std::string> m_packetErrorContexts;
  std::exception_ptr m_outputError;
};

#endif // CTRACE_SRC_DECODE_OPENCSDPACKETCOLLECTOR_H
