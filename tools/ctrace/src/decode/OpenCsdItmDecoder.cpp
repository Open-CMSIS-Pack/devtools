/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "OpenCsdItmDecoder.hpp"

#include "TraceEvent.hpp"
#include "OpenCsdErrorController.hpp"
#include "OpenCsdPacketCollector.hpp"
#include "OpenCsdItmSession.hpp"
#include "OpenCsdTraceElement.hpp"
#include "opencsd/ocsd_if_types.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

static_assert(sizeof(ocsd_trc_index_t) == sizeof(std::uint64_t), "ctrace requires 64-bit OpenCSD trace indices");

class OpenCsdItmDecoderImpl {
public:
  OpenCsdItmDecoderImpl(OpenCsdTraceElementSink& elementSink, const OpenCsdItmSessionFactory& sessionFactory)
    : collector_(elementSink)
  {
    try {
      session_ = sessionFactory(collector_, errorController_);
      if (session_ == nullptr) {
        failInitialization("OpenCSD ITM session factory returned no session");
      }
    } catch (const OpenCsdItmSessionError& error) {
      failInitialization(error.what());
    }
  }

  void push(const std::uint8_t* data, std::uint32_t size)
  {
    if (finished_) {
      throw std::runtime_error("OpenCSD ITM decoder already finished");
    }
    std::uint32_t offset = 0;
    while (offset < size) {
      const auto span = std::min(kMaxTraceDataInBytes, size - offset);
      processBlock(data + offset, span);
      offset += span;
    }
    result_.bytesIn = static_cast<std::uint64_t>(traceIndex_);
  }

  OpenCsdItmDecodeResult finish()
  {
    if (finished_) {
      return result_;
    }
    completeConsumedDataLoss(traceIndex_);
    collector_.beginTransaction();
    errorController_.beginDataPathCall();
    const auto response = session_->endOfTrace();
    collector_.rethrowOutputError();
    const auto decision = errorController_.decide(response);
    if (decision.action == OpenCsdErrorController::Action::Abort) {
      abortDecode(decision, 0U, traceIndex_, 0U, "OpenCSD aborted end-of-trace processing: ");
    }
    if (decision.action == OpenCsdErrorController::Action::RecoverStream) {
      const auto sourceOffset = OpenCsdErrorController::errorOffset(decision, traceIndex_);
      collector_.commitTransactionBefore(sourceOffset);
      appendReportedErrors(decision, traceIndex_, true);
    } else {
      collector_.commitTransaction();
      appendReportedErrors(decision, traceIndex_, false);
      if (decision.action == OpenCsdErrorController::Action::Wait) {
        flushAfterWait();
      }
    }
    finished_ = true;
    result_.bytesIn = static_cast<std::uint64_t>(traceIndex_);
    return result_;
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
      collector_.appendDecodeError(static_cast<ocsd_trc_index_t>(sourceOffset), // LCOV_EXCL_BR_LINE
                                   OpenCsdErrorController::describeSummary(item),
                                   OpenCsdErrorController::issueCode(item),
                                   nextDiscontinuity && isError, // LCOV_EXCL_BR_LINE
                                   isError ? TraceIssueSeverity::Error : TraceIssueSeverity::Warning);
      if (isError) {
        emittedError = true;
        nextDiscontinuity = false;
      }
    }
    if (!emittedError && (force || OpenCsdErrorController::responseReportsError(decision.response))) {
      collector_.appendDecodeError(
          static_cast<ocsd_trc_index_t>(OpenCsdErrorController::errorOffset(decision, baseOffset)),
          OpenCsdErrorController::describeSummary(decision), OpenCsdErrorController::issueCode(decision), discontinuity,
          TraceIssueSeverity::Error);
    }
  }

  [[noreturn]] void abortDecode(const OpenCsdErrorController::Decision& decision, std::uint32_t size,
                                std::uint64_t baseOffset, std::uint32_t bytesConsumed, const std::string& prefix)
  {
    collector_.rollbackTransaction();
    completeConsumedDataLoss(OpenCsdErrorController::errorOffset(decision, baseOffset));
    appendReportedErrors(decision, baseOffset, true);
    const auto processed = baseOffset + std::min<std::uint64_t>(bytesConsumed, size);
    throw OpenCsdFatalError(prefix + OpenCsdErrorController::describe(decision), processed);
  }

  void completeConsumedDataLoss(std::uint64_t resumeOffset)
  {
    if (!consumedDataLossStart_.has_value()) {
      return;
    }
    const auto startOffset = *consumedDataLossStart_;
    const auto endOffset = std::max(startOffset, resumeOffset);
    const auto consumed = endOffset - startOffset;
    consumedDataLossStart_.reset();
    const auto boundaryAlreadyMarked = consumedDataLossBoundaryMarked_;
    consumedDataLossBoundaryMarked_ = false;
    if (consumed == 0U) {
      return;
    }
    const auto message =
        "OpenCSD consumed " + std::to_string(consumed) +
        " raw bytes while waiting for usable ITM trace packets; data loss until a later sync/recovery point";
    if (boundaryAlreadyMarked) {
      collector_.prependDataLossError(static_cast<ocsd_trc_index_t>(startOffset), message, consumed);
    } else {
      collector_.prependDiscontinuity(static_cast<ocsd_trc_index_t>(startOffset), message, "data-loss", consumed);
    }
  }

  void processBlock(const std::uint8_t* data, std::uint32_t size)
  {
    std::uint32_t processed = 0;
    std::optional<std::uint64_t> noProgressRetryOffset;
    while (processed < size) {
      const auto callIndex = static_cast<std::uint64_t>(traceIndex_);
      const auto callSize = size - processed;
      std::uint32_t processedThisPass = 0;
      collector_.beginTransaction();
      errorController_.beginDataPathCall();
      const auto response = session_->pushData(traceIndex_, callSize, data + processed, processedThisPass);
      collector_.rethrowOutputError();
      const auto decision = errorController_.decide(response);
      if (decision.action == OpenCsdErrorController::Action::RecoverStream) {
        const auto sourceOffset = OpenCsdErrorController::errorOffset(decision, callIndex);
        completeConsumedDataLoss(
            std::min(sourceOffset, collector_.transactionFirstSourceOffset().value_or(sourceOffset)));
        // Callbacks before the bad packet remain valid; callbacks at or
        // after its offset belong to the failed decode transaction.
        collector_.commitTransactionBefore(sourceOffset);
        appendReportedErrors(decision, callIndex, true);
        const auto consumed = std::min(processedThisPass, callSize);
        processed += consumed;
        traceIndex_ += consumed;
        dataLossActive_ = true;
        consumedDataLossStart_ = static_cast<std::uint64_t>(traceIndex_);
        consumedDataLossBoundaryMarked_ = true;
        resetDecoder();
        continue;
      }
      if (decision.action == OpenCsdErrorController::Action::Abort) {
        abortDecode(decision, callSize, callIndex, processedThisPass, "OpenCSD aborted decode: ");
      }
      if (decision.action == OpenCsdErrorController::Action::Wait) {
        if (collector_.transactionElementCount() == 0U) {
          collector_.rollbackTransaction();
        } else {
          completeConsumedDataLoss(collector_.transactionFirstSourceOffset().value_or(callIndex));
          collector_.commitTransaction();
        }
        appendReportedErrors(decision, callIndex, false);
        const auto consumed = std::min(processedThisPass, callSize);
        processed += consumed;
        traceIndex_ += consumed;
        flushAfterWait();
        continue;
      }
      if (processedThisPass == 0) {
        collector_.rollbackTransaction();
        if (noProgressRetryOffset.has_value() && *noProgressRetryOffset == traceIndex_) { // LCOV_EXCL_BR_LINE
          completeConsumedDataLoss(traceIndex_);
          collector_.appendDecodeError(traceIndex_, "OpenCSD made no progress after decoder reset; decode aborted",
                                       "opencsd-no-progress", false);
          throw OpenCsdFatalError("OpenCSD made no progress after decoder reset",
                                  static_cast<std::uint64_t>(traceIndex_));
        }
        collector_.appendDecodeError(traceIndex_,
                                     "OpenCSD made no progress while raw data was present; decoder reset and searching "
                                     "for next real ITM async sync",
                                     "opencsd-no-progress");
        noProgressRetryOffset = static_cast<std::uint64_t>(traceIndex_);
        dataLossActive_ = true;
        consumedDataLossStart_ = static_cast<std::uint64_t>(traceIndex_);
        consumedDataLossBoundaryMarked_ = true;
        resetDecoder();
        continue;
      }
      if (collector_.transactionElementCount() == 0U) {
        collector_.rollbackTransaction();
        appendReportedErrors(decision, callIndex, false);
        if (!dataLossActive_) {
          consumedDataLossStart_ = static_cast<std::uint64_t>(traceIndex_);
          consumedDataLossBoundaryMarked_ = false;
          dataLossActive_ = true;
        }
        processed += processedThisPass;
        traceIndex_ += processedThisPass;
        continue;
      }
      completeConsumedDataLoss(collector_.transactionFirstSourceOffset().value_or(callIndex));
      collector_.commitTransaction();
      appendReportedErrors(decision, callIndex, false);
      dataLossActive_ = false;
      processed += processedThisPass;
      traceIndex_ += processedThisPass;
      noProgressRetryOffset.reset();
    }
  }

  void flushAfterWait()
  {
    // Bound backpressure handling so a broken decoder cannot stall a file forever.
    static constexpr std::uint32_t kMaxFlushCalls = 1024U;
    for (std::uint32_t call = 0; call < kMaxFlushCalls; ++call) {
      collector_.beginTransaction();
      errorController_.beginDataPathCall();
      const auto response = session_->flush();
      collector_.rethrowOutputError();
      const auto decision = errorController_.decide(response);
      if (decision.action == OpenCsdErrorController::Action::Abort) {
        abortDecode(decision, 0U, traceIndex_, 0U, "OpenCSD aborted while flushing a WAIT response: ");
      }
      if (decision.action == OpenCsdErrorController::Action::RecoverStream) {
        const auto sourceOffset = OpenCsdErrorController::errorOffset(decision, traceIndex_);
        collector_.commitTransactionBefore(sourceOffset);
        appendReportedErrors(decision, traceIndex_, true);
        resetDecoder();
        return;
      }
      if (collector_.transactionElementCount() == 0U) {
        collector_.rollbackTransaction();
      } else {
        collector_.commitTransaction();
      }
      appendReportedErrors(decision, traceIndex_, false);
      if (decision.action == OpenCsdErrorController::Action::Continue) {
        return;
      }
    }
    const auto limit = std::to_string(kMaxFlushCalls);
    collector_.appendDecodeError(traceIndex_,
                                 "OpenCSD WAIT did not clear after " + limit + " FLUSH operations; decode aborted",
                                 "opencsd-wait-timeout");
    throw OpenCsdFatalError("OpenCSD WAIT did not clear after " + limit + " FLUSH operations",
                            static_cast<std::uint64_t>(traceIndex_));
  }

  void resetDecoder()
  {
    errorController_.beginDataPathCall();
    const auto response = session_->reset();
    collector_.rethrowOutputError();
    const auto decision = errorController_.decide(response);
    if (decision.action != OpenCsdErrorController::Action::Continue) {
      appendReportedErrors(decision, traceIndex_, true, true);
      throw OpenCsdFatalError("OpenCSD decoder reset failed: " + OpenCsdErrorController::describe(decision),
                              static_cast<std::uint64_t>(traceIndex_));
    }
    appendReportedErrors(decision, traceIndex_, false);
  }

  [[noreturn]] void failInitialization(const std::string& message)
  {
    collector_.appendDecodeError(traceIndex_, message, "opencsd-initialization-error", false);
    throw OpenCsdFatalError(message, static_cast<std::uint64_t>(traceIndex_));
  }

  OpenCsdPacketCollector collector_;
  OpenCsdErrorController errorController_;
  std::unique_ptr<OpenCsdItmSessionInterface> session_;
  ocsd_trc_index_t traceIndex_ = 0;
  bool dataLossActive_ = false;
  std::optional<std::uint64_t> consumedDataLossStart_;
  bool consumedDataLossBoundaryMarked_ = false;
  OpenCsdItmDecodeResult result_;
  bool finished_ = false;
};

OpenCsdItmDecoder::OpenCsdItmDecoder(OpenCsdTraceElementSink& elementSink)
  : OpenCsdItmDecoder(elementSink, [](OpenCsdPacketCollector& collector, OpenCsdErrorController& errorController) {
      return std::make_unique<OpenCsdItmSession>(collector, errorController);
    })
{
}

OpenCsdItmDecoder::OpenCsdItmDecoder(OpenCsdTraceElementSink& elementSink,
                                     const OpenCsdItmSessionFactory& sessionFactory)
  : impl_(std::make_unique<OpenCsdItmDecoderImpl>(elementSink, sessionFactory))
{
}

OpenCsdItmDecoder::~OpenCsdItmDecoder() = default;

void OpenCsdItmDecoder::push(const std::uint8_t* data, std::uint32_t size)
{
  impl_->push(data, size);
}

OpenCsdItmDecodeResult OpenCsdItmDecoder::finish()
{
  return impl_->finish();
}
