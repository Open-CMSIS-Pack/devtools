/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#pragma once

#include "TraceEvent.hpp"

#include <cstdint>
#include <string>

enum class LocalTimestampRelation {
  Synchronous,
  TimestampDelayed,
  PayloadDelayed,
  TimestampAndPayloadDelayed,
};

struct OpenCsdTraceElement {
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

class OpenCsdTraceElementSink {
public:
  virtual ~OpenCsdTraceElementSink() = default;

  virtual void append(OpenCsdTraceElement element) = 0;
};
