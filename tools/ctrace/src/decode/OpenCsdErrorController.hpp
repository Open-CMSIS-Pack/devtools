/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#pragma once

#include "common/ocsd_error.h"
#include "common/ocsd_msg_logger.h"
#include "interfaces/trc_error_log_i.h"
#include "opencsd/ocsd_if_types.h"

#include <array>
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

class OpenCsdErrorController final : public ITraceErrorLog {
  // OpenCSD declares these scalar return types with top-level const.  Keep
  // the exact override type without repeating the ineffective qualifier at
  // every declaration and definition.
  using ErrorSourceHandleResult = const ocsd_hndl_err_log_t;
  using ErrorLogVerbosityResult = const ocsd_err_severity_t;

public:
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

  ErrorSourceHandleResult RegisterErrorSource(const std::string& componentName) override;
  ErrorLogVerbosityResult GetErrorLogVerbosity() const override;
  void LogError(ocsd_hndl_err_log_t, const ocsdError* error) override;
  void LogMessage(ocsd_hndl_err_log_t handle, ocsd_err_severity_t filterLevel, const std::string& message) override;
  ocsdError* GetLastError() override;
  ocsdError* GetLastIDError(std::uint8_t traceBusId) override;
  ocsdMsgLogger* getOutputLogger() override;
  void setOutputLogger(ocsdMsgLogger* logger) override;

private:
  static OpenCsdErrorRecord makeRecord(const ocsdError& error);
  std::string sourceName(ocsd_hndl_err_log_t handle) const;

  std::vector<std::string> sources_{"Gen_Err", "Gen_Warn", "Gen_Info"};
  std::vector<OpenCsdErrorRecord> callErrors_;
  std::optional<ocsdError> lastError_;
  std::array<std::optional<ocsdError>, 0x80> lastTraceErrors_{};
  ocsdMsgLogger* outputLogger_ = nullptr;
};
