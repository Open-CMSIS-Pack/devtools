/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#pragma once

#include "OpenCsdTraceElement.hpp"

#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace OpenCsdTestSupport {

class CollectingOpenCsdElementSink : public OpenCsdTraceElementSink {
public:
  void append(OpenCsdTraceElement element) override
  {
    elements.push_back(std::move(element));
  }

  bool hasIssue(std::string_view code) const
  {
    for (const auto& element : elements) {
      if (element.issueCode == code) {
        return true;
      }
    }
    return false;
  }

  std::vector<OpenCsdTraceElement> elements;
};

inline OpenCsdTraceElement openCsdElement(OpenCsdTraceElement::Kind kind, std::uint64_t index = 0U,
                                          std::uint8_t stream = 0U)
{
  OpenCsdTraceElement element;
  element.kind = kind;
  element.sourceIndex = index;
  element.traceBusId = stream;
  return element;
}

inline OpenCsdTraceElement openCsdSoftwareElement(std::uint32_t channel, std::uint32_t value = 0U,
                                                  std::uint64_t index = 0U, std::uint8_t stream = 0U,
                                                  std::uint8_t size = 1U)
{
  auto element = openCsdElement(OpenCsdTraceElement::Kind::Software, index, stream);
  element.channel = channel;
  element.size = size;
  element.value = value;
  return element;
}

inline OpenCsdTraceElement
openCsdTimestampElement(std::uint64_t tcyc, std::uint64_t index = 0U, std::uint8_t stream = 0U,
                        LocalTimestampRelation relation = LocalTimestampRelation::Synchronous)
{
  auto element = openCsdElement(OpenCsdTraceElement::Kind::LocalTimestamp, index, stream);
  element.timestampRelation = relation;
  element.tcyc = tcyc;
  return element;
}

} // namespace OpenCsdTestSupport
