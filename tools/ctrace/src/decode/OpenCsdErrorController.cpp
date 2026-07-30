/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "OpenCsdErrorController.hpp"

#include "common/ocsd_error.h"
#include "common/ocsd_msg_logger.h"
#include "opencsd/ocsd_if_types.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <sstream>
#include <string>

namespace {

std::string trimTrailingWhitespace(std::string value)
{
  while (!value.empty() &&
         (value.back() == '\n' || value.back() == '\r' || value.back() == ' ' || value.back() == '\t')) {
    value.pop_back();
  }
  return value;
}

struct OpenCsdErrorName {
  ocsd_err_t code;
  const char* name;
};

constexpr OpenCsdErrorName kOpenCsdErrorNames[] = {
    {OCSD_OK, "OCSD_OK"},
    {OCSD_ERR_FAIL, "OCSD_ERR_FAIL"},
    {OCSD_ERR_MEM, "OCSD_ERR_MEM"},
    {OCSD_ERR_NOT_INIT, "OCSD_ERR_NOT_INIT"},
    {OCSD_ERR_INVALID_ID, "OCSD_ERR_INVALID_ID"},
    {OCSD_ERR_BAD_HANDLE, "OCSD_ERR_BAD_HANDLE"},
    {OCSD_ERR_INVALID_PARAM_VAL, "OCSD_ERR_INVALID_PARAM_VAL"},
    {OCSD_ERR_INVALID_PARAM_TYPE, "OCSD_ERR_INVALID_PARAM_TYPE"},
    {OCSD_ERR_FILE_ERROR, "OCSD_ERR_FILE_ERROR"},
    {OCSD_ERR_NO_PROTOCOL, "OCSD_ERR_NO_PROTOCOL"},
    {OCSD_ERR_ATTACH_TOO_MANY, "OCSD_ERR_ATTACH_TOO_MANY"},
    {OCSD_ERR_ATTACH_INVALID_PARAM, "OCSD_ERR_ATTACH_INVALID_PARAM"},
    {OCSD_ERR_ATTACH_COMP_NOT_FOUND, "OCSD_ERR_ATTACH_COMP_NOT_FOUND"},
    {OCSD_ERR_RDR_FILE_NOT_FOUND, "OCSD_ERR_RDR_FILE_NOT_FOUND"},
    {OCSD_ERR_RDR_INVALID_INIT, "OCSD_ERR_RDR_INVALID_INIT"},
    {OCSD_ERR_RDR_NO_DECODER, "OCSD_ERR_RDR_NO_DECODER"},
    {OCSD_ERR_DATA_DECODE_FATAL, "OCSD_ERR_DATA_DECODE_FATAL"},
    {OCSD_ERR_DFMTR_NOTCONTTRACE, "OCSD_ERR_DFMTR_NOTCONTTRACE"},
    {OCSD_ERR_DFMTR_BAD_FHSYNC, "OCSD_ERR_DFMTR_BAD_FHSYNC"},
    {OCSD_ERR_BAD_PACKET_SEQ, "OCSD_ERR_BAD_PACKET_SEQ"},
    {OCSD_ERR_INVALID_PCKT_HDR, "OCSD_ERR_INVALID_PCKT_HDR"},
    {OCSD_ERR_PKT_INTERP_FAIL, "OCSD_ERR_PKT_INTERP_FAIL"},
    {OCSD_ERR_UNSUPPORTED_ISA, "OCSD_ERR_UNSUPPORTED_ISA"},
    {OCSD_ERR_HW_CFG_UNSUPP, "OCSD_ERR_HW_CFG_UNSUPP"},
    {OCSD_ERR_UNSUPP_DECODE_PKT, "OCSD_ERR_UNSUPP_DECODE_PKT"},
    {OCSD_ERR_BAD_DECODE_PKT, "OCSD_ERR_BAD_DECODE_PKT"},
    {OCSD_ERR_COMMIT_PKT_OVERRUN, "OCSD_ERR_COMMIT_PKT_OVERRUN"},
    {OCSD_ERR_MEM_NACC, "OCSD_ERR_MEM_NACC"},
    {OCSD_ERR_RET_STACK_OVERFLOW, "OCSD_ERR_RET_STACK_OVERFLOW"},
    {OCSD_ERR_DCDT_NO_FORMATTER, "OCSD_ERR_DCDT_NO_FORMATTER"},
    {OCSD_ERR_MEM_ACC_OVERLAP, "OCSD_ERR_MEM_ACC_OVERLAP"},
    {OCSD_ERR_MEM_ACC_FILE_NOT_FOUND, "OCSD_ERR_MEM_ACC_FILE_NOT_FOUND"},
    {OCSD_ERR_MEM_ACC_FILE_DIFF_RANGE, "OCSD_ERR_MEM_ACC_FILE_DIFF_RANGE"},
    {OCSD_ERR_MEM_ACC_RANGE_INVALID, "OCSD_ERR_MEM_ACC_RANGE_INVALID"},
    {OCSD_ERR_MEM_ACC_BAD_LEN, "OCSD_ERR_MEM_ACC_BAD_LEN"},
    {OCSD_ERR_TEST_SNAPSHOT_PARSE, "OCSD_ERR_TEST_SNAPSHOT_PARSE"},
    {OCSD_ERR_TEST_SNAPSHOT_PARSE_INFO, "OCSD_ERR_TEST_SNAPSHOT_PARSE_INFO"},
    {OCSD_ERR_TEST_SNAPSHOT_READ, "OCSD_ERR_TEST_SNAPSHOT_READ"},
    {OCSD_ERR_TEST_SS_TO_DECODER, "OCSD_ERR_TEST_SS_TO_DECODER"},
    {OCSD_ERR_DCDREG_NAME_REPEAT, "OCSD_ERR_DCDREG_NAME_REPEAT"},
    {OCSD_ERR_DCDREG_NAME_UNKNOWN, "OCSD_ERR_DCDREG_NAME_UNKNOWN"},
    {OCSD_ERR_DCDREG_TYPE_UNKNOWN, "OCSD_ERR_DCDREG_TYPE_UNKNOWN"},
    {OCSD_ERR_DCDREG_TOOMANY, "OCSD_ERR_DCDREG_TOOMANY"},
    {OCSD_ERR_DCD_INTERFACE_UNUSED, "OCSD_ERR_DCD_INTERFACE_UNUSED"},
    {OCSD_ERR_INVALID_OPCODE, "OCSD_ERR_INVALID_OPCODE"},
    {OCSD_ERR_I_RANGE_LIMIT_OVERRUN, "OCSD_ERR_I_RANGE_LIMIT_OVERRUN"},
    {OCSD_ERR_BAD_DECODE_IMAGE, "OCSD_ERR_BAD_DECODE_IMAGE"},
    {OCSD_ERR_LAST, "OCSD_ERR_LAST"},
};

} // namespace

void OpenCsdErrorController::beginDataPathCall()
{
  callErrors_.clear();
}

OpenCsdErrorController::Decision OpenCsdErrorController::decide(ocsd_datapath_resp_t response) const
{
  Decision decision;
  decision.response = response;
  decision.errors = callErrors_;

  const auto recoverable = std::find_if(callErrors_.begin(), callErrors_.end(), [](const auto& error) {
    return error.severity == OCSD_ERR_SEV_ERROR && isRecoverableStreamError(error.code);
  });
  const auto nonRecoverableError = std::find_if(callErrors_.rbegin(), callErrors_.rend(), [](const auto& error) {
    return error.severity == OCSD_ERR_SEV_ERROR && !isRecoverableStreamError(error.code);
  });
  if (OCSD_DATA_RESP_IS_FATAL(response) && nonRecoverableError != callErrors_.rend()) {
    decision.error = *nonRecoverableError;
  } else if (recoverable != callErrors_.end()) {
    decision.error = *recoverable;
  } else if (!callErrors_.empty()) {
    decision.error = callErrors_.back();
  }

  if (OCSD_DATA_RESP_IS_FATAL(response)) {
    decision.action = response == OCSD_RESP_FATAL_INVALID_DATA && nonRecoverableError == callErrors_.rend() &&
                              decision.error.has_value() && decision.error->severity == OCSD_ERR_SEV_ERROR &&
                              isRecoverableStreamError(decision.error->code)
                          ? Action::RecoverStream
                          : Action::Abort;
    return decision;
  }
  if (recoverable != callErrors_.end()) {
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

std::string OpenCsdErrorController::issueCode(const Decision& decision)
{
  if (!decision.error.has_value()) {
    return "opencsd-decode-error";
  }
  switch (decision.error->code) {
  case OCSD_ERR_BAD_PACKET_SEQ:
    return "opencsd-bad-packet-sequence";
  case OCSD_ERR_INVALID_PCKT_HDR:
    return "opencsd-invalid-packet-header";
  default:
    return "opencsd-decode-error";
  }
}

std::string OpenCsdErrorController::describeApiError(ocsd_err_t code, const std::string& message)
{
  return message + " (" + errorCodeName(code) + ")";
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

std::string OpenCsdErrorController::responseName(ocsd_datapath_resp_t response)
{
  switch (response) {
  case OCSD_RESP_CONT:
    return "OCSD_RESP_CONT";
  case OCSD_RESP_WARN_CONT:
    return "OCSD_RESP_WARN_CONT";
  case OCSD_RESP_ERR_CONT:
    return "OCSD_RESP_ERR_CONT";
  case OCSD_RESP_WAIT:
    return "OCSD_RESP_WAIT";
  case OCSD_RESP_WARN_WAIT:
    return "OCSD_RESP_WARN_WAIT";
  case OCSD_RESP_ERR_WAIT:
    return "OCSD_RESP_ERR_WAIT";
  case OCSD_RESP_FATAL_NOT_INIT:
    return "OCSD_RESP_FATAL_NOT_INIT";
  case OCSD_RESP_FATAL_INVALID_OP:
    return "OCSD_RESP_FATAL_INVALID_OP";
  case OCSD_RESP_FATAL_INVALID_PARAM:
    return "OCSD_RESP_FATAL_INVALID_PARAM";
  case OCSD_RESP_FATAL_INVALID_DATA:
    return "OCSD_RESP_FATAL_INVALID_DATA";
  case OCSD_RESP_FATAL_SYS_ERR:
    return "OCSD_RESP_FATAL_SYS_ERR";
  }
  return "OCSD_RESP_UNKNOWN(" + std::to_string(static_cast<int>(response)) + ")";
}

std::string OpenCsdErrorController::errorCodeName(ocsd_err_t code)
{
  const auto found = std::find_if(std::begin(kOpenCsdErrorNames), std::end(kOpenCsdErrorNames),
                                  [code](const auto& entry) { return entry.code == code; });
  if (found != std::end(kOpenCsdErrorNames)) {
    return found->name;
  }
  return "OCSD_ERR_UNKNOWN(" + std::to_string(static_cast<int>(code)) + ")";
}

std::string OpenCsdErrorController::describe(const Decision& decision)
{
  std::ostringstream out;
  out << responseName(decision.response);
  if (!decision.error.has_value()) {
    return out.str();
  }
  const auto& error = *decision.error;
  out << ": " << errorCodeName(error.code);
  if (error.hasIndex) {
    out << " at raw offset " << error.index;
  }
  if (!error.message.empty()) {
    out << ": " << error.message;
  }
  return out.str();
}

OpenCsdErrorController::ErrorSourceHandleResult
OpenCsdErrorController::RegisterErrorSource(const std::string& componentName)
{
  const auto existing = std::find(sources_.begin(), sources_.end(), componentName);
  if (existing != sources_.end()) {
    return static_cast<ocsd_hndl_err_log_t>(std::distance(sources_.begin(), existing));
  }
  const auto handle = static_cast<ocsd_hndl_err_log_t>(sources_.size());
  sources_.push_back(componentName);
  return handle;
}

OpenCsdErrorController::ErrorLogVerbosityResult OpenCsdErrorController::GetErrorLogVerbosity() const
{
  return OCSD_ERR_SEV_INFO;
}

void OpenCsdErrorController::LogError(ocsd_hndl_err_log_t, const ocsdError* error)
{
  if (error == nullptr) {
    return;
  }
  callErrors_.push_back(makeRecord(*error));
  lastError_ = *error;
  if (OCSD_IS_VALID_CS_SRC_ID(error->getErrorChanID())) {
    lastTraceErrors_[error->getErrorChanID()] = *error;
  }
}

void OpenCsdErrorController::LogMessage(ocsd_hndl_err_log_t handle, ocsd_err_severity_t filterLevel,
                                        const std::string& message)
{
  if (outputLogger_ != nullptr && GetErrorLogVerbosity() >= filterLevel && outputLogger_->isLogging()) {
    outputLogger_->LogMsg(sourceName(handle) + ": " + message);
  }
}

ocsdError* OpenCsdErrorController::GetLastError()
{
  return lastError_.has_value() ? &*lastError_ : nullptr;
}

ocsdError* OpenCsdErrorController::GetLastIDError(std::uint8_t traceBusId)
{
  if (!OCSD_IS_VALID_CS_SRC_ID(traceBusId)) {
    return nullptr;
  }
  auto& error = lastTraceErrors_[traceBusId];
  return error.has_value() ? &error.value() : nullptr;
}

ocsdMsgLogger* OpenCsdErrorController::getOutputLogger()
{
  return outputLogger_;
}

void OpenCsdErrorController::setOutputLogger(ocsdMsgLogger* logger)
{
  outputLogger_ = logger;
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

std::string OpenCsdErrorController::sourceName(ocsd_hndl_err_log_t handle) const
{
  if (handle < sources_.size()) {
    return sources_[handle];
  }
  return "OpenCSD";
}
