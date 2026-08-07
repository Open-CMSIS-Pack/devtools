/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "OpenCsdItmDecoder.h"

#include "TraceEvent.h"
#include "OpenCsdErrorController.h"
#include "OpenCsdPacketCollector.h"
#include "OpenCsdItmSession.h"
#include "OpenCsdTraceElement.h"
#include "opencsd/ocsd_if_types.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

static_assert(sizeof(ocsd_trc_index_t) == sizeof(std::uint64_t), "ctrace requires 64-bit OpenCSD trace indices");

/** @brief Implements OpenCSD feeding, bounded retry, and hardware-sync recovery. */
class OpenCsdItmDecoderImpl {
public:
  /** @brief Creates a decoder implementation around one session factory. */
  OpenCsdItmDecoderImpl(OpenCsdTraceElementSink& elementSink, const OpenCsdItmSessionFactory& sessionFactory)
    : m_collector(elementSink)
  {
    try {
      m_session = sessionFactory(m_collector, m_errorController);
      if (m_session == nullptr) {
        failInitialization("OpenCSD ITM session factory returned no session");
      }
    } catch (const OpenCsdItmSessionError& error) {
      failInitialization(error.what());
    }
  }

  /** @brief Pushes the next raw trace byte chunk. */
  void push(const std::uint8_t* data, std::uint32_t size)
  {
    if (m_finished) {
      throw std::runtime_error("OpenCSD ITM decoder already finished");
    }
    std::uint32_t offset = 0;
    while (offset < size) {
      const auto span = std::min(kMaxTraceDataInBytes, size - offset);
      processBlock(data + offset, span);
      offset += span;
    }
    m_result.bytesIn = static_cast<std::uint64_t>(m_traceIndex);
  }

  /** @brief Completes the OpenCSD stream and returns the consumed byte count. */
  OpenCsdItmDecodeResult finish()
  {
    if (m_finished) {
      return m_result;
    }
    completeConsumedDataLoss(m_traceIndex);
    m_collector.beginTransaction();
    m_errorController.beginDataPathCall();
    const auto response = m_session->endOfTrace();
    m_collector.rethrowOutputError();
    const auto decision = m_errorController.decide(response);
    if (decision.action == OpenCsdErrorController::Action::Abort) {
      abortDecode(decision, 0U, m_traceIndex, 0U, "OpenCSD aborted end-of-trace processing: ");
    }
    if (decision.action == OpenCsdErrorController::Action::RecoverStream) {
      const auto sourceOffset = OpenCsdErrorController::errorOffset(decision, m_traceIndex);
      m_collector.commitTransactionBefore(sourceOffset);
      appendReportedErrors(decision, m_traceIndex, true);
    } else {
      m_collector.commitTransaction();
      appendReportedErrors(decision, m_traceIndex, false);
      if (decision.action == OpenCsdErrorController::Action::Wait) {
        flushAfterWait();
      }
    }
    m_finished = true;
    m_result.bytesIn = static_cast<std::uint64_t>(m_traceIndex);
    return m_result;
  }

private:
  static constexpr std::uint32_t kMaxTraceDataInBytes = 4U * 1024U;

  void appendReportedErrors(const OpenCsdErrorController::Decision& decision, std::uint64_t baseOffset,
                            bool discontinuity, bool force = false)
  {
    bool emittedError = false;
    bool nextDiscontinuity = discontinuity;
    for (const auto& error : decision.errors) {
      if (error.severity != OCSD_ERR_SEV_ERROR && error.severity != OCSD_ERR_SEV_WARN) {
        continue;
      }
      auto item = decision;
      item.error = error;
      const auto sourceOffset = OpenCsdErrorController::errorOffset(item, baseOffset);
      const auto isError = error.severity == OCSD_ERR_SEV_ERROR;
      m_collector.appendDecodeError(static_cast<ocsd_trc_index_t>(sourceOffset),
                                    OpenCsdErrorController::describeSummary(item),
                                    OpenCsdErrorController::issueCode(item), nextDiscontinuity && isError,
                                    isError ? TraceIssueSeverity::Error : TraceIssueSeverity::Warning);
      if (isError) {
        emittedError = true;
        nextDiscontinuity = false;
      }
    }
    if (!emittedError && (force || OpenCsdErrorController::responseReportsError(decision.response))) {
      m_collector.appendDecodeError(
          static_cast<ocsd_trc_index_t>(OpenCsdErrorController::errorOffset(decision, baseOffset)),
          OpenCsdErrorController::describeSummary(decision), OpenCsdErrorController::issueCode(decision), discontinuity,
          TraceIssueSeverity::Error);
    }
  }

  [[noreturn]] void abortDecode(const OpenCsdErrorController::Decision& decision, std::uint32_t size,
                                std::uint64_t baseOffset, std::uint32_t bytesConsumed, const std::string& prefix)
  {
    m_collector.rollbackTransaction();
    completeConsumedDataLoss(OpenCsdErrorController::errorOffset(decision, baseOffset));
    appendReportedErrors(decision, baseOffset, true);
    const auto processed = baseOffset + std::min<std::uint64_t>(bytesConsumed, size);
    throw OpenCsdFatalError(prefix + OpenCsdErrorController::describeSummary(decision), processed);
  }

  void completeConsumedDataLoss(std::uint64_t resumeOffset)
  {
    if (!m_consumedDataLossStart.has_value()) {
      return;
    }
    const auto startOffset = *m_consumedDataLossStart;
    const auto endOffset = std::max(startOffset, resumeOffset);
    const auto consumed = endOffset - startOffset;
    m_consumedDataLossStart.reset();
    const auto boundaryAlreadyMarked = m_consumedDataLossBoundaryMarked;
    m_consumedDataLossBoundaryMarked = false;
    if (consumed == 0U) {
      return;
    }
    const auto message =
        "OpenCSD consumed " + std::to_string(consumed) +
        " raw bytes while waiting for usable ITM trace packets; data loss until a later sync/recovery point";
    if (boundaryAlreadyMarked) {
      m_collector.prependDataLossError(static_cast<ocsd_trc_index_t>(startOffset), message, consumed);
    } else {
      m_collector.prependDiscontinuity(static_cast<ocsd_trc_index_t>(startOffset), message, TraceIssueCode::DataLoss,
                                       consumed);
    }
  }

  void processBlock(const std::uint8_t* data, std::uint32_t size)
  {
    std::uint32_t processed = 0;
    bool retriedWithoutProgress = false;
    while (processed < size) {
      const auto callIndex = static_cast<std::uint64_t>(m_traceIndex);
      const auto callSize = size - processed;
      std::uint32_t processedThisPass = 0;
      m_collector.beginTransaction();
      m_errorController.beginDataPathCall();
      const auto response = m_session->pushData(m_traceIndex, callSize, data + processed, processedThisPass);
      m_collector.rethrowOutputError();
      const auto decision = m_errorController.decide(response);
      if (decision.action == OpenCsdErrorController::Action::Abort) {
        abortDecode(decision, callSize, callIndex, processedThisPass, "OpenCSD aborted decode: ");
      }

      const auto consumed = std::min(processedThisPass, callSize);
      if (consumed == 0U) {
        if (retriedWithoutProgress) {
          m_collector.rollbackTransaction();
          completeConsumedDataLoss(m_traceIndex);
          m_collector.appendDecodeError(m_traceIndex, "OpenCSD made no progress after a retry; decode aborted",
                                        TraceIssueCode::OpenCsdNoProgress, false);
          throw OpenCsdFatalError("OpenCSD made no progress after a retry", static_cast<std::uint64_t>(m_traceIndex));
        }
        retriedWithoutProgress = true;
      } else {
        retriedWithoutProgress = false;
      }

      if (decision.action == OpenCsdErrorController::Action::RecoverStream) {
        const auto sourceOffset = OpenCsdErrorController::errorOffset(decision, callIndex);
        completeConsumedDataLoss(
            std::min(sourceOffset, m_collector.transactionFirstSourceOffset().value_or(sourceOffset)));
        // Callbacks before the bad packet remain valid; callbacks at or
        // after its offset belong to the failed decode transaction.
        m_collector.commitTransactionBefore(sourceOffset);
        appendReportedErrors(decision, callIndex, true);
        processed += consumed;
        m_traceIndex += consumed;
        m_dataLossActive = true;
        m_consumedDataLossStart = static_cast<std::uint64_t>(m_traceIndex);
        m_consumedDataLossBoundaryMarked = true;
        resetDecoder();
        continue;
      }
      if (decision.action == OpenCsdErrorController::Action::Wait) {
        if (m_collector.transactionElementCount() == 0U) {
          m_collector.rollbackTransaction();
        } else {
          completeConsumedDataLoss(m_collector.transactionFirstSourceOffset().value_or(callIndex));
          m_collector.commitTransaction();
        }
        appendReportedErrors(decision, callIndex, false);
        processed += consumed;
        m_traceIndex += consumed;
        flushAfterWait();
        continue;
      }
      if (consumed == 0U) {
        m_collector.rollbackTransaction();
        m_collector.appendDecodeError(
            m_traceIndex,
            "OpenCSD made no progress while raw data was present; decoder reset and searching "
            "for next real ITM async sync",
            TraceIssueCode::OpenCsdNoProgress);
        m_dataLossActive = true;
        m_consumedDataLossStart = static_cast<std::uint64_t>(m_traceIndex);
        m_consumedDataLossBoundaryMarked = true;
        resetDecoder();
        continue;
      }
      if (m_collector.transactionElementCount() == 0U) {
        m_collector.rollbackTransaction();
        appendReportedErrors(decision, callIndex, false);
        if (!m_dataLossActive) {
          m_consumedDataLossStart = static_cast<std::uint64_t>(m_traceIndex);
          m_consumedDataLossBoundaryMarked = false;
          m_dataLossActive = true;
        }
        processed += consumed;
        m_traceIndex += consumed;
        continue;
      }
      completeConsumedDataLoss(m_collector.transactionFirstSourceOffset().value_or(callIndex));
      m_collector.commitTransaction();
      appendReportedErrors(decision, callIndex, false);
      m_dataLossActive = false;
      processed += consumed;
      m_traceIndex += consumed;
    }
  }

  void flushAfterWait()
  {
    // Bound backpressure handling so a broken decoder cannot stall a file forever.
    static constexpr std::uint32_t kMaxFlushCalls = 1024U;
    for (std::uint32_t call = 0; call < kMaxFlushCalls; ++call) {
      m_collector.beginTransaction();
      m_errorController.beginDataPathCall();
      const auto response = m_session->flush();
      m_collector.rethrowOutputError();
      const auto decision = m_errorController.decide(response);
      if (decision.action == OpenCsdErrorController::Action::Abort) {
        abortDecode(decision, 0U, m_traceIndex, 0U, "OpenCSD aborted while flushing a WAIT response: ");
      }
      if (decision.action == OpenCsdErrorController::Action::RecoverStream) {
        const auto sourceOffset = OpenCsdErrorController::errorOffset(decision, m_traceIndex);
        m_collector.commitTransactionBefore(sourceOffset);
        appendReportedErrors(decision, m_traceIndex, true);
        resetDecoder();
        return;
      }
      if (m_collector.transactionElementCount() == 0U) {
        m_collector.rollbackTransaction();
      } else {
        m_collector.commitTransaction();
      }
      appendReportedErrors(decision, m_traceIndex, false);
      if (decision.action == OpenCsdErrorController::Action::Continue) {
        return;
      }
    }
    const auto limit = std::to_string(kMaxFlushCalls);
    m_collector.appendDecodeError(m_traceIndex,
                                  "OpenCSD WAIT did not clear after " + limit + " FLUSH operations; decode aborted",
                                  TraceIssueCode::OpenCsdWaitTimeout);
    throw OpenCsdFatalError("OpenCSD WAIT did not clear after " + limit + " FLUSH operations",
                            static_cast<std::uint64_t>(m_traceIndex));
  }

  void resetDecoder()
  {
    m_errorController.beginDataPathCall();
    const auto response = m_session->reset();
    m_collector.rethrowOutputError();
    const auto decision = m_errorController.decide(response);
    if (decision.action != OpenCsdErrorController::Action::Continue) {
      appendReportedErrors(decision, m_traceIndex, true, true);
      throw OpenCsdFatalError("OpenCSD decoder reset failed: " + OpenCsdErrorController::describeSummary(decision),
                              static_cast<std::uint64_t>(m_traceIndex));
    }
    appendReportedErrors(decision, m_traceIndex, false);
  }

  [[noreturn]] void failInitialization(const std::string& message)
  {
    m_collector.appendDecodeError(m_traceIndex, message, TraceIssueCode::OpenCsdInitializationError, false);
    throw OpenCsdFatalError(message, static_cast<std::uint64_t>(m_traceIndex));
  }

  OpenCsdPacketCollector m_collector;
  OpenCsdErrorController m_errorController;
  std::unique_ptr<OpenCsdItmSessionInterface> m_session;
  ocsd_trc_index_t m_traceIndex = 0;
  bool m_dataLossActive = false;
  std::optional<std::uint64_t> m_consumedDataLossStart;
  bool m_consumedDataLossBoundaryMarked = false;
  OpenCsdItmDecodeResult m_result;
  bool m_finished = false;
};

OpenCsdItmDecoder::OpenCsdItmDecoder(OpenCsdTraceElementSink& elementSink)
  : OpenCsdItmDecoder(elementSink, [](OpenCsdPacketCollector& collector, OpenCsdErrorController& errorController) {
      return std::make_unique<OpenCsdItmSession>(collector, errorController);
    })
{
}

OpenCsdItmDecoder::OpenCsdItmDecoder(OpenCsdTraceElementSink& elementSink,
                                     const OpenCsdItmSessionFactory& sessionFactory)
  : m_impl(std::make_unique<OpenCsdItmDecoderImpl>(elementSink, sessionFactory))
{
}

OpenCsdItmDecoder::~OpenCsdItmDecoder() = default;

void OpenCsdItmDecoder::push(const std::uint8_t* data, std::uint32_t size)
{
  m_impl->push(data, size);
}

OpenCsdItmDecodeResult OpenCsdItmDecoder::finish()
{
  return m_impl->finish();
}
