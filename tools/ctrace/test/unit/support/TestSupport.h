/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_TEST_UNIT_SUPPORT_TESTSUPPORT_H
#define CTRACE_TEST_UNIT_SUPPORT_TESTSUPPORT_H

#include "DiagnosticSink.h"
#include "TraceEvent.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/** @brief Throws a test failure message when a required condition is false. */
inline void require(bool condition, const std::string& message)
{
  if (!condition) {
    throw std::runtime_error(message);
  }
}

/** @brief Executes an action and captures a matching exception message. */
template <typename Exception = std::runtime_error, typename Function>
inline std::optional<std::string> captureExceptionMessage(Function&& action)
{
  try {
    std::forward<Function>(action)();
  } catch (const Exception& error) {
    return error.what();
  }
  return std::nullopt;
}

/** @brief Tests whether an action throws the requested exception type. */
template <typename Exception = std::runtime_error, typename Function> inline bool throwsException(Function&& action)
{
  return captureExceptionMessage<Exception>(std::forward<Function>(action)).has_value();
}

/** @brief Tests whether an exception message contains expected text. */
template <typename Exception = std::runtime_error, typename Function>
inline bool throwsWithMessage(Function&& action, std::string_view expected)
{
  const auto message = captureExceptionMessage<Exception>(std::forward<Function>(action));
  return message.has_value() && message->find(expected) != std::string::npos;
}

/** @brief Collects structured diagnostics emitted during a unit test. */
class CollectingDiagnosticSink final : public DiagnosticSink {
public:
  /** @brief Returns all collected diagnostic events. */
  const std::vector<Event>& events() const
  {
    return m_events;
  }

  /** @brief Tests whether a diagnostic code was collected. */
  bool contains(std::string_view code) const
  {
    for (const auto& event : m_events) {
      if (event.code == code) {
        return true;
      }
    }
    return false;
  }

  /** @brief Returns the only collected event after checking its code. */
  const Event& singleEvent(std::string_view code) const
  {
    require(m_events.size() == 1U, "expected exactly one diagnostic event");
    require(m_events.front().code == code, "unexpected diagnostic code: " + m_events.front().code);
    return m_events.front();
  }

  /** @brief Tests whether a diagnostic contains one context entry. */
  bool containsContext(std::string_view code, std::string_view key, std::string_view value) const
  {
    for (const auto& event : m_events) {
      if (event.code != code) {
        continue;
      }
      for (const auto& [contextKey, contextValue] : event.context) {
        if (contextKey == key && contextValue == value) {
          return true;
        }
      }
    }
    return false;
  }

protected:
  /** @brief Stores one reported diagnostic event. */
  void write(const Event& event) override
  {
    m_events.push_back(event);
  }

private:
  std::vector<Event> m_events;
};

/** @brief Collects semantic trace events emitted during a unit test. */
class CollectingEventSink final : public TraceEventSink {
public:
  /** @brief Stores one emitted semantic event. */
  void append(const TraceEvent& event) override
  {
    m_events.push_back(event);
  }

  /** @brief Returns all collected semantic events. */
  const std::vector<TraceEvent>& events() const
  {
    return m_events;
  }

  /** @brief Returns mutable collected events for ownership transfer in tests. */
  std::vector<TraceEvent>& events()
  {
    return m_events;
  }

private:
  std::vector<TraceEvent> m_events;
};

/** @brief Creates a binary test file with the supplied contents. */
inline void writeTestFile(const std::filesystem::path& path, const std::string& contents = {})
{
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    throw std::runtime_error("failed to create test file: " + path.string());
  }
  out << contents;
}

/** @brief Reads a complete test file as text. */
inline std::string readTestTextFile(const std::filesystem::path& path)
{
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    throw std::runtime_error("failed to read test file: " + path.string());
  }
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

/** @brief Reads all lines from a test text file. */
inline std::vector<std::string> readTestLines(const std::filesystem::path& path)
{
  std::ifstream in(path);
  if (!in) {
    throw std::runtime_error("failed to read test file: " + path.string());
  }
  std::vector<std::string> lines;
  for (std::string line; std::getline(in, line);) {
    lines.push_back(std::move(line));
  }
  return lines;
}

/** @brief Reads a complete test file as bytes. */
inline std::vector<unsigned char> readTestBinaryFile(const std::filesystem::path& path)
{
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    throw std::runtime_error("failed to read binary test file: " + path.string());
  }
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

/** @brief Creates a timestamped exception event for a test. */
inline TraceEvent exceptionPacket(std::uint32_t number, ExceptionAction action, std::uint64_t tcyc = 0)
{
  TraceEvent packet{ExceptionTraceEvent{number, action}};
  packet.tcyc = tcyc;
  return packet;
}

/** @brief Creates a timestamped overflow event for a test. */
inline TraceEvent overflowPacket(std::uint64_t tcyc)
{
  TraceEvent packet{OverflowTraceEvent{}};
  packet.tcyc = tcyc;
  packet.quality = TraceQuality{true, false, 1U};
  return packet;
}

/** @brief Creates an ITM software event for a test. */
inline TraceEvent softwarePacket(std::uint32_t channel, std::uint8_t size = 1, std::uint32_t value = 0)
{
  return TraceEvent{SoftwareTraceEvent{channel, size, value}};
}

/** @brief Assigns a cycle timestamp to a copied event. */
inline TraceEvent atCycle(TraceEvent event, std::uint64_t tcyc)
{
  event.tcyc = tcyc;
  return event;
}

/** @brief Assigns a Trace Bus ID to a copied event. */
inline TraceEvent onStream(TraceEvent event, std::uint8_t traceBusId)
{
  event.traceBusId = traceBusId;
  return event;
}

/** @brief Creates a decoder issue event for a test. */
inline TraceEvent issuePacket(std::string code, std::string message = {},
                              TraceIssueSeverity severity = TraceIssueSeverity::Error)
{
  return TraceEvent{TraceIssueEvent{
      std::move(code),
      severity,
      std::move(message),
  }};
}

#endif  // CTRACE_TEST_UNIT_SUPPORT_TESTSUPPORT_H
