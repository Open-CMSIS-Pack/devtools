/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_DECODE_OPENCSDPACKETCOLLECTOR_H
#define CTRACE_SRC_DECODE_OPENCSDPACKETCOLLECTOR_H

#include "TraceEvent.h"
#include "OpenCsdTraceElement.h"
#include "common/trc_gen_elem.h"
#include "interfaces/trc_gen_elem_in_i.h"
#include "interfaces/trc_pkt_raw_in_i.h"
#include "opencsd/itm/trc_pkt_elem_itm.h"
#include "opencsd/ocsd_if_types.h"
#include "opencsd/trc_gen_elem_types.h"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <string>
#include <vector>

/** @brief Collects OpenCSD callbacks into transactional ctrace elements. */
class OpenCsdPacketCollector : public ITrcGenElemIn, public IPktRawDataMon<ItmTrcPacket> {
public:
  /** @brief Creates a collector that emits committed elements to a sink. */
  explicit OpenCsdPacketCollector(OpenCsdTraceElementSink& elementSink);

  /** @brief Starts buffering elements for one recoverable decoder operation. */
  void beginTransaction();
  /** @brief Commits all buffered elements. */
  void commitTransaction();
  /** @brief Commits buffered elements before a raw source offset. */
  void commitTransactionBefore(std::uint64_t sourceOffset);
  /** @brief Discards all buffered elements. */
  void rollbackTransaction();
  /** @brief Rethrows an exception captured from the downstream sink. */
  void rethrowOutputError();
  /** @brief Returns the number of currently buffered elements. */
  std::size_t transactionElementCount() const;
  /** @brief Returns the first buffered raw offset, if present. */
  std::optional<std::uint64_t> transactionFirstSourceOffset() const;
  /** @brief Appends a decoder issue element. */
  void appendDecodeError(ocsd_trc_index_t index, const std::string& message,
                         const std::string& issueCode = "opencsd-decode-error", bool discontinuity = true,
                         TraceIssueSeverity severity = TraceIssueSeverity::Error);
  /** @brief Prepends a discontinuity before buffered resumed events. */
  void prependDiscontinuity(ocsd_trc_index_t index, const std::string& message, const std::string& issueCode,
                            std::optional<std::uint64_t> rawBytesConsumed = std::nullopt);
  /** @brief Prepends a data-loss error before buffered resumed events. */
  void prependDataLossError(ocsd_trc_index_t index, const std::string& message, std::uint64_t rawBytesConsumed);
  /** @brief Receives one generic element callback from OpenCSD. */
  ocsd_datapath_resp_t TraceElemIn(ocsd_trc_index_t index_sop, std::uint8_t trc_chan_id,
                                   const OcsdTraceElement& elem) override;
  /** @brief Receives one raw ITM packet callback from OpenCSD. */
  void RawPacketDataMon(ocsd_datapath_op_t op, ocsd_trc_index_t index_sop, const ItmTrcPacket* pkt, std::uint32_t size,
                        const std::uint8_t* data) override;

private:
  void appendSync(ocsd_trc_index_t index);
  void appendOverflow(ocsd_trc_index_t index);
  void appendGlobalTimestamp(ocsd_trc_index_t index, std::uint8_t traceBusId, const OcsdTraceElement& elem);
  void appendError(ocsd_trc_index_t index, const ItmTrcPacket& pkt);
  void appendSoftware(ocsd_trc_index_t index, std::uint8_t traceBusId, const OcsdTraceElement& elem);
  void appendDwt(ocsd_trc_index_t index, std::uint8_t traceBusId, const OcsdTraceElement& elem);
  void appendTimestamp(ocsd_trc_index_t index, std::uint8_t traceBusId, const OcsdTraceElement& elem);
  static LocalTimestampRelation timestampRelation(swt_itm_type type);
  void appendElement(OpenCsdTraceElement element);
  void appendCommitted(OpenCsdTraceElement element);

  OpenCsdTraceElementSink& elementSink_;
  bool transactionActive_ = false;
  std::vector<OpenCsdTraceElement> transactionElements_;
  std::exception_ptr outputError_;
};

#endif  // CTRACE_SRC_DECODE_OPENCSDPACKETCOLLECTOR_H
