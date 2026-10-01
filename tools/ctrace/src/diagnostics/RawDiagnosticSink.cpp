/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "RawDiagnosticSink.h"

#include "CoreSightFormatter.h"

#include <algorithm>
#include <limits>
#include <string>

RawDiagnosticSink::RawDiagnosticSink(DiagnosticSink& target, bool formatted,
                                     std::optional<std::uint64_t> inputSize)
  : m_target(target), m_formatted(formatted), m_inputSize(inputSize)
{
}

void RawDiagnosticSink::write(const Event& event)
{
  auto contextual = event;
  if (event.rawLocation.has_value()) {
    const auto& location = *event.rawLocation;
    auto& context = contextual.detailedContext;
    const auto progress = location.kind == RawDiagnosticLocation::Kind::InputProgress;
    const auto hint = !progress && (m_formatted || location.kind == RawDiagnosticLocation::Kind::FormatterGroup);
    context.emplace_back("raw_offset", std::to_string(location.offset));
    context.emplace_back("position_kind", progress ? "input_progress" : hint ? "formatter_hint" : "exact");

    auto endOffset = location.endOffset;
    if (location.packetSize.has_value()) {
      context.emplace_back("packet_bytes_kind", m_formatted ? "deformatted" : "file");
      if (!hint && !progress && !endOffset.has_value() &&
          *location.packetSize <= std::numeric_limits<std::uint64_t>::max() - location.offset) {
        endOffset = location.offset + *location.packetSize;
      }
    }
    if (endOffset.has_value()) {
      context.emplace_back("raw_end", std::to_string(*endOffset));
    }
    if (location.kind == RawDiagnosticLocation::Kind::Decoder) {
      context.emplace_back("previous_sync_offset", location.previousSyncOffset.has_value()
                                                      ? std::to_string(*location.previousSyncOffset) : "unknown");
      context.emplace_back("next_sync_offset", location.nextSyncOffset.has_value()
                                                  ? std::to_string(*location.nextSyncOffset) : "unknown");
    }

    // The bounded window is for inspection, not a guaranteed decoder restart.
    // Do not invent a readable slice if the input size or position is unknown.
    if (m_inputSize.has_value() && location.offset <= *m_inputSize) {
      constexpr std::uint64_t precedingBytes = 64U;
      constexpr std::uint64_t windowBytes = 128U;
      auto start = location.offset > precedingBytes ? location.offset - precedingBytes : 0U;
      if (m_formatted) {
        start -= start % CoreSightFormatter::kMemoryAlignedFrameSize;
      }
      const auto length = std::min(windowBytes, *m_inputSize - start);
      context.emplace_back("read_offset", std::to_string(start));
      context.emplace_back("read_length", std::to_string(length));
    }
  }
  m_target.report(contextual);
}
