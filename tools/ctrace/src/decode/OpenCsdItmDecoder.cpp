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
                        OpenCsdUnsupportedTraceIdObserver unsupportedTraceIdObserver,
                        OpenCsdSkippedBytesObserver skippedBytesObserver)
    : m_inputMode(inputMode),
      m_collector(createCollector(routes, inputMode, elementSink, skippedBytesObserver))
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
                                                                 m_collector, std::move(unsupportedTraceIdSink),
                                                                 std::move(skippedBytesObserver));
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

  /** @brief Tracks progress while one memory-aligned formatter frame is consumed. */
  struct FormattedFrameState {
    std::uint32_t processed = 0U;
    bool retriedWithoutProgress = false;
  };

  /** @brief Captures the result of one formatted root DATA operation. */
  struct FormattedPushResult {
    std::uint64_t baseOffset = 0U;
    std::uint32_t supplied = 0U;
    std::uint32_t consumed = 0U;
    OpenCsdErrorController::Decision decision;
    FormattedOperationOutcome outcome;
  };

  /** @brief Tracks input progress and bounded retries within one SINGLE block. */
  struct SingleBlockState {
    std::uint32_t processed = 0U;
    bool retriedWithoutProgress = false;
  };

  /** @brief Captures the result of one SINGLE root DATA operation. */
  struct SinglePushResult {
    std::uint64_t baseOffset = 0U;
    std::uint32_t supplied = 0U;
    std::uint32_t consumed = 0U;
    OpenCsdErrorController::Decision decision;
  };

  /** @brief Tracks which formatted diagnostics have already opened a discontinuity. */
  struct FormattedDiagnosticState {
    std::set<TraceRouteId> discontinuousRoutes;
    bool emittedError = false;
    bool inputWideDiscontinuity = true;
  };

  /** @brief Creates a fixed-route or channel-routed collector for the selected transport. */
  static OpenCsdPacketCollector createCollector(const std::vector<TraceRouteIdentity>& routes,
                                                OpenCsdItmInputMode inputMode, OpenCsdTraceElementSink& elementSink,
                                                OpenCsdSkippedBytesObserver skippedBytesObserver)
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
    return OpenCsdPacketCollector(routes, elementSink, std::move(skippedBytesObserver));
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

  /** @brief Records one route's earliest recoverable failure in an operation outcome. */
  static void recordFormattedFailure(FormattedOperationOutcome& outcome, const TraceRouteIdentity& route,
                                     std::uint64_t sourceOffset)
  {
    const auto found = outcome.failures.find(route.id);
    if (found == outcome.failures.end()) {
      outcome.failures.emplace(route.id, FormattedRouteFailure{route, sourceOffset});
    } else {
      found->second.sourceOffset = std::min(found->second.sourceOffset, sourceOffset);
    }
  }

  /** @brief Classifies one formatted error callback as route-local or input-fatal. */
  void classifyFormattedError(FormattedOperationOutcome& outcome, const OpenCsdErrorRecord& error,
                              std::uint64_t baseOffset) const
  {
    if (error.severity != OCSD_ERR_SEV_ERROR) {
      return;
    }
    const auto* route = error.channel.has_value() ? m_collector.routeForChannel(*error.channel) : nullptr;
    if (!OpenCsdErrorController::isRecoverableStreamError(error.code) || route == nullptr) {
      if (!outcome.fatal) {
        outcome.fatalDecision.error = error;
      }
      outcome.fatal = true;
      return;
    }

    recordFormattedFailure(outcome, *route, error.hasIndex ? error.index : baseOffset);
  }

  /** @brief Returns whether a callback batch contains an input-fatal decoder error. */
  static bool hasNonRecoverableError(const OpenCsdErrorController::Decision& decision)
  {
    return std::any_of(decision.errors.begin(), decision.errors.end(), [](const auto& error) {
      return error.severity == OCSD_ERR_SEV_ERROR &&
             !OpenCsdErrorController::isRecoverableStreamError(error.code);
    });
  }

  /** @brief Applies root response semantics after callback errors have been classified. */
  static void classifyFormattedResponse(FormattedOperationOutcome& outcome,
                                        const OpenCsdErrorController::Decision& decision)
  {
    const auto recoverableInvalidData =
        decision.response == OCSD_RESP_FATAL_INVALID_DATA && !outcome.fatal && !outcome.failures.empty();
    if (OCSD_DATA_RESP_IS_FATAL(decision.response) && !recoverableInvalidData) {
      outcome.fatal = true;
      if (decision.response != OCSD_RESP_FATAL_INVALID_DATA && !hasNonRecoverableError(decision)) {
        outcome.fatalDecision.error.reset();
        outcome.fatalDecision.errors.clear();
      }
    }
    if (OpenCsdErrorController::responseReportsError(decision.response) && outcome.failures.empty()) {
      outcome.fatal = true;
    }
  }

  /** @brief Resolves recoverable formatted errors and rejects every input-wide failure. */
  FormattedOperationOutcome classifyFormattedOperation(const OpenCsdErrorController::Decision& decision,
                                                       std::uint64_t baseOffset) const
  {
    FormattedOperationOutcome outcome;
    outcome.fatalDecision = decision;
    outcome.wait = OCSD_DATA_RESP_IS_WAIT(decision.response);
    for (const auto& error : decision.errors) {
      classifyFormattedError(outcome, error, baseOffset);
    }
    classifyFormattedResponse(outcome, decision);

    const auto cutoffs = failureOffsets(outcome.failures);
    if (m_collector.transactionHasUnmatchedError(cutoffs)) {
      outcome.fatal = true;
      if (decision.errors.empty()) {
        outcome.fatalDecision.error.reset();
      }
    }
    return outcome;
  }

  /** @brief Seeds one formatted diagnostic batch with already active route recoveries. */
  FormattedDiagnosticState formattedDiagnosticState() const
  {
    FormattedDiagnosticState state;
    for (const auto& [routeId, recovery] : m_formattedRecoveries) {
      static_cast<void>(recovery);
      state.discontinuousRoutes.insert(routeId);
    }
    return state;
  }

  /** @brief Joins the logger error with a matching packet observed later in the same operation. */
  std::string reportedMessage(const OpenCsdErrorController::Decision& decision) const
  {
    auto message = OpenCsdErrorController::describeSummary(decision);
    if (decision.error.has_value() && decision.error->hasIndex) {
      const auto context = m_collector.packetErrorContext(decision.error->channel, decision.error->index);
      if (!context.empty()) {
        message += (message.back() == ';' ? " " : "; ") + context;
      }
    }
    return message;
  }

  /** @brief Preserves warning-only responses when no warning/error callback explains them. */
  void appendUnreportedWarning(const OpenCsdErrorController::Decision& decision, std::uint64_t baseOffset)
  {
    const auto hasDiagnostic = std::any_of(decision.errors.begin(), decision.errors.end(), [](const auto& error) {
      return error.severity == OCSD_ERR_SEV_ERROR || error.severity == OCSD_ERR_SEV_WARN;
    });
    if (OCSD_DATA_RESP_IS_WARN(decision.response) && !hasDiagnostic) {
      OpenCsdErrorController::Decision warning;
      warning.response = decision.response;
      m_collector.appendDecodeError(static_cast<ocsd_trc_index_t>(baseOffset),
                                    OpenCsdErrorController::describeSummary(warning),
                                    TraceIssueCode::OpenCsdDecodeError, false, TraceIssueSeverity::Warning);
    }
  }

  /** @brief Appends one formatted warning or error with its route-local discontinuity state. */
  void appendFormattedReportedError(const OpenCsdErrorController::Decision& decision,
                                    const OpenCsdErrorRecord& error, std::uint64_t baseOffset,
                                    FormattedDiagnosticState& state)
  {
    auto item = decision;
    item.error = error;
    const auto sourceOffset = OpenCsdErrorController::errorOffset(item, baseOffset);
    const auto isError = error.severity == OCSD_ERR_SEV_ERROR;
    const auto* route = error.channel.has_value() ? m_collector.routeForChannel(*error.channel) : nullptr;
    bool discontinuity = false;
    if (isError) {
      state.emittedError = true;
      if (route != nullptr) {
        discontinuity = state.discontinuousRoutes.insert(route->id).second;
      } else {
        discontinuity = std::exchange(state.inputWideDiscontinuity, false);
      }
    }

    const auto severity = isError ? TraceIssueSeverity::Error : TraceIssueSeverity::Warning;
    if (route != nullptr) {
      m_collector.appendReportedDecodeError(*route, static_cast<ocsd_trc_index_t>(sourceOffset),
                                            reportedMessage(item), error.callbackOrder,
                                            OpenCsdErrorController::issueCode(item), discontinuity, severity);
    } else {
      m_collector.appendReportedDecodeError(static_cast<ocsd_trc_index_t>(sourceOffset),
                                            reportedMessage(item), error.callbackOrder,
                                            OpenCsdErrorController::issueCode(item), discontinuity, severity);
    }
  }

  /** @brief Reports a formatted callback batch while retaining exact channel attribution. */
  void appendFormattedReportedErrors(const OpenCsdErrorController::Decision& decision, std::uint64_t baseOffset,
                                     bool force = false)
  {
    auto state = formattedDiagnosticState();
    for (const auto& error : decision.errors) {
      if (error.severity == OCSD_ERR_SEV_ERROR || error.severity == OCSD_ERR_SEV_WARN) {
        appendFormattedReportedError(decision, error, baseOffset, state);
      }
    }
    if (!state.emittedError && (force || OpenCsdErrorController::responseReportsError(decision.response))) {
      m_collector.appendDecodeError(
          static_cast<ocsd_trc_index_t>(OpenCsdErrorController::errorOffset(decision, baseOffset)),
          OpenCsdErrorController::describeSummary(decision), OpenCsdErrorController::issueCode(decision), true,
          TraceIssueSeverity::Error);
    }
    if (!force) {
      appendUnreportedWarning(decision, baseOffset);
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
      const auto message = "ITM decoding resumed at hardware SYNC at raw offset " + std::to_string(*syncOffset) +
                           "; affected raw interval [" + std::to_string(startOffset) + ", " +
                           std::to_string(*syncOffset) + ") spans " + std::to_string(rawBytesConsumed) + " bytes";
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
    // The raw monitor can report an incomplete tail while OpenCSD returns CONT without a logger error.
    const auto incompleteTailOnly = retainedErrors != 0U &&
                                    !OpenCsdErrorController::responseReportsError(decision.response) &&
                                    (!decision.error.has_value() || decision.error->severity != OCSD_ERR_SEV_ERROR);
    const auto reason = incompleteTailOnly ? std::string("incomplete ITM packet at end of input")
                                          : OpenCsdErrorController::describeSummary(decision);
    throw OpenCsdFatalError(prefix + reason, processed);
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
                                    reportedMessage(item),
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
    if (!force) {
      appendUnreportedWarning(decision, baseOffset);
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

  /** @brief Executes and validates one formatted root DATA operation. */
  FormattedPushResult pushFormattedData(const std::uint8_t* data, std::uint32_t size)
  {
    FormattedPushResult result;
    result.baseOffset = static_cast<std::uint64_t>(m_traceIndex);
    result.supplied = size;
    m_collector.beginTransaction();
    m_errorController.beginDataPathCall();
    const auto response = invokeSessionOperation(
        [&] { return m_session->pushData(m_traceIndex, result.supplied, data, result.consumed); }, result.baseOffset,
        result.supplied, &result.consumed, "OpenCSD aborted decode: ");
    result.decision = m_errorController.decide(response);
    result.outcome = classifyFormattedOperation(result.decision, result.baseOffset);
    if (result.outcome.fatal) {
      abortFormattedDecode(result.outcome.fatalDecision, result.supplied, result.baseOffset, result.consumed,
                           "OpenCSD aborted decode: ");
    }
    if (result.consumed > result.supplied) {
      abortFormattedProgress(result.baseOffset, result.supplied, result.supplied,
                             "OpenCSD reported more formatted bytes processed than were supplied");
    }
    if (result.consumed != 0U && result.consumed != result.supplied) {
      abortFormattedProgress(result.baseOffset, result.supplied, result.consumed,
                             "OpenCSD stopped inside a memory-aligned formatter frame");
    }
    return result;
  }

  /** @brief Performs any route reset and root draining requested by a formatted DATA operation. */
  void recoverFormattedData(const FormattedOperationOutcome& outcome)
  {
    if (!outcome.failures.empty()) {
      openFormattedRecoveries(outcome.failures);
      resetFormattedRoutes(outcome.failures);
      drainFormattedPending();
    } else if (outcome.wait) {
      drainFormattedPending();
    }
  }

  /** @brief Updates the bounded retry state after recovery and rejects permanent stalls. */
  void updateFormattedProgress(FormattedFrameState& state, const FormattedPushResult& result)
  {
    if (result.consumed != 0U) {
      state.retriedWithoutProgress = false;
      return;
    }
    if (!result.outcome.wait && result.outcome.failures.empty()) {
      m_collector.appendDecodeError(m_traceIndex, "OpenCSD made no progress on formatted trace input",
                                    TraceIssueCode::OpenCsdNoProgress, false);
      throw OpenCsdFatalError("OpenCSD made no progress on formatted trace input",
                              static_cast<std::uint64_t>(m_traceIndex));
    }
    if (state.retriedWithoutProgress) {
      m_collector.appendDecodeError(m_traceIndex,
                                    "OpenCSD made no progress after draining formatted trace; decode aborted",
                                    TraceIssueCode::OpenCsdNoProgress, false);
      throw OpenCsdFatalError("OpenCSD made no progress after draining formatted trace",
                              static_cast<std::uint64_t>(m_traceIndex));
    }
    state.retriedWithoutProgress = true;
  }

  /** @brief Processes exactly one memory-aligned formatter frame. */
  void processFormattedFrame(const std::uint8_t* data, std::uint32_t size)
  {
    FormattedFrameState state;
    while (state.processed < size) {
      const auto result = pushFormattedData(data + state.processed, size - state.processed);
      commitFormattedOperation(result.outcome, result.decision, result.baseOffset);
      state.processed += result.consumed;
      m_traceIndex += result.consumed;
      recoverFormattedData(result.outcome);
      updateFormattedProgress(state, result);
    }
  }

  /** @brief Emits one explicit unresolved interval for every route still awaiting hardware sync. */
  void closeUnresolvedFormattedRecoveries()
  {
    for (const auto& [routeId, recovery] : m_formattedRecoveries) {
      static_cast<void>(routeId);
      const auto rawBytesConsumed =
          m_traceIndex > recovery.sourceOffset ? static_cast<std::uint64_t>(m_traceIndex) - recovery.sourceOffset : 0U;
      const auto message = "ITM decoding did not resume before end of input at raw offset " +
                           std::to_string(m_traceIndex) + "; no later hardware SYNC; affected raw interval [" +
                           std::to_string(recovery.sourceOffset) + ", " + std::to_string(m_traceIndex) +
                           ") spans " + std::to_string(rawBytesConsumed) + " bytes";
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
    m_collector.reportUnsynchronizedFormattedRoutes();
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

  /** @brief Executes one SINGLE root DATA operation and normalizes its consumed-byte count. */
  SinglePushResult pushSingleData(const std::uint8_t* data, std::uint32_t size)
  {
    SinglePushResult result;
    result.baseOffset = static_cast<std::uint64_t>(m_traceIndex);
    result.supplied = size;
    m_collector.beginTransaction();
    m_errorController.beginDataPathCall();
    const auto response = invokeSessionOperation(
        [&] { return m_session->pushData(m_traceIndex, result.supplied, data, result.consumed); }, result.baseOffset,
        result.supplied, &result.consumed, "OpenCSD aborted decode: ");
    result.decision = m_errorController.decide(response);
    if (result.decision.action == OpenCsdErrorController::Action::Abort) {
      abortDecode(result.decision, result.supplied, result.baseOffset, result.consumed, "OpenCSD aborted decode: ");
    }
    result.consumed = std::min(result.consumed, result.supplied);
    return result;
  }

  /** @brief Updates the bounded SINGLE retry state before response-specific recovery. */
  void updateSingleProgress(SingleBlockState& state, const SinglePushResult& result)
  {
    if (result.consumed != 0U) {
      state.retriedWithoutProgress = false;
      return;
    }
    if (state.retriedWithoutProgress) {
      m_collector.rollbackTransaction();
      completeConsumedDataLoss(m_traceIndex);
      m_collector.appendDecodeError(m_traceIndex, "OpenCSD made no progress after a retry; decode aborted",
                                    TraceIssueCode::OpenCsdNoProgress, false);
      throw OpenCsdFatalError("OpenCSD made no progress after a retry", static_cast<std::uint64_t>(m_traceIndex));
    }
    state.retriedWithoutProgress = true;
  }

  /** @brief Advances both the caller-visible block cursor and the absolute raw trace index. */
  void advanceSingleInput(SingleBlockState& state, std::uint32_t consumed)
  {
    state.processed += consumed;
    m_traceIndex += consumed;
  }

  /** @brief Commits valid callbacks before a recoverable SINGLE stream error and resets the decoder. */
  void recoverSingleStream(SingleBlockState& state, const SinglePushResult& result)
  {
    const auto sourceOffset = OpenCsdErrorController::errorOffset(result.decision, result.baseOffset);
    completeConsumedDataLoss(
        std::min(sourceOffset, m_collector.transactionFirstSourceOffset().value_or(sourceOffset)));
    // Callbacks before the bad packet remain valid; callbacks at or
    // after its offset belong to the failed decode transaction.
    m_collector.commitTransactionBefore(sourceOffset);
    appendReportedErrors(result.decision, result.baseOffset, true);
    advanceSingleInput(state, result.consumed);
    m_dataLossActive = true;
    m_consumedDataLossStart = static_cast<std::uint64_t>(m_traceIndex);
    m_consumedDataLossBoundaryMarked = true;
    resetDecoder(static_cast<ocsd_trc_index_t>(sourceOffset));
  }

  /** @brief Commits any valid SINGLE callbacks before draining a WAIT response. */
  void drainSingleWait(SingleBlockState& state, const SinglePushResult& result)
  {
    if (m_collector.transactionElementCount() == 0U) {
      m_collector.rollbackTransaction();
    } else {
      completeConsumedDataLoss(m_collector.transactionFirstSourceOffset().value_or(result.baseOffset));
      m_collector.commitTransaction();
    }
    appendReportedErrors(result.decision, result.baseOffset, false);
    advanceSingleInput(state, result.consumed);
    flushAfterWait();
  }

  /** @brief Resets a stalled SINGLE decoder so the next call can search for hardware sync. */
  void recoverSingleNoProgress()
  {
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
  }

  /** @brief Records consumed SINGLE input that produced no trace elements. */
  void consumeSilentSingleData(SingleBlockState& state, const SinglePushResult& result)
  {
    m_collector.rollbackTransaction();
    appendReportedErrors(result.decision, result.baseOffset, false);
    if (!m_dataLossActive) {
      m_consumedDataLossStart = static_cast<std::uint64_t>(m_traceIndex);
      m_consumedDataLossBoundaryMarked = false;
      m_dataLossActive = true;
    }
    advanceSingleInput(state, result.consumed);
  }

  /** @brief Commits a successful SINGLE DATA operation containing trace elements. */
  void commitSingleData(SingleBlockState& state, const SinglePushResult& result)
  {
    completeConsumedDataLoss(m_collector.transactionFirstSourceOffset().value_or(result.baseOffset));
    m_collector.commitTransaction();
    appendReportedErrors(result.decision, result.baseOffset, false);
    m_dataLossActive = false;
    advanceSingleInput(state, result.consumed);
  }

  /** @brief Processes one bounded block of unformatted SINGLE trace input. */
  void processSingleBlock(const std::uint8_t* data, std::uint32_t size)
  {
    SingleBlockState state;
    while (state.processed < size) {
      const auto result = pushSingleData(data + state.processed, size - state.processed);
      updateSingleProgress(state, result);
      if (result.decision.action == OpenCsdErrorController::Action::RecoverStream) {
        recoverSingleStream(state, result);
      } else if (result.decision.action == OpenCsdErrorController::Action::Wait) {
        drainSingleWait(state, result);
      } else if (result.consumed == 0U) {
        recoverSingleNoProgress();
      } else if (m_collector.transactionElementCount() == 0U) {
        consumeSilentSingleData(state, result);
      } else {
        commitSingleData(state, result);
      }
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
    m_collector.clearPacketErrorContexts();
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
                                     OpenCsdUnsupportedTraceIdObserver unsupportedTraceIdObserver,
                                     OpenCsdSkippedBytesObserver skippedBytesObserver)
  : m_impl(std::make_unique<OpenCsdItmDecoderImpl>(std::move(routes), inputMode, elementSink,
                                                   OpenCsdItmSessionFactory{}, std::move(unsupportedTraceIdObserver),
                                                   std::move(skippedBytesObserver)))
{
}

OpenCsdItmDecoder::OpenCsdItmDecoder(std::vector<TraceRouteIdentity> routes, OpenCsdItmInputMode inputMode,
                                     OpenCsdTraceElementSink& elementSink,
                                     const OpenCsdItmSessionFactory& sessionFactory)
  : m_impl(std::make_unique<OpenCsdItmDecoderImpl>(std::move(routes), inputMode, elementSink, sessionFactory,
                                                   OpenCsdUnsupportedTraceIdObserver{}, OpenCsdSkippedBytesObserver{}))
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
