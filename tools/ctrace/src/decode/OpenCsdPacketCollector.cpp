/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "OpenCsdPacketCollector.h"

#include "TraceEvent.h"
#include "TraceStreamId.h"
#include "OpenCsdTraceElement.h"
#include "common/trc_gen_elem.h"
#include "opencsd/itm/trc_pkt_elem_itm.h"
#include "opencsd/itm/trc_pkt_types_itm.h"
#include "opencsd/ocsd_if_types.h"
#include "opencsd/trc_gen_elem_types.h"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <optional>
#include <string>
#include <utility>
#include <vector>

OpenCsdPacketCollector::OpenCsdPacketCollector(OpenCsdTraceElementSink& elementSink)
  : m_elementSink(elementSink)
{
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
  OpenCsdTraceElement element;
  element.kind = OpenCsdTraceElement::Kind::Error;
  element.sourceIndex = static_cast<std::uint64_t>(index);
  element.discontinuity = discontinuity;
  element.issueCode = issueCode;
  element.issueSeverity = severity;
  element.errorMessage = message;
  appendElement(std::move(element));
}

void OpenCsdPacketCollector::prependDiscontinuity(ocsd_trc_index_t index, const std::string& message,
                                                  TraceIssueCode issueCode,
                                                  std::optional<std::uint64_t> rawBytesConsumed)
{
  OpenCsdTraceElement element;
  element.kind = OpenCsdTraceElement::Kind::Discontinuity;
  element.sourceIndex = static_cast<std::uint64_t>(index);
  element.discontinuity = true;
  element.issueCode = issueCode;
  element.errorMessage = message;
  element.rawBytesConsumed = rawBytesConsumed;
  if (m_transactionActive) {
    m_transactionElements.insert(m_transactionElements.begin(), std::move(element));
    return;
  }
  appendElement(std::move(element));
}

void OpenCsdPacketCollector::prependDataLossError(ocsd_trc_index_t index, const std::string& message,
                                                  std::uint64_t rawBytesConsumed)
{
  OpenCsdTraceElement element;
  element.kind = OpenCsdTraceElement::Kind::Error;
  element.sourceIndex = static_cast<std::uint64_t>(index);
  element.issueCode = TraceIssueCode::DataLoss;
  element.errorMessage = message;
  element.rawBytesConsumed = rawBytesConsumed;
  element.awaitingResumeTimestamp = true;
  if (m_transactionActive) {
    m_transactionElements.insert(m_transactionElements.begin(), std::move(element));
    return;
  }
  appendElement(std::move(element));
}

ocsd_datapath_resp_t OpenCsdPacketCollector::TraceElemIn(const ocsd_trc_index_t index_sop,
                                                         const std::uint8_t trc_chan_id, const OcsdTraceElement& elem)
{
  try {
    if (elem.getType() != OCSD_GEN_TRC_ELEM_ITMTRACE) {
      return OCSD_RESP_CONT;
    }

    const auto& info = elem.swt_itm;
    const auto traceBusId = CoreSight::isTraceBusId(trc_chan_id) ? trc_chan_id : CoreSight::kUnformattedTraceBusId;
    switch (info.pkt_type) {
    case SWIT_PAYLOAD:
      appendSoftware(index_sop, traceBusId, elem);
      break;
    case DWT_PAYLOAD:
      appendDwt(index_sop, traceBusId, elem);
      break;
    case TS_SYNC:
    case TS_DELAY:
    case TS_PKT_DELAY:
    case TS_PKT_TS_DELAY:
      appendTimestamp(index_sop, traceBusId, elem);
      break;
    case TS_GLOBAL:
      appendGlobalTimestamp(index_sop, traceBusId, elem);
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
                                              const ItmTrcPacket* pkt, const std::uint32_t, const std::uint8_t*)
{
  try {
    if (pkt == nullptr) {
      return;
    }

    // OpenCSD publishes the incomplete packet through the raw monitor as DATA
    // while processing EOT; its following EOT monitor notification has no packet.
    if (pkt->getPktType() == ITM_PKT_INCOMPLETE_EOT) {
      appendDecodeError(index_sop, "incomplete ITM packet at end of input", TraceIssueCode::OpenCsdIncompleteTail, true,
                        TraceIssueSeverity::Error);
      return;
    }
    if (op != OCSD_OP_DATA) {
      return;
    }

    switch (pkt->getPktType()) {
    case ITM_PKT_ASYNC:
      appendSync(index_sop);
      break;
    case ITM_PKT_OVERFLOW:
      appendOverflow(index_sop);
      break;
    case ITM_PKT_TS_GLOBAL_1:
    case ITM_PKT_TS_GLOBAL_2:
      // The ITM decoder combines GTS1/GTS2 and publishes the complete
      // 64-bit value as a generic TS_GLOBAL element. Raw fragments are
      // intentionally not forwarded as independent timestamps.
      break;
    case ITM_PKT_BAD_SEQUENCE:
    case ITM_PKT_RESERVED:
      appendError(index_sop, *pkt);
      break;
    default:
      break;
    }
  } catch (...) {
    if (!m_outputError) {
      m_outputError = std::current_exception();
    }
  }
}

void OpenCsdPacketCollector::appendSync(ocsd_trc_index_t index)
{
  OpenCsdTraceElement element;
  element.kind = OpenCsdTraceElement::Kind::Sync;
  element.sourceIndex = static_cast<std::uint64_t>(index);
  appendElement(std::move(element));
}

void OpenCsdPacketCollector::appendOverflow(ocsd_trc_index_t index)
{
  OpenCsdTraceElement element;
  element.kind = OpenCsdTraceElement::Kind::Overflow;
  element.sourceIndex = static_cast<std::uint64_t>(index);
  appendElement(std::move(element));
}

void OpenCsdPacketCollector::appendGlobalTimestamp(ocsd_trc_index_t index, std::uint8_t traceBusId,
                                                   const OcsdTraceElement& elem)
{
  OpenCsdTraceElement element;
  element.kind = OpenCsdTraceElement::Kind::GlobalTimestamp;
  element.sourceIndex = static_cast<std::uint64_t>(index);
  element.traceBusId = traceBusId;
  element.timestampValue = elem.timestamp;
  element.clockChange = elem.cpu_freq_change != 0U;
  appendElement(std::move(element));
}

void OpenCsdPacketCollector::appendError(ocsd_trc_index_t index, const ItmTrcPacket& pkt)
{
  OpenCsdTraceElement element;
  element.kind = OpenCsdTraceElement::Kind::Error;
  element.sourceIndex = static_cast<std::uint64_t>(index);
  element.issueCode = TraceIssueCode::OpenCsdDecodeError;
  element.errorMessage = pkt.getPktType() == ITM_PKT_RESERVED ? "Reserved ITM packet" : "Bad ITM packet sequence";
  appendElement(std::move(element));
}

void OpenCsdPacketCollector::appendSoftware(ocsd_trc_index_t index, std::uint8_t traceBusId,
                                            const OcsdTraceElement& elem)
{
  const auto& info = elem.swt_itm;
  OpenCsdTraceElement element;
  element.kind = OpenCsdTraceElement::Kind::Software;
  element.sourceIndex = static_cast<std::uint64_t>(index);
  element.traceBusId = traceBusId;
  element.channel = info.payload_src_id;
  element.size = info.payload_size;
  element.value = info.value;
  element.overflow = info.overflow;
  appendElement(std::move(element));
}

void OpenCsdPacketCollector::appendDwt(ocsd_trc_index_t index, std::uint8_t traceBusId, const OcsdTraceElement& elem)
{
  const auto& info = elem.swt_itm;
  OpenCsdTraceElement element;
  element.kind = OpenCsdTraceElement::Kind::Hardware;
  element.sourceIndex = static_cast<std::uint64_t>(index);
  element.traceBusId = traceBusId;
  element.discriminator = info.payload_src_id;
  element.size = info.payload_size;
  element.value = info.value;
  element.overflow = info.overflow;
  appendElement(std::move(element));
}

void OpenCsdPacketCollector::appendTimestamp(ocsd_trc_index_t index, std::uint8_t traceBusId,
                                             const OcsdTraceElement& elem)
{
  const auto& info = elem.swt_itm;
  OpenCsdTraceElement element;
  element.kind = OpenCsdTraceElement::Kind::LocalTimestamp;
  element.sourceIndex = static_cast<std::uint64_t>(index);
  element.traceBusId = traceBusId;
  element.timestampRelation = timestampRelation(info.pkt_type);
  element.tcyc = elem.timestamp;
  element.overflow = info.overflow;
  appendElement(std::move(element));
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

void OpenCsdPacketCollector::appendElement(OpenCsdTraceElement element)
{
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
