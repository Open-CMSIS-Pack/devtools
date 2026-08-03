/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_DECODE_OPENCSDTRACEELEMENT_H
#define CTRACE_SRC_DECODE_OPENCSDTRACEELEMENT_H

#include "TraceEvent.h"

#include <cstdint>
#include <string>

/** @brief Describes ordering between a local timestamp and adjacent payloads. */
enum class LocalTimestampRelation {
  Synchronous,
  TimestampDelayed,
  PayloadDelayed,
  TimestampAndPayloadDelayed,
};

/** @brief Stores one normalized element received from OpenCSD callbacks. */
struct OpenCsdTraceElement {
  /** @brief Identifies the normalized element payload. */
  enum class Kind {
    Software,
    Hardware,
    LocalTimestamp,
    GlobalTimestamp,
    Sync,
    Overflow,
    Discontinuity,
    Error,
  };

  Kind kind = Kind::Error;
  std::uint64_t sourceIndex = 0;

  // CoreSight Trace Bus ID reported by OpenCSD. ID 0 identifies input for
  // which no formatted source ID exists, such as the current SWO path.
  std::uint8_t traceBusId = 0U;
  std::uint32_t channel = 0;
  std::uint32_t discriminator = 0;
  std::uint8_t size = 0;
  std::uint32_t value = 0;
  LocalTimestampRelation timestampRelation = LocalTimestampRelation::Synchronous;
  std::uint64_t timestampValue = 0;
  std::optional<std::uint64_t> tcyc;
  std::optional<std::uint64_t> rawBytesConsumed;
  bool overflow = false;
  bool discontinuity = false;
  bool awaitingResumeTimestamp = false;

  bool clockChange = false;

  std::string issueCode;
  TraceIssueSeverity issueSeverity = TraceIssueSeverity::Error;
  std::string errorMessage;
};

/** @brief Receives normalized OpenCSD trace elements. */
class OpenCsdTraceElementSink {
public:
  /** @brief Destroys an element sink through its interface. */
  virtual ~OpenCsdTraceElementSink() = default;

  /** @brief Appends one normalized OpenCSD element. */
  virtual void append(OpenCsdTraceElement element) = 0;
};

#endif  // CTRACE_SRC_DECODE_OPENCSDTRACEELEMENT_H
