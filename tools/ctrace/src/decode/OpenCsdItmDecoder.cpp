/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "OpenCsdItmDecoder.h"

#include "CoreSightFormatter.h"
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
#include <map>
#include <memory>
#include <optional>
#include <set>
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
    m_errorController.setCallbackOrderSource([this] { return m_collector.reserveTransactionOrder(); });
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
    if (isFormatted() && size % CoreSightFormatter::kMemoryAlignedFrameSize != 0U) {
      m_collector.appendDecodeError(m_traceIndex, "formatted raw trace chunk is not a multiple of 16 bytes",
                                    TraceIssueCode::OpenCsdDecodeError, false);
      throw OpenCsdFatalError("formatted raw trace chunk is not a multiple of 16 bytes",
                              static_cast<std::uint64_t>(m_traceIndex));
    }
    std::uint32_t offset = 0;
    while (offset < size) {
      const auto span =
          isFormatted() ? std::min(CoreSightFormatter::kMemoryAlignedFrameSize, size - offset)
                        : std::min(kMaxTraceDataInBytes, size - offset);
      if (isFormatted()) {
        processFormattedFrame(data + offset, span);
      } else {
        processSingleBlock(data + offset, span);
      }
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
    if (isFormatted()) {
      return finishFormatted();
    }
    completeConsumedDataLoss(m_traceIndex);
    m_collector.beginTransaction();
    m_errorController.beginDataPathCall();
    const auto response = invokeSessionOperation([&] { return m_session->endOfTrace(); }, m_traceIndex, 0U, nullptr,
                                                 "OpenCSD aborted end-of-trace processing: ", true);
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
  static constexpr std::uint32_t kMaxFlushCalls = 1024U;

  /** @brief Describes the first recoverable failure observed for one formatted route. */
  struct FormattedRouteFailure {
    TraceRouteIdentity route;
    std::uint64_t sourceOffset = 0U;
  };

  /** @brief Stores the route-local loss interval opened by a successful decoder reset. */
  struct FormattedRecoveryState {
    TraceRouteIdentity route;
    std::uint64_t sourceOffset = 0U;
  };

  using FormattedRouteFailures = std::map<TraceRouteId, FormattedRouteFailure>;

  /** @brief Classifies one completed formatted-tree operation without mutating OpenCSD. */
  struct FormattedOperationOutcome {
    FormattedRouteFailures failures;
    OpenCsdErrorController::Decision fatalDecision;
    bool fatal = false;
    bool wait = false;
  };

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

  /** @brief Returns the earliest raw failure offset for every affected formatted route. */
  static std::map<TraceRouteId, std::uint64_t> failureOffsets(const FormattedRouteFailures& failures)
  {
    std::map<TraceRouteId, std::uint64_t> offsets;
    for (const auto& [routeId, failure] : failures) {
      offsets.emplace(routeId, failure.sourceOffset);
    }
    return offsets;
  }

  /** @brief Resolves recoverable formatted errors and rejects every input-wide failure. */
  FormattedOperationOutcome classifyFormattedOperation(const OpenCsdErrorController::Decision& decision,
                                                       std::uint64_t baseOffset) const
  {
    FormattedOperationOutcome outcome;
    outcome.fatalDecision = decision;
    outcome.wait = OCSD_DATA_RESP_IS_WAIT(decision.response);

    for (const auto& error : decision.errors) {
      if (error.severity != OCSD_ERR_SEV_ERROR) {
        continue;
      }
      const auto* route = error.channel.has_value() ? m_collector.routeForChannel(*error.channel) : nullptr;
      if (!OpenCsdErrorController::isRecoverableStreamError(error.code) || route == nullptr) {
        if (!outcome.fatal) {
          outcome.fatalDecision.error = error;
        }
        outcome.fatal = true;
        continue;
      }

      const auto sourceOffset = error.hasIndex ? error.index : baseOffset;
      const auto found = outcome.failures.find(route->id);
      if (found == outcome.failures.end()) {
        outcome.failures.emplace(route->id, FormattedRouteFailure{*route, sourceOffset});
      } else {
        found->second.sourceOffset = std::min(found->second.sourceOffset, sourceOffset);
      }
    }

    const auto recoverableInvalidData =
        decision.response == OCSD_RESP_FATAL_INVALID_DATA && !outcome.fatal && !outcome.failures.empty();
    if (OCSD_DATA_RESP_IS_FATAL(decision.response) && !recoverableInvalidData) {
      outcome.fatal = true;
      const auto hasNonRecoverableError =
          std::any_of(decision.errors.begin(), decision.errors.end(), [](const auto& error) {
            return error.severity == OCSD_ERR_SEV_ERROR &&
                   !OpenCsdErrorController::isRecoverableStreamError(error.code);
          });
      if (decision.response != OCSD_RESP_FATAL_INVALID_DATA && !hasNonRecoverableError) {
        outcome.fatalDecision.error.reset();
        outcome.fatalDecision.errors.clear();
      }
    }
    if (OpenCsdErrorController::responseReportsError(decision.response) && outcome.failures.empty()) {
      outcome.fatal = true;
    }

    const auto cutoffs = failureOffsets(outcome.failures);
    if (m_collector.transactionHasUnmatchedError(cutoffs)) {
      outcome.fatal = true;
      if (decision.errors.empty()) {
        outcome.fatalDecision.error.reset();
      }
    }
    return outcome;
  }

  /** @brief Reports a formatted callback batch while retaining exact channel attribution. */
  void appendFormattedReportedErrors(const OpenCsdErrorController::Decision& decision, std::uint64_t baseOffset,
                                     bool force = false)
  {
    bool emittedError = false;
    bool inputWideDiscontinuity = true;
    std::set<TraceRouteId> discontinuousRoutes;
    for (const auto& [routeId, recovery] : m_formattedRecoveries) {
      static_cast<void>(recovery);
      discontinuousRoutes.insert(routeId);
    }

    for (const auto& error : decision.errors) {
      if (error.severity != OCSD_ERR_SEV_ERROR && error.severity != OCSD_ERR_SEV_WARN) {
        continue;
      }
      auto item = decision;
      item.error = error;
      const auto sourceOffset = OpenCsdErrorController::errorOffset(item, baseOffset);
      const auto isError = error.severity == OCSD_ERR_SEV_ERROR;
      const auto* route = error.channel.has_value() ? m_collector.routeForChannel(*error.channel) : nullptr;
      bool discontinuity = false;
      if (isError) {
        emittedError = true;
        if (route != nullptr) {
          discontinuity = discontinuousRoutes.insert(route->id).second;
        } else {
          discontinuity = std::exchange(inputWideDiscontinuity, false);
        }
      }
      if (route != nullptr) {
        m_collector.appendReportedDecodeError(*route, static_cast<ocsd_trc_index_t>(sourceOffset),
                                              OpenCsdErrorController::describeSummary(item), error.callbackOrder,
                                              OpenCsdErrorController::issueCode(item), discontinuity,
                                              isError ? TraceIssueSeverity::Error : TraceIssueSeverity::Warning);
      } else {
        m_collector.appendReportedDecodeError(static_cast<ocsd_trc_index_t>(sourceOffset),
                                              OpenCsdErrorController::describeSummary(item), error.callbackOrder,
                                              OpenCsdErrorController::issueCode(item), discontinuity,
                                              isError ? TraceIssueSeverity::Error : TraceIssueSeverity::Warning);
      }
    }
    if (!emittedError && (force || OpenCsdErrorController::responseReportsError(decision.response))) {
      m_collector.appendDecodeError(
          static_cast<ocsd_trc_index_t>(OpenCsdErrorController::errorOffset(decision, baseOffset)),
          OpenCsdErrorController::describeSummary(decision), OpenCsdErrorController::issueCode(decision), true,
          TraceIssueSeverity::Error);
    }
  }

  /** @brief Inserts completed data-loss intervals immediately before each route's recovered sync. */
  void closeFormattedRecoveries(const FormattedRouteFailures& failures)
  {
    for (auto recovery = m_formattedRecoveries.begin(); recovery != m_formattedRecoveries.end();) {
      std::optional<std::uint64_t> beforeOffset;
      const auto failure = failures.find(recovery->first);
      if (failure != failures.end()) {
        beforeOffset = failure->second.sourceOffset;
      }
      const auto syncOffset = m_collector.transactionFirstSyncOffset(recovery->second.route, beforeOffset);
      if (!syncOffset.has_value()) {
        ++recovery;
        continue;
      }

      const auto startOffset = recovery->second.sourceOffset;
      const auto rawBytesConsumed = *syncOffset > startOffset ? *syncOffset - startOffset : 0U;
      const auto message = "OpenCSD discarded " + std::to_string(rawBytesConsumed) +
                           " raw bytes for this ITM route while searching for the next hardware sync";
      static_cast<void>(m_collector.insertDataLossBeforeSync(
          recovery->second.route, static_cast<ocsd_trc_index_t>(startOffset), message, rawBytesConsumed, beforeOffset));
      recovery = m_formattedRecoveries.erase(recovery);
    }
  }

  /** @brief Opens route-local recovery intervals without shortening an already active interval. */
  void openFormattedRecoveries(const FormattedRouteFailures& failures)
  {
    for (const auto& [routeId, failure] : failures) {
      m_formattedRecoveries.try_emplace(routeId, FormattedRecoveryState{failure.route, failure.sourceOffset});
    }
  }

  /** @brief Commits one formatted callback batch according to its route-local failure boundaries. */
  void commitFormattedOperation(const FormattedOperationOutcome& outcome,
                                const OpenCsdErrorController::Decision& decision, std::uint64_t baseOffset)
  {
    closeFormattedRecoveries(outcome.failures);
    appendFormattedReportedErrors(decision, baseOffset);
    if (outcome.failures.empty()) {
      if (m_collector.transactionElementCount() == 0U) {
        m_collector.rollbackTransaction();
      } else {
        m_collector.commitTransaction();
      }
    } else {
      m_collector.commitTransactionForRouteFailures(failureOffsets(outcome.failures));
    }
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

  /** @brief Aborts a formatted operation after rolling back every unsafe callback. */
  [[noreturn]] void abortFormattedDecode(const OpenCsdErrorController::Decision& decision, std::uint32_t size,
                                         std::uint64_t baseOffset, std::uint32_t bytesConsumed,
                                         const std::string& prefix, bool preserveIncompleteTail = false)
  {
    std::size_t retainedErrors = 0U;
    if (preserveIncompleteTail) {
      retainedErrors = m_collector.commitTransactionErrors(TraceIssueCode::OpenCsdIncompleteTail);
    } else {
      m_collector.rollbackTransaction();
    }
    appendFormattedReportedErrors(decision, baseOffset, retainedErrors == 0U);
    const auto processed = baseOffset + std::min<std::uint64_t>(bytesConsumed, size);
    throw OpenCsdFatalError(prefix + OpenCsdErrorController::describeSummary(decision), processed);
  }

  /** @brief Aborts on an invalid formatted root-consumption result. */
  [[noreturn]] void abortFormattedProgress(std::uint64_t baseOffset, std::uint32_t size, std::uint32_t bytesConsumed,
                                           const std::string& message)
  {
    m_collector.rollbackTransaction();
    m_collector.appendDecodeError(static_cast<ocsd_trc_index_t>(baseOffset), message, TraceIssueCode::OpenCsdNoProgress,
                                  false);
    const auto processed = baseOffset + std::min<std::uint64_t>(bytesConsumed, size);
    throw OpenCsdFatalError(message, processed);
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
                                std::uint64_t baseOffset, std::uint32_t bytesConsumed, const std::string& prefix)
  {
    m_collector.rollbackTransaction();
    completeConsumedDataLoss(OpenCsdErrorController::errorOffset(decision, baseOffset));
    appendReportedErrors(decision, baseOffset, true, true);
    const auto processed = baseOffset + std::min<std::uint64_t>(bytesConsumed, size);
    throw OpenCsdFatalError(prefix + OpenCsdErrorController::describeSummary(decision), processed);
  }

  /** @brief Resets one formatted protocol chain without changing deformatter state. */
  void resetFormattedRoute(const FormattedRouteFailure& failure)
  {
    const auto channel = *failure.route.traceBusId;
    m_collector.beginTransaction();
    m_errorController.beginDataPathCall();
    const auto response = invokeSessionOperation(
        [&] { return m_session->resetRoute(channel, static_cast<ocsd_trc_index_t>(failure.sourceOffset)); },
        m_traceIndex, 0U, nullptr, "OpenCSD route-local decoder reset failed: ");
    const auto decision = m_errorController.decide(response);
    const auto outcome = classifyFormattedOperation(decision, failure.sourceOffset);
    if (outcome.fatal || outcome.wait || !outcome.failures.empty()) {
      m_collector.rollbackTransaction();
      const auto hasReportedError = std::any_of(decision.errors.begin(), decision.errors.end(),
                                                [](const auto& error) { return error.severity == OCSD_ERR_SEV_ERROR; });
      appendFormattedReportedErrors(decision, failure.sourceOffset);
      if (!hasReportedError) {
        m_collector.appendDecodeError(failure.route, static_cast<ocsd_trc_index_t>(failure.sourceOffset),
                                      "OpenCSD route-local decoder reset failed: " +
                                          OpenCsdErrorController::describeSummary(decision),
                                      TraceIssueCode::OpenCsdDecodeError, false);
      }
      throw OpenCsdFatalError("OpenCSD route-local decoder reset failed: " +
                                  OpenCsdErrorController::describeSummary(decision),
                              static_cast<std::uint64_t>(m_traceIndex));
    }
    appendFormattedReportedErrors(decision, failure.sourceOffset);
    if (m_collector.transactionElementCount() == 0U) {
      m_collector.rollbackTransaction();
    } else {
      m_collector.commitTransaction();
    }
  }

  /** @brief Resets every route that failed during one completed root operation. */
  void resetFormattedRoutes(const FormattedRouteFailures& failures)
  {
    for (const auto& [routeId, failure] : failures) {
      static_cast<void>(routeId);
      resetFormattedRoute(failure);
    }
  }

  /** @brief Drains pending deformatter segments with bounded root FLUSH calls. */
  void drainFormattedPending()
  {
    for (std::uint32_t call = 0U; call < kMaxFlushCalls; ++call) {
      m_collector.beginTransaction();
      m_errorController.beginDataPathCall();
      const auto response = invokeSessionOperation([&] { return m_session->flush(); }, m_traceIndex, 0U, nullptr,
                                                   "OpenCSD aborted while draining formatted trace: ");
      const auto decision = m_errorController.decide(response);
      const auto outcome = classifyFormattedOperation(decision, m_traceIndex);
      if (outcome.fatal) {
        abortFormattedDecode(outcome.fatalDecision, 0U, m_traceIndex, 0U,
                             "OpenCSD aborted while draining formatted trace: ");
      }

      commitFormattedOperation(outcome, decision, m_traceIndex);
      if (!outcome.failures.empty()) {
        openFormattedRecoveries(outcome.failures);
        resetFormattedRoutes(outcome.failures);
        continue;
      }
      if (!outcome.wait) {
        return;
      }
    }

    const auto limit = std::to_string(kMaxFlushCalls);
    m_collector.appendDecodeError(
        m_traceIndex, "OpenCSD formatted drain did not clear after " + limit + " FLUSH operations; decode aborted",
        TraceIssueCode::OpenCsdWaitTimeout);
    throw OpenCsdFatalError("OpenCSD formatted drain did not clear after " + limit + " FLUSH operations",
                            static_cast<std::uint64_t>(m_traceIndex));
  }

  /** @brief Processes exactly one memory-aligned formatter frame. */
  void processFormattedFrame(const std::uint8_t* data, std::uint32_t size)
  {
    std::uint32_t processed = 0U;
    bool retriedWithoutProgress = false;
    while (processed < size) {
      const auto callIndex = static_cast<std::uint64_t>(m_traceIndex);
      const auto callSize = size - processed;
      std::uint32_t processedThisPass = 0U;
      m_collector.beginTransaction();
      m_errorController.beginDataPathCall();
      const auto response = invokeSessionOperation(
          [&] { return m_session->pushData(m_traceIndex, callSize, data + processed, processedThisPass); }, callIndex,
          callSize, &processedThisPass, "OpenCSD aborted decode: ");
      const auto decision = m_errorController.decide(response);
      const auto outcome = classifyFormattedOperation(decision, callIndex);
      if (outcome.fatal) {
        abortFormattedDecode(outcome.fatalDecision, callSize, callIndex, processedThisPass, "OpenCSD aborted decode: ");
      }
      if (processedThisPass > callSize) {
        abortFormattedProgress(callIndex, callSize, callSize,
                               "OpenCSD reported more formatted bytes processed than were supplied");
      }
      if (processedThisPass != 0U && processedThisPass != callSize) {
        abortFormattedProgress(callIndex, callSize, processedThisPass,
                               "OpenCSD stopped inside a memory-aligned formatter frame");
      }

      commitFormattedOperation(outcome, decision, callIndex);
      processed += processedThisPass;
      m_traceIndex += processedThisPass;

      if (!outcome.failures.empty()) {
        openFormattedRecoveries(outcome.failures);
        resetFormattedRoutes(outcome.failures);
        drainFormattedPending();
      } else if (outcome.wait) {
        drainFormattedPending();
      }

      if (processedThisPass != 0U) {
        retriedWithoutProgress = false;
        continue;
      }
      if (!outcome.wait && outcome.failures.empty()) {
        m_collector.appendDecodeError(m_traceIndex, "OpenCSD made no progress on formatted trace input",
                                      TraceIssueCode::OpenCsdNoProgress, false);
        throw OpenCsdFatalError("OpenCSD made no progress on formatted trace input",
                                static_cast<std::uint64_t>(m_traceIndex));
      }
      if (retriedWithoutProgress) {
        m_collector.appendDecodeError(m_traceIndex,
                                      "OpenCSD made no progress after draining formatted trace; decode aborted",
                                      TraceIssueCode::OpenCsdNoProgress, false);
        throw OpenCsdFatalError("OpenCSD made no progress after draining formatted trace",
                                static_cast<std::uint64_t>(m_traceIndex));
      }
      retriedWithoutProgress = true;
    }
  }

  /** @brief Emits one explicit unresolved interval for every route still awaiting hardware sync. */
  void closeUnresolvedFormattedRecoveries()
  {
    for (const auto& [routeId, recovery] : m_formattedRecoveries) {
      static_cast<void>(routeId);
      const auto rawBytesConsumed =
          m_traceIndex > recovery.sourceOffset ? static_cast<std::uint64_t>(m_traceIndex) - recovery.sourceOffset : 0U;
      const auto message = "OpenCSD discarded " + std::to_string(rawBytesConsumed) +
                           " raw bytes for this ITM route; no later hardware sync before end of input";
      m_collector.appendDataLossError(recovery.route, static_cast<ocsd_trc_index_t>(recovery.sourceOffset), message,
                                      rawBytesConsumed);
    }
    m_formattedRecoveries.clear();
  }

  /** @brief Completes a formatted stream without resetting unresolved routes at end-of-input. */
  OpenCsdItmDecodeResult finishFormatted()
  {
    m_collector.beginTransaction();
    m_errorController.beginDataPathCall();
    const auto response = invokeSessionOperation([&] { return m_session->endOfTrace(); }, m_traceIndex, 0U, nullptr,
                                                 "OpenCSD aborted end-of-trace processing: ", true);
    const auto decision = m_errorController.decide(response);
    const auto outcome = classifyFormattedOperation(decision, m_traceIndex);
    if (outcome.fatal) {
      abortFormattedDecode(outcome.fatalDecision, 0U, m_traceIndex, 0U,
                           "OpenCSD aborted end-of-trace processing: ", true);
    }

    commitFormattedOperation(outcome, decision, m_traceIndex);
    if (!outcome.failures.empty()) {
      openFormattedRecoveries(outcome.failures);
      if (outcome.wait) {
        resetFormattedRoutes(outcome.failures);
        drainFormattedPending();
      }
    } else if (outcome.wait) {
      drainFormattedPending();
    }
    closeUnresolvedFormattedRecoveries();
    m_finished = true;
    m_result.bytesIn = static_cast<std::uint64_t>(m_traceIndex);
    return m_result;
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

  void processSingleBlock(const std::uint8_t* data, std::uint32_t size)
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
        resetDecoder(static_cast<ocsd_trc_index_t>(sourceOffset));
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
        resetDecoder(m_traceIndex);
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
    for (std::uint32_t call = 0; call < kMaxFlushCalls; ++call) {
      m_collector.beginTransaction();
      m_errorController.beginDataPathCall();
      const auto response = invokeSessionOperation([&] { return m_session->flush(); }, m_traceIndex, 0U, nullptr,
                                                   "OpenCSD aborted while flushing a WAIT response: ");
      const auto decision = m_errorController.decide(response);
      if (decision.action == OpenCsdErrorController::Action::Abort) {
        abortDecode(decision, 0U, m_traceIndex, 0U, "OpenCSD aborted while flushing a WAIT response: ");
      }
      if (decision.action == OpenCsdErrorController::Action::RecoverStream) {
        const auto sourceOffset = OpenCsdErrorController::errorOffset(decision, m_traceIndex);
        m_collector.commitTransactionBefore(sourceOffset);
        appendReportedErrors(decision, m_traceIndex, true);
        resetDecoder(static_cast<ocsd_trc_index_t>(sourceOffset));
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

  /** @brief Resets the channel-zero SINGLE decoder at the reported recovery boundary. */
  void resetDecoder(ocsd_trc_index_t index)
  {
    m_errorController.beginDataPathCall();
    const auto response = m_session->resetRoute(0U, index);
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

  // Declaration order is intentional: the external session is destroyed
  // before the callback targets whose addresses may still be installed in it.
  OpenCsdItmInputMode m_inputMode = OpenCsdItmInputMode::Single;
  OpenCsdPacketCollector m_collector;
  OpenCsdErrorController m_errorController;
  std::unique_ptr<OpenCsdItmSessionInterface> m_session;
  ocsd_trc_index_t m_traceIndex = 0;
  bool m_dataLossActive = false;
  std::optional<std::uint64_t> m_consumedDataLossStart;
  bool m_consumedDataLossBoundaryMarked = false;
  std::map<TraceRouteId, FormattedRecoveryState> m_formattedRecoveries;
  OpenCsdItmDecodeResult m_result;
  bool m_finished = false;
};

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
