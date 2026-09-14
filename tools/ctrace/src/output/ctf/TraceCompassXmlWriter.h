/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_OUTPUT_CTF_TRACECOMPASSXMLWRITER_H
#define CTRACE_SRC_OUTPUT_CTF_TRACECOMPASSXMLWRITER_H

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

/** @brief Writes the Trace Compass analysis definition accompanying CTF output. */
class TraceCompassXmlWriter final {
public:
  /** @brief Identifies one graphical analysis block that can be exposed in Trace Compass. */
  enum class View : std::uint32_t {
    DwtValue = 1U << 0U,
    DwtAddress = 1U << 1U,
    DwtMatch = 1U << 2U,
    DwtEvent = 1U << 3U,
    PmuEvent = 1U << 4U,
    Exception = 1U << 5U,
    ProcessorState = 1U << 6U,
  };

  /** @brief Stores a set of graphical analysis blocks. */
  using ViewMask = std::uint32_t;

  /** @brief Converts one graphical view to its set bit. */
  static constexpr ViewMask viewMask(View view) noexcept
  {
    return static_cast<ViewMask>(view);
  }

  /** @brief Enables every supported graphical analysis block. */
  static constexpr ViewMask AllViews = static_cast<ViewMask>(View::DwtValue) |
                                       static_cast<ViewMask>(View::DwtAddress) |
                                       static_cast<ViewMask>(View::DwtMatch) |
                                       static_cast<ViewMask>(View::DwtEvent) |
                                       static_cast<ViewMask>(View::PmuEvent) |
                                       static_cast<ViewMask>(View::Exception) |
                                       static_cast<ViewMask>(View::ProcessorState);

  /** @brief Identifies one visible route without exposing its architectural ID in the label. */
  struct ViewRoute {
    std::uint8_t traceBusId = 0U;
    std::string label;
    ViewMask views = AllViews;
  };

  /** @brief Writes a legacy single-stream analysis definition to a file. */
  static void writeLegacyFile(const std::filesystem::path& path, ViewMask views = AllViews);
  /** @brief Writes a route-prefixed multi-stream analysis definition to a file. */
  static void writeRoutedFile(const std::filesystem::path& path, const std::vector<ViewRoute>& routes);

private:
  /** @brief Prevents construction of this stateless XML utility. */
  TraceCompassXmlWriter() = delete;
};

#endif  // CTRACE_SRC_OUTPUT_CTF_TRACECOMPASSXMLWRITER_H
