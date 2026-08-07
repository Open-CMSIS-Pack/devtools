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
#include <cstdint>
#include <string>

/** @brief Removes line terminators and trailing whitespace from an OpenCSD message. */
static std::string trimTrailingWhitespace(std::string value)
{
  while (!value.empty() &&
         (value.back() == '\n' || value.back() == '\r' || value.back() == ' ' || value.back() == '\t')) {
    value.pop_back();
  }
  return value;
}

/** @brief Uses OpenCSD itself to format one native error enum. */
static std::string openCsdErrorText(const OpenCsdErrorRecord& error)
{
  const auto nativeError =
      error.hasIndex ? ocsdError(error.severity, error.code, static_cast<ocsd_trc_index_t>(error.index), error.message)
                     : ocsdError(error.severity, error.code, error.message);
  return trimTrailingWhitespace(ocsdError::getErrorString(nativeError));
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
  return openCsdErrorText({OCSD_ERR_SEV_ERROR, code, 0U, false, message});
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
  return summary + ".";
}

void OpenCsdErrorController::LogError(ocsd_hndl_err_log_t handle, const ocsdError* error)
{
  if (error == nullptr) {
    return;
  }
  m_callErrors.push_back(makeRecord(*error));
  ocsdDefaultErrorLogger::LogError(handle, error);
}

OpenCsdErrorRecord OpenCsdErrorController::makeRecord(const ocsdError& error)
{
  OpenCsdErrorRecord record;
  record.severity = error.getErrorSeverity();
  record.code = error.getErrorCode();
  record.hasIndex = error.getErrorIndex() != OCSD_BAD_TRC_INDEX;
  record.index = record.hasIndex ? static_cast<std::uint64_t>(error.getErrorIndex()) : 0U;
  record.message = trimTrailingWhitespace(error.getMessage());
  return record;
}
