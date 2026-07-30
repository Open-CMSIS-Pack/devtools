/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#pragma once

#include "DiagnosticSink.hpp"
#include "TraceEvent.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

inline void require(bool condition, const std::string& message)
{
  if (!condition) {
    throw std::runtime_error(message);
  }
}

class CollectingDiagnosticSink final : public DiagnosticSink {
public:
  const std::vector<Event>& events() const
  {
    return events_;
  }

  void clear()
  {
    events_.clear();
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

inline std::vector<std::string> splitCsvTestLine(const std::string& line)
{
  std::vector<std::string> fields;
  std::istringstream in(line);
  std::string field;
  while (std::getline(in, field, ',')) {
    fields.push_back(field);
  }
  if (!line.empty() && line.back() == ',') {
    fields.emplace_back();
  }
  return fields;
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

inline TraceEvent issuePacket(std::string code, std::string message = {},
                              TraceIssueSeverity severity = TraceIssueSeverity::Error)
{
  return TraceEvent{TraceIssueEvent{
      std::move(code),
      severity,
      std::move(message),
  }};
}
