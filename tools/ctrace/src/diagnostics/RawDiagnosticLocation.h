/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_DIAGNOSTICS_RAWDIAGNOSTICLOCATION_H
#define CTRACE_SRC_DIAGNOSTICS_RAWDIAGNOSTICLOCATION_H

#include <cstdint>
#include <optional>

/** @brief Keeps input positions typed until the input format and size are known. */
struct RawDiagnosticLocation {
  /** @brief Distinguishes decoder indexes, formatter groups, and input progress. */
  enum class Kind {
    Decoder,
    FormatterGroup,
    InputProgress,
  };

  // All positions use zero-based input-byte coordinates. Formatted decoder
  // indexes are hints, not guaranteed physical packet starts.
  std::uint64_t offset = 0U;
  Kind kind = Kind::Decoder;
  /** @brief Optional exclusive interval end; not a discarded-payload count. */
  std::optional<std::uint64_t> endOffset;
  std::optional<std::uint64_t> previousSyncOffset;
  std::optional<std::uint64_t> nextSyncOffset;
  /** @brief Protocol packet size, which excludes formatter bytes. */
  std::optional<std::uint32_t> packetSize;
};

#endif // CTRACE_SRC_DIAGNOSTICS_RAWDIAGNOSTICLOCATION_H
