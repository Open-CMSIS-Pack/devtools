/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_DECODE_OPENCSDERRORCONTROLLER_H
#define CTRACE_SRC_DECODE_OPENCSDERRORCONTROLLER_H

#include "TraceEvent.h"
#include "common/ocsd_error_logger.h"
#include "opencsd/ocsd_if_types.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

/** @brief Stores the stable subset of one OpenCSD error callback. */
struct OpenCsdErrorRecord {
  ocsd_err_severity_t severity = OCSD_ERR_SEV_ERROR;
  ocsd_err_t code = OCSD_OK;
  std::uint64_t index = 0;
  bool hasIndex = false;
  std::string message;
};

/** @brief Captures OpenCSD errors and decides whether decoding can recover. */
class OpenCsdErrorController final : public ocsdDefaultErrorLogger {
public:
  /** @brief Creates and configures the OpenCSD error logger. */
  OpenCsdErrorController();

  /** @brief Defines the decoder action selected after an OpenCSD call. */
  enum class Action {
    Continue,
    Wait,
    RecoverStream,
    Abort,
  };

  /** @brief Combines an OpenCSD response with captured errors and the selected action. */
  struct Decision {
    Action action = Action::Continue;
    ocsd_datapath_resp_t response = OCSD_RESP_CONT;
    std::optional<OpenCsdErrorRecord> error;
    std::vector<OpenCsdErrorRecord> errors;
  };

  /** @brief Clears errors before invoking one OpenCSD data-path operation. */
  void beginDataPathCall();
  /** @brief Classifies the response and errors from the current data-path operation. */
  Decision decide(ocsd_datapath_resp_t response) const;

  /** @brief Tests whether an OpenCSD error permits stream resynchronization. */
  static bool isRecoverableStreamError(ocsd_err_t code);
  /** @brief Tests whether an OpenCSD response reports an error condition. */
  static bool responseReportsError(ocsd_datapath_resp_t response);
  /** @brief Returns the captured raw offset or a supplied fallback. */
  static std::uint64_t errorOffset(const Decision& decision, std::uint64_t fallback);
  /** @brief Returns the ctrace issue state for a decision. */
  static TraceIssueCode issueCode(const Decision& decision);
  /** @brief Formats an OpenCSD API setup failure. */
  static std::string describeApiError(ocsd_err_t code, const std::string& message);
  /** @brief Formats the user-facing summary for a decoder decision. */
  static std::string describeSummary(const Decision& decision);

  /** @brief Captures an OpenCSD callback while preserving default logging behavior. */
  void LogError(ocsd_hndl_err_log_t handle, const ocsdError* error) override;

private:
  /** @brief Copies stable fields from an OpenCSD error callback. */
  static OpenCsdErrorRecord makeRecord(const ocsdError& error);

  std::vector<OpenCsdErrorRecord> m_callErrors;
};

#endif // CTRACE_SRC_DECODE_OPENCSDERRORCONTROLLER_H
