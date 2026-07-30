/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CtfTestSupport.hpp"
#include "TestSupport.hpp"

#include <gtest/gtest.h>

#include "ctf/CtfEncoder.hpp"
#include "TestPath.hpp"
#include "TraceEvent.hpp"
#include "TraceSelection.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace {

using namespace CtfTestSupport;

std::string formatCtfUuid(const std::vector<unsigned char>& bytes, std::size_t offset)
{
  static constexpr char hexDigits[] = "0123456789abcdef";
  std::string result;
  result.reserve(36U);
  for (std::size_t index = 0; index < 16U; ++index) {
    if (index == 4U || index == 6U || index == 8U || index == 10U) {
      result.push_back('-');
    }
    const auto byte = bytes[offset + index];
    result.push_back(hexDigits[(byte >> 4U) & 0x0fU]);
    result.push_back(hexDigits[byte & 0x0fU]);
  }
  return result;
}

void requireSingleCtfItmEvent(const std::filesystem::path& streamPath, std::uint8_t expectedChannel,
                              const std::string& message)
{
  const auto bytes = readTestBinaryFile(streamPath);

  constexpr std::size_t itmPayloadSize = 8U;
  constexpr auto contentSize = kCtfEventOffset + kCtfEventHeaderSize + itmPayloadSize;
  require(bytes.size() >= contentSize, message);
  require(readLe32(bytes, kCtfPacketHeaderSize + 4U) == contentSize * 8U, message);
  require(readLe32(bytes, kCtfEventOffset) == 0U, message);
  require(bytes[kCtfEventOffset + kCtfEventHeaderSize] == expectedChannel, message);
}

} // namespace

TEST(CtraceUnitTests, testCtfEncoderWritesOnlyIntoProvidedDirectory)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-encoder-boundary-test");
  const auto& root = temporaryPath.path();
  const auto missingDirectory = root / "missing";
  const auto outputDirectory = root / "provided";

  CtfEncoder encoder(CtfEncoderConfig{
      1000000U,
      TraceSelection{{"itm"}, {}},
      {},
  });
  bool rejectedMissingDirectory = false;
  try {
    encoder.start(missingDirectory);
  } catch (const std::runtime_error&) {
    rejectedMissingDirectory = true;
  }
  require(rejectedMissingDirectory && !std::filesystem::exists(missingDirectory),
          "CtfEncoder must not create or own its output directory");

  std::filesystem::create_directories(outputDirectory);
  encoder.start(outputDirectory);
  TraceEvent software = softwarePacket(1U, 1U, 'A');
  software.tcyc = 10U;
  encoder.writeEvent(software);
  encoder.stop();
  require(std::filesystem::is_regular_file(outputDirectory / "metadata") &&
              std::filesystem::is_regular_file(outputDirectory / "stream_0"),
          "CtfEncoder must encode metadata and stream data into the provided directory");
  requireSingleCtfItmEvent(outputDirectory / "stream_0", 1U, "CtfEncoder encoded an unexpected ITM event");

  encoder.abort();
  require(std::filesystem::is_regular_file(outputDirectory / "metadata"),
          "CtfEncoder abort must not delete a directory owned by its caller");
}

TEST(CtraceUnitTests, testCtfEncoderPacketBoundaryAndUuid)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-encoder-packet-boundary-test");
  const auto& outputDirectory = temporaryPath.path();
  std::filesystem::create_directories(outputDirectory);

  CtfEncoder encoder(CtfEncoderConfig{
      1000000U,
      TraceSelection{{"itm"}, {}},
      {},
  });
  encoder.start(outputDirectory);

  // A one-byte ITM event occupies 21 bytes. Exactly 3118 events fit after
  // the 56-byte header/context; event 3119 starts packet 2.
  constexpr std::size_t eventsInFirstPacket = 3118U;
  for (std::size_t index = 0; index <= eventsInFirstPacket; ++index) {
    TraceEvent software = softwarePacket(1U, 1U, static_cast<std::uint32_t>(index));
    software.tcyc = index + 1U;
    encoder.writeEvent(software);
  }
  encoder.stop();

  const auto stream = readTestBinaryFile(outputDirectory / "stream_0");
  const auto metadata = readTestTextFile(outputDirectory / "metadata");

  constexpr std::size_t eventSize = 21U;
  constexpr auto firstContentSize = kCtfPacketHeaderSize + kCtfPacketContextSize + eventsInFirstPacket * eventSize;
  constexpr std::uint32_t ctfMagic = 0xc1fc1fc1U;
  require(stream.size() == kCtfPacketSize * 2U, "CTF packet rollover must emit two complete 64-KiB packets");
  require(readLe32(stream, 0U) == ctfMagic && readLe32(stream, kCtfPacketSize) == ctfMagic,
          "CTF packet rollover emitted an invalid packet header");
  require(readLe32(stream, kCtfPacketHeaderSize) == kCtfPacketSize * 8U &&
              readLe32(stream, kCtfPacketHeaderSize + 4U) == firstContentSize * 8U,
          "first CTF packet content size mismatch at rollover");
  require(readLe32(stream, kCtfPacketSize + kCtfPacketHeaderSize + 4U) == (kCtfEventOffset + eventSize) * 8U,
          "second CTF packet content size mismatch");
  require(readLe32(stream, kCtfPacketHeaderSize + 28U) == 0U &&
              readLe32(stream, kCtfPacketSize + kCtfPacketHeaderSize + 28U) == 1U,
          "CTF packet sequence must advance across a 64-KiB boundary");
  require(std::equal(stream.begin() + 4U, stream.begin() + 20U, stream.begin() + kCtfPacketSize + 4U),
          "CTF packet UUID must remain stable across packet rollover");

  const auto uuid = formatCtfUuid(stream, 4U);
  require(metadata.find("uuid = \"" + uuid + "\";") != std::string::npos,
          "CTF metadata UUID must match the binary packet UUID");
  require((stream[4U + 6U] & 0xf0U) == 0x40U && (stream[4U + 8U] & 0xc0U) == 0x80U,
          "CTF UUID must use RFC 4122 version-4 and variant bits");

  encoder.abort();
}

TEST(CtraceUnitTests, testCtfEncoderDwtAddressEncoding)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-encoder-dwt-address-test");
  const auto& outputDirectory = temporaryPath.path();
  std::filesystem::create_directories(outputDirectory);

  CtfEncoder encoder(CtfEncoderConfig{
      1000000U,
      TraceSelection{{"dwt"}, {}},
      {},
  });
  encoder.start(outputDirectory);
  TraceEvent address{DwtAddressTraceEvent{
      3U,
      DwtPcAndOffsetTraceLocation{0x12345678U, 0x0000abcdU},
  }};
  address.tcyc = 99U;
  encoder.writeEvent(address);
  encoder.stop();

  const auto stream = readTestBinaryFile(outputDirectory / "stream_0");

  constexpr std::size_t payloadSize = 14U;
  constexpr std::size_t payloadOffset = kCtfEventOffset + kCtfEventHeaderSize;
  require(readLe32(stream, kCtfPacketHeaderSize + 4U) == (kCtfEventOffset + kCtfEventHeaderSize + payloadSize) * 8U,
          "CTF DWT address event content size mismatch");
  require(readLe32(stream, kCtfEventOffset) == 6U, "CTF DWT address event ID mismatch");
  require(readLe64(stream, kCtfEventOffset + 4U) == 99U, "CTF DWT address timestamp mismatch");
  require(stream[payloadOffset] == 3U && stream[payloadOffset + 1U] == 1U && stream[payloadOffset + 2U] == 1U,
          "CTF DWT address comparator or presence flags mismatch");
  require(readLe32(stream, payloadOffset + 3U) == 0x12345678U && readLe16(stream, payloadOffset + 7U) == 0xabcdU,
          "CTF DWT PC/address payload mismatch");

  encoder.abort();
}
