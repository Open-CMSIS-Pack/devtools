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

inline void require(bool condition, const std::string& message)
{
  if (!condition) {
    throw std::runtime_error(message);
  }
}

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

template <typename Exception = std::runtime_error, typename Function> inline bool throwsException(Function&& action)
{
  return captureExceptionMessage<Exception>(std::forward<Function>(action)).has_value();
}

template <typename Exception = std::runtime_error, typename Function>
inline bool throwsWithMessage(Function&& action, std::string_view expected)
{
  const auto message = captureExceptionMessage<Exception>(std::forward<Function>(action));
  return message.has_value() && message->find(expected) != std::string::npos;
}

class CollectingDiagnosticSink final : public DiagnosticSink {
public:
  const std::vector<Event>& events() const
  {
    return events_;
  }

  bool contains(std::string_view code) const
  {
    for (const auto& event : events_) {
      if (event.code == code) {
        return true;
      }
    }
    return false;
  }

  const Event& singleEvent(std::string_view code) const
  {
    require(events_.size() == 1U, "expected exactly one diagnostic event");
    require(events_.front().code == code, "unexpected diagnostic code: " + events_.front().code);
    return events_.front();
  }

  bool containsContext(std::string_view code, std::string_view key, std::string_view value) const
  {
    for (const auto& event : events_) {
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
  void write(const Event& event) override
  {
    events_.push_back(event);
  }

private:
  std::vector<Event> events_;
};

class CollectingEventSink final : public TraceEventSink {
public:
  void append(const TraceEvent& event) override
  {
    events.push_back(event);
  }

  std::vector<TraceEvent> events;
};

inline void writeTestFile(const std::filesystem::path& path, const std::string& contents = {})
{
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    throw std::runtime_error("failed to create test file: " + path.string());
  }
  out << contents;
}

inline std::string readTestTextFile(const std::filesystem::path& path)
{
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    throw std::runtime_error("failed to read test file: " + path.string());
  }
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

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

inline std::vector<unsigned char> readTestBinaryFile(const std::filesystem::path& path)
{
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    throw std::runtime_error("failed to read binary test file: " + path.string());
  }
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

inline TraceEvent exceptionPacket(std::uint32_t number, ExceptionAction action, std::uint64_t tcyc = 0)
{
  TraceEvent packet{ExceptionTraceEvent{number, action}};
  packet.tcyc = tcyc;
  return packet;
}

inline TraceEvent overflowPacket(std::uint64_t tcyc)
{
  TraceEvent packet{OverflowTraceEvent{}};
  packet.tcyc = tcyc;
  packet.quality = TraceQuality{true, false, 1U};
  return packet;
}

inline TraceEvent softwarePacket(std::uint32_t channel, std::uint8_t size = 1, std::uint32_t value = 0)
{
  return TraceEvent{SoftwareTraceEvent{channel, size, value}};
}

inline TraceEvent atCycle(TraceEvent event, std::uint64_t tcyc)
{
  event.tcyc = tcyc;
  return event;
}

inline TraceEvent onStream(TraceEvent event, std::uint8_t traceBusId)
{
  event.traceBusId = traceBusId;
  return event;
}

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
