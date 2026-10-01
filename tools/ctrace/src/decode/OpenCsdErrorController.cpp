/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "OpenCsdErrorController.h"

#include "TraceEvent.h"
#include "common/ocsd_error.h"
#include "common/ocsd_error_logger.h"
#include "opencsd/ocsd_if_types.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <utility>

/** @brief Collapses whitespace to keep native diagnostics on one readable line. */
static std::string normalizeWhitespace(const std::string& value)
{
  std::string normalized;
  bool pendingSpace = false;
  for (const unsigned char character : value) {
    if (std::isspace(character)) {
      pendingSpace = !normalized.empty();
    } else {
      if (pendingSpace) {
        normalized += ' ';
        pendingSpace = false;
      }
      normalized += static_cast<char>(character);
    }
  }
  return normalized;
}

/** @brief Uses OpenCSD itself to format one native error enum. */
static std::string openCsdErrorText(const OpenCsdErrorRecord& error)
{
  const ocsdError nativeError(error.severity, error.code, error.message);
  return normalizeWhitespace(ocsdError::getErrorString(nativeError));
}

OpenCsdErrorController::OpenCsdErrorController()
{
  // Without an owned output logger, initialization only sets the verbosity and cannot fail.
  static_cast<void>(initErrorLogger(OCSD_ERR_SEV_INFO));
}

void OpenCsdErrorController::beginDataPathCall()
{
  m_callErrors.clear();
}

void OpenCsdErrorController::setCallbackOrderSource(std::function<std::optional<std::uint64_t>()> callbackOrderSource)
{
  m_callbackOrderSource = std::move(callbackOrderSource);
}

OpenCsdErrorController::Decision OpenCsdErrorController::decide(ocsd_datapath_resp_t response) const
{
  Decision decision;
  decision.response = response;
  decision.errors = m_callErrors;

  const auto recoverable = std::find_if(m_callErrors.begin(), m_callErrors.end(), [](const auto& error) {
    return error.severity == OCSD_ERR_SEV_ERROR && isRecoverableStreamError(error.code);
  });
  const auto nonRecoverableError = std::find_if(m_callErrors.rbegin(), m_callErrors.rend(), [](const auto& error) {
    return error.severity == OCSD_ERR_SEV_ERROR && !isRecoverableStreamError(error.code);
  });
  if (OCSD_DATA_RESP_IS_FATAL(response) && nonRecoverableError != m_callErrors.rend()) {
    decision.error = *nonRecoverableError;
  } else if (recoverable != m_callErrors.end()) {
    decision.error = *recoverable;
  } else if (!m_callErrors.empty()) {
    decision.error = m_callErrors.back();
  }

  if (OCSD_DATA_RESP_IS_FATAL(response)) {
    decision.action = response == OCSD_RESP_FATAL_INVALID_DATA && nonRecoverableError == m_callErrors.rend() &&
                              decision.error.has_value() && decision.error->severity == OCSD_ERR_SEV_ERROR &&
                              isRecoverableStreamError(decision.error->code)
                          ? Action::RecoverStream
                          : Action::Abort;
    return decision;
  }
  if (recoverable != m_callErrors.end()) {
    decision.action = Action::RecoverStream;
    return decision;
  }
  if (OCSD_DATA_RESP_IS_WAIT(response)) {
    decision.action = Action::Wait;
    return decision;
  }
  decision.action = Action::Continue;
  return decision;
}

bool OpenCsdErrorController::isRecoverableStreamError(ocsd_err_t code)
{
  return code == OCSD_ERR_BAD_PACKET_SEQ || code == OCSD_ERR_INVALID_PCKT_HDR;
}

bool OpenCsdErrorController::responseReportsError(ocsd_datapath_resp_t response)
{
  return response == OCSD_RESP_ERR_CONT || response == OCSD_RESP_ERR_WAIT || OCSD_DATA_RESP_IS_FATAL(response);
}

std::uint64_t OpenCsdErrorController::errorOffset(const Decision& decision, std::uint64_t fallback)
{
  return decision.error.has_value() && decision.error->hasIndex ? decision.error->index : fallback;
}

TraceIssueCode OpenCsdErrorController::issueCode(const Decision& decision)
{
  if (!decision.error.has_value()) {
    return TraceIssueCode::OpenCsdDecodeError;
  }
  switch (decision.error->code) {
  case OCSD_ERR_BAD_PACKET_SEQ:
    return TraceIssueCode::OpenCsdBadPacketSequence;
  case OCSD_ERR_INVALID_PCKT_HDR:
    return TraceIssueCode::OpenCsdInvalidPacketHeader;
  default:
    return TraceIssueCode::OpenCsdDecodeError;
  }
}

std::string OpenCsdErrorController::describeApiError(ocsd_err_t code, const std::string& message)
{
  OpenCsdErrorRecord error;
  error.code = code;
  error.message = message;
  return openCsdErrorText(error);
}

std::string OpenCsdErrorController::describeSummary(const Decision& decision)
{
  std::string summary;
  if (decision.error.has_value()) {
    switch (decision.error->code) {
    case OCSD_ERR_BAD_PACKET_SEQ:
      summary = "OpenCSD detected an invalid ITM packet sequence";
      break;
    case OCSD_ERR_INVALID_PCKT_HDR:
      summary = "OpenCSD detected an invalid ITM packet header";
      break;
    case OCSD_ERR_NOT_INIT:
      summary = "OpenCSD decoder is not initialized";
      break;
    case OCSD_ERR_MEM:
      summary = "OpenCSD decoder ran out of memory";
      break;
    case OCSD_ERR_INVALID_PARAM_VAL:
    case OCSD_ERR_INVALID_PARAM_TYPE:
      summary = "OpenCSD rejected a decoder parameter";
      break;
    case OCSD_ERR_FILE_ERROR:
    case OCSD_ERR_RDR_FILE_NOT_FOUND:
      summary = "OpenCSD could not read required input data";
      break;
    case OCSD_ERR_DATA_DECODE_FATAL:
      summary = "OpenCSD could not decode the trace data";
      break;
    default:
      summary = "OpenCSD decoder error";
      break;
    }
  } else {
    switch (decision.response) {
    case OCSD_RESP_WARN_CONT:
    case OCSD_RESP_WARN_WAIT:
      summary = "OpenCSD decoder warning";
      break;
    case OCSD_RESP_FATAL_NOT_INIT:
      summary = "OpenCSD decoder is not initialized";
      break;
    case OCSD_RESP_FATAL_INVALID_OP:
      summary = "OpenCSD rejected a decoder operation";
      break;
    case OCSD_RESP_FATAL_INVALID_PARAM:
      summary = "OpenCSD rejected a decoder parameter";
      break;
    case OCSD_RESP_FATAL_INVALID_DATA:
      summary = "OpenCSD rejected invalid trace data";
      break;
    case OCSD_RESP_FATAL_SYS_ERR:
      summary = "OpenCSD reported a system error";
      break;
    default:
      summary = "OpenCSD decoder error";
      break;
    }
  }
  const auto offset = errorOffset(decision, 0U);
  if ((decision.error.has_value() && decision.error->hasIndex) || offset != 0U) {
    summary += " at raw offset " + std::to_string(offset);
  }
  const auto nativeText = decision.error.has_value()
                              ? openCsdErrorText(*decision.error)
                              : normalizeWhitespace(ocsdDataRespStr(decision.response).getStr());
  return summary + ". " + nativeText;
}

void OpenCsdErrorController::LogError(ocsd_hndl_err_log_t handle, const ocsdError* error)
{
  if (error == nullptr) {
    return;
  }
  auto record = makeRecord(*error);
  if (m_callbackOrderSource) {
    record.callbackOrder = m_callbackOrderSource();
  }
  m_callErrors.push_back(std::move(record));
  ocsdDefaultErrorLogger::LogError(handle, error);
}

OpenCsdErrorRecord OpenCsdErrorController::makeRecord(const ocsdError& error)
{
  OpenCsdErrorRecord record;
  record.severity = error.getErrorSeverity();
  record.code = error.getErrorCode();
  const auto index = error.getErrorIndex();
  record.hasIndex = index != OCSD_BAD_TRC_INDEX;
  record.index = record.hasIndex ? static_cast<std::uint64_t>(index) : 0U;
  const auto channel = error.getErrorChanID();
  if (channel != OCSD_BAD_CS_SRC_ID) {
    record.channel = channel;
  }
  record.message = normalizeWhitespace(error.getMessage());
  return record;
}
