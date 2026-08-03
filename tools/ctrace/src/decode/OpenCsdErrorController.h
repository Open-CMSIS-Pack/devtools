/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_DECODE_OPENCSDERRORCONTROLLER_H
#define CTRACE_SRC_DECODE_OPENCSDERRORCONTROLLER_H

#include "common/ocsd_error_logger.h"
#include "opencsd/ocsd_if_types.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct OpenCsdErrorRecord {
  ocsd_err_severity_t severity = OCSD_ERR_SEV_ERROR;
  ocsd_err_t code = OCSD_OK;
  std::uint64_t index = 0;
  bool hasIndex = false;
  std::string message;
};

class OpenCsdErrorController final : public ocsdDefaultErrorLogger {
public:
  OpenCsdErrorController();

  enum class Action {
    Continue,
    Wait,
    RecoverStream,
    Abort,
  };

  struct Decision {
    Action action = Action::Continue;
    ocsd_datapath_resp_t response = OCSD_RESP_CONT;
    std::optional<OpenCsdErrorRecord> error;
    std::vector<OpenCsdErrorRecord> errors;
  };

  void beginDataPathCall();
  Decision decide(ocsd_datapath_resp_t response) const;

  static bool isRecoverableStreamError(ocsd_err_t code);
  static bool responseReportsError(ocsd_datapath_resp_t response);
  static std::string responseName(ocsd_datapath_resp_t response);
  static std::string errorCodeName(ocsd_err_t code);
  static std::uint64_t errorOffset(const Decision& decision, std::uint64_t fallback);
  static std::string issueCode(const Decision& decision);
  static std::string describeApiError(ocsd_err_t code, const std::string& message);
  static std::string describeSummary(const Decision& decision);
  static std::string describe(const Decision& decision);

  void LogError(ocsd_hndl_err_log_t handle, const ocsdError* error) override;

private:
  static OpenCsdErrorRecord makeRecord(const ocsdError& error);

  std::vector<OpenCsdErrorRecord> callErrors_;
};

#endif  // CTRACE_SRC_DECODE_OPENCSDERRORCONTROLLER_H
