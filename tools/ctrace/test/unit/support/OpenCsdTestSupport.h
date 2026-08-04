/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_TEST_UNIT_SUPPORT_OPENCSDTESTSUPPORT_H
#define CTRACE_TEST_UNIT_SUPPORT_OPENCSDTESTSUPPORT_H

#include "OpenCsdTraceElement.h"

#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace OpenCsdTestSupport {

/** @brief Collects OpenCSD elements emitted during a unit test. */
class CollectingOpenCsdElementSink : public OpenCsdTraceElementSink {
public:
  /** @brief Stores one emitted OpenCSD element. */
  void append(OpenCsdTraceElement element) override
  {
    m_elements.push_back(std::move(element));
  }

  /** @brief Tests whether the collected elements include an issue code. */
  bool hasIssue(std::string_view code) const
  {
    for (const auto& element : m_elements) {
      if (element.issueCode == code) {
        return true;
      }
    }
    return false;
  }

  /** @brief Returns all collected OpenCSD elements. */
  const std::vector<OpenCsdTraceElement>& elements() const
  {
    return m_elements;
  }

private:
  std::vector<OpenCsdTraceElement> m_elements;
};

/** @brief Creates a basic OpenCSD element for a test. */
inline OpenCsdTraceElement openCsdElement(OpenCsdTraceElement::Kind kind, std::uint64_t index = 0U,
                                          std::uint8_t stream = 0U)
{
  OpenCsdTraceElement element;
  element.kind = kind;
  element.sourceIndex = index;
  element.traceBusId = stream;
  return element;
}

/** @brief Creates an OpenCSD software element for a test. */
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

/** @brief Creates an OpenCSD local timestamp element for a test. */
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

#endif  // CTRACE_TEST_UNIT_SUPPORT_OPENCSDTESTSUPPORT_H
