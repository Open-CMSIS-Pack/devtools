/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_OUTPUT_CTF_TRACECOMPASSXMLWRITER_H
#define CTRACE_SRC_OUTPUT_CTF_TRACECOMPASSXMLWRITER_H

#include "CtfGraphicalTopic.h"

#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

/** @brief Writes the Trace Compass analysis definition accompanying CTF output. */
class TraceCompassXmlWriter final {
public:
  /** @brief Identifies one graphical analysis block that can be exposed in Trace Compass. */
  using View = CtfGraphicalTopic;

  /** @brief Stores a set of graphical analysis blocks. */
  using ViewMask = std::uint32_t;

  static_assert(static_cast<ViewMask>(View::Count) < std::numeric_limits<ViewMask>::digits,
                "Trace Compass view mask must represent every graphical topic");

  /** @brief Converts one graphical view to its set bit. */
  static constexpr ViewMask viewMask(View view) noexcept
  {
    return ViewMask{1U} << static_cast<ViewMask>(view);
  }

  /** @brief Enables every supported graphical analysis block. */
  static constexpr ViewMask AllViews = (ViewMask{1U} << static_cast<ViewMask>(View::Count)) - 1U;

  /** @brief Identifies one visible route within a capture's independent clock domain. */
  struct ViewRoute {
    std::uint8_t traceBusId = 0U;
    std::string label;
    ViewMask views = AllViews;
    std::string clockUuid;
  };

  /** @brief Writes one analysis for routes identified by canonical lower-case clock UUIDs and Trace Bus IDs. */
  static void writeFile(const std::filesystem::path& path, const std::vector<ViewRoute>& routes);

private:
  /** @brief Prevents construction of this stateless XML utility. */
  TraceCompassXmlWriter() = delete;
};

#endif // CTRACE_SRC_OUTPUT_CTF_TRACECOMPASSXMLWRITER_H
