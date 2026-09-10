/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "OpenCsdItmDecoder.h"

#include "TraceEvent.h"
#include "OpenCsdErrorController.h"
#include "OpenCsdFormattedItmSession.h"
#include "OpenCsdPacketCollector.h"
#include "OpenCsdItmSession.h"
#include "OpenCsdTraceElement.h"
#include "TraceRoute.h"
#include "opencsd/ocsd_if_types.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

static_assert(sizeof(ocsd_trc_index_t) == sizeof(std::uint64_t), "ctrace requires 64-bit OpenCSD trace indices");

/** @brief Implements OpenCSD feeding, bounded retry, and hardware-sync recovery. */
class OpenCsdItmDecoderImpl {
public:
  /** @brief Creates a decoder implementation around the selected frontend and optional session factory. */
  OpenCsdItmDecoderImpl(std::vector<TraceRouteIdentity> routes, OpenCsdItmInputMode inputMode,
                        OpenCsdTraceElementSink& elementSink, const OpenCsdItmSessionFactory& sessionFactory,
                        OpenCsdUnsupportedTraceIdObserver unsupportedTraceIdObserver)
    : m_inputMode(inputMode),
      m_collector(createCollector(routes, inputMode, elementSink))
  {
    try {
      if (sessionFactory) {
        m_session = sessionFactory(m_collector, m_errorController);
      } else if (isFormatted()) {
        OpenCsdUnsupportedTraceIdSink unsupportedTraceIdSink;
        if (unsupportedTraceIdObserver) {
          unsupportedTraceIdSink = [observer = std::move(unsupportedTraceIdObserver)](std::uint8_t traceBusId,
                                                                                      ocsd_trc_index_t sourceOffset) {
            observer(traceBusId, static_cast<std::uint64_t>(sourceOffset));
          };
        }
        m_session = std::make_unique<OpenCsdFormattedItmSession>(std::move(routes), m_collector, m_errorController,
                                                                 m_collector, std::move(unsupportedTraceIdSink));
      } else {
        m_session = std::make_unique<OpenCsdItmSession>(m_collector, m_errorController);
      }
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
    if (data == nullptr && size != 0U) {
      throw std::invalid_argument("raw trace data pointer is null while bytes are present");
    }
    if (isFormatted() && size % kFormattedFrameSize != 0U) {
      m_collector.appendDecodeError(m_traceIndex, "formatted raw trace chunk is not a multiple of 16 bytes",
                                    TraceIssueCode::OpenCsdDecodeError, false);
      throw OpenCsdFatalError("formatted raw trace chunk is not a multiple of 16 bytes",
                              static_cast<std::uint64_t>(m_traceIndex));
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
    const auto response = invokeSessionOperation([&] { return m_session->endOfTrace(); }, m_traceIndex, 0U, nullptr,
                                                 "OpenCSD aborted end-of-trace processing: ", true);
    const auto decision = m_errorController.decide(response);
    if (decision.action == OpenCsdErrorController::Action::Abort || formattedOperationFailed(decision)) {
      abortDecode(decision, 0U, m_traceIndex, 0U, "OpenCSD aborted end-of-trace processing: ", isFormatted());
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
  static constexpr std::uint32_t kFormattedFrameSize = 16U;

  /** @brief Creates a fixed-route or channel-routed collector for the selected transport. */
  static OpenCsdPacketCollector createCollector(const std::vector<TraceRouteIdentity>& routes,
                                                OpenCsdItmInputMode inputMode, OpenCsdTraceElementSink& elementSink)
  {
    if (routes.empty()) {
      throw std::invalid_argument("OpenCSD ITM decoding requires at least one normalized route");
    }
    if (inputMode == OpenCsdItmInputMode::Single) {
      if (routes.size() != 1U) {
        throw std::invalid_argument("OpenCSD SINGLE decoding requires exactly one normalized route");
      }
      return OpenCsdPacketCollector(routes.front(), elementSink);
    }
    return OpenCsdPacketCollector(routes, elementSink);
  }

  /** @brief Reports whether the frontend is a CoreSight frame deformatter. */
  bool isFormatted() const noexcept
  {
    return m_inputMode == OpenCsdItmInputMode::CoreSightFormatted;
  }

  /** @brief Makes every formatted protocol or deformatter error input-fatal in Phase 7. */
  bool formattedOperationFailed(const OpenCsdErrorController::Decision& decision) const
  {
    if (!isFormatted()) {
      return false;
    }
    const auto reportedError = std::any_of(decision.errors.begin(), decision.errors.end(),
                                           [](const auto& error) { return error.severity == OCSD_ERR_SEV_ERROR; });
    return decision.action == OpenCsdErrorController::Action::RecoverStream || reportedError ||
           OpenCsdErrorController::responseReportsError(decision.response) || m_collector.transactionHasError();
  }

  /** @brief Runs one session operation while keeping legacy SINGLE exception behavior unchanged. */
  template <typename Operation>
  ocsd_datapath_resp_t invokeSessionOperation(Operation&& operation, std::uint64_t baseOffset, std::uint32_t size,
                                              const std::uint32_t* bytesConsumed, const std::string& fatalPrefix,
                                              bool preserveIncompleteTail = false)
  {
    if (!isFormatted()) {
      const auto response = operation();
      m_collector.rethrowOutputError();
      return response;
    }

    try {
      const auto response = operation();
      m_collector.rethrowOutputError();
      return response;
    } catch (const OpenCsdFormattedInputError& error) {
      abortFormattedException(error.what(), error.sourceOffset(), processedOffset(baseOffset, size, bytesConsumed),
                              fatalPrefix, TraceIssueCode::OpenCsdFormattedInputError, preserveIncompleteTail);
    } catch (const std::exception& error) {
      const auto sourceOffset = m_collector.transactionFirstSourceOffset().value_or(baseOffset);
      abortFormattedException(std::string("formatted OpenCSD session operation failed: ") + error.what() +
                                  " at raw input offset " + std::to_string(sourceOffset),
                              sourceOffset, processedOffset(baseOffset, size, bytesConsumed), fatalPrefix,
                              TraceIssueCode::OpenCsdDecodeError, preserveIncompleteTail);
    }
  }

  /** @brief Computes the raw byte boundary reached by a session operation. */
  static std::uint64_t processedOffset(std::uint64_t baseOffset, std::uint32_t size,
                                       const std::uint32_t* bytesConsumed) noexcept
  {
    return baseOffset +
           (bytesConsumed == nullptr ? 0U : std::min<std::uint64_t>(*bytesConsumed, static_cast<std::uint64_t>(size)));
  }

  /** @brief Normalizes one formatted-session exception into the input-fatal decoder contract. */
  [[noreturn]] void abortFormattedException(const std::string& message, std::uint64_t sourceOffset,
                                            std::uint64_t bytesProcessed, const std::string& fatalPrefix,
                                            TraceIssueCode issueCode, bool preserveIncompleteTail)
  {
    if (preserveIncompleteTail) {
      static_cast<void>(m_collector.commitTransactionErrors(TraceIssueCode::OpenCsdIncompleteTail));
    } else {
      m_collector.rollbackTransaction();
    }
    m_collector.appendDecodeError(static_cast<ocsd_trc_index_t>(sourceOffset), message, issueCode, true);
    throw OpenCsdFatalError(fatalPrefix + message, bytesProcessed);
  }

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
                                std::uint64_t baseOffset, std::uint32_t bytesConsumed, const std::string& prefix,
                                bool preserveIncompleteTail = false)
  {
    std::size_t retainedErrors = 0U;
    if (preserveIncompleteTail) {
      retainedErrors = m_collector.commitTransactionErrors(TraceIssueCode::OpenCsdIncompleteTail);
    } else {
      m_collector.rollbackTransaction();
    }
    completeConsumedDataLoss(OpenCsdErrorController::errorOffset(decision, baseOffset));
    appendReportedErrors(decision, baseOffset, true, retainedErrors == 0U);
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
      const auto response = invokeSessionOperation(
          [&] { return m_session->pushData(m_traceIndex, callSize, data + processed, processedThisPass); }, callIndex,
          callSize, &processedThisPass, "OpenCSD aborted decode: ");
      const auto decision = m_errorController.decide(response);
      if (decision.action == OpenCsdErrorController::Action::Abort || formattedOperationFailed(decision)) {
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
        if (isFormatted()) {
          m_collector.appendDecodeError(m_traceIndex, "OpenCSD made no progress on formatted trace input",
                                        TraceIssueCode::OpenCsdNoProgress, false);
          throw OpenCsdFatalError("OpenCSD made no progress on formatted trace input",
                                  static_cast<std::uint64_t>(m_traceIndex));
        }
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
        if (isFormatted()) {
          processed += consumed;
          m_traceIndex += consumed;
          continue;
        }
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
      const auto response = invokeSessionOperation([&] { return m_session->flush(); }, m_traceIndex, 0U, nullptr,
                                                   "OpenCSD aborted while flushing a WAIT response: ");
      const auto decision = m_errorController.decide(response);
      if (decision.action == OpenCsdErrorController::Action::Abort || formattedOperationFailed(decision)) {
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

  OpenCsdItmInputMode m_inputMode = OpenCsdItmInputMode::Single;
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

OpenCsdItmDecoder::OpenCsdItmDecoder(TraceRouteIdentity route, OpenCsdTraceElementSink& elementSink)
  : OpenCsdItmDecoder(std::vector<TraceRouteIdentity>{std::move(route)}, OpenCsdItmInputMode::Single, elementSink)
{
}

OpenCsdItmDecoder::OpenCsdItmDecoder(TraceRouteIdentity route, OpenCsdTraceElementSink& elementSink,
                                     const OpenCsdItmSessionFactory& sessionFactory)
  : OpenCsdItmDecoder(std::vector<TraceRouteIdentity>{std::move(route)}, OpenCsdItmInputMode::Single, elementSink,
                      sessionFactory)
{
}

OpenCsdItmDecoder::OpenCsdItmDecoder(std::vector<TraceRouteIdentity> routes, OpenCsdItmInputMode inputMode,
                                     OpenCsdTraceElementSink& elementSink,
                                     OpenCsdUnsupportedTraceIdObserver unsupportedTraceIdObserver)
  : m_impl(std::make_unique<OpenCsdItmDecoderImpl>(std::move(routes), inputMode, elementSink,
                                                   OpenCsdItmSessionFactory{}, std::move(unsupportedTraceIdObserver)))
{
}

OpenCsdItmDecoder::OpenCsdItmDecoder(std::vector<TraceRouteIdentity> routes, OpenCsdItmInputMode inputMode,
                                     OpenCsdTraceElementSink& elementSink,
                                     const OpenCsdItmSessionFactory& sessionFactory)
  : m_impl(std::make_unique<OpenCsdItmDecoderImpl>(std::move(routes), inputMode, elementSink, sessionFactory,
                                                   OpenCsdUnsupportedTraceIdObserver{}))
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
