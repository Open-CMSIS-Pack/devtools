/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_DECODE_OPENCSDITMDECODER_H
#define CTRACE_SRC_DECODE_OPENCSDITMDECODER_H

#include "OpenCsdTraceElement.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>

/** @brief Summarizes raw input consumed by an OpenCSD ITM decoder. */
struct OpenCsdItmDecodeResult {
  std::uint64_t bytesIn = 0;
};

class OpenCsdErrorController;
class OpenCsdItmSessionInterface;
class OpenCsdPacketCollector;

/** @brief Creates an OpenCSD session connected to the supplied callbacks. */
using OpenCsdItmSessionFactory =
    std::function<std::unique_ptr<OpenCsdItmSessionInterface>(OpenCsdPacketCollector&, OpenCsdErrorController&)>;

/** @brief Reports an unrecoverable OpenCSD failure and its processed byte count. */
class OpenCsdFatalError final : public std::runtime_error {
public:
  /**
   * @brief Creates a fatal decoder error.
   * @param message Human-readable failure description.
   * @param bytesProcessed Raw bytes consumed before the failure.
   */
  OpenCsdFatalError(const std::string& message, std::uint64_t bytesProcessed)
    : std::runtime_error(message),
      m_bytesProcessed(bytesProcessed)
  {
  }

  /**
   * @brief Returns the number of raw bytes processed before failure.
   * @return Raw bytes consumed before the decoder became unrecoverable.
   */
  std::uint64_t bytesProcessed() const noexcept
  {
    return m_bytesProcessed;
  }

private:
  std::uint64_t m_bytesProcessed = 0;
};

class OpenCsdItmDecoderImpl;

/** @brief Feeds raw ITM bytes to OpenCSD and recovers at hardware synchronization. */
class OpenCsdItmDecoder {
public:
  /**
   * @brief Creates a decoder using the production OpenCSD session.
   * @param elementSink Sink receiving decoded and recovery elements.
   */
  OpenCsdItmDecoder(OpenCsdTraceElementSink& elementSink);
  /**
   * @brief Creates a decoder with an injected OpenCSD session factory.
   * @param elementSink Sink receiving decoded and recovery elements.
   * @param sessionFactory Factory used to construct the external session.
   */
  OpenCsdItmDecoder(OpenCsdTraceElementSink& elementSink, const OpenCsdItmSessionFactory& sessionFactory);
  /** @brief Destroys the decoder implementation and external session. */
  ~OpenCsdItmDecoder();

  /** @brief Disables copying because a decoder owns one external session. */
  OpenCsdItmDecoder(const OpenCsdItmDecoder&) = delete;
  /** @brief Disables copy assignment because a decoder owns one external session. */
  OpenCsdItmDecoder& operator=(const OpenCsdItmDecoder&) = delete;

  /**
   * @brief Pushes the next raw byte chunk into the decoder.
   * @param data Beginning of a contiguous byte chunk; may be null when size is zero.
   * @param size Number of bytes available at data.
   * @throws OpenCsdFatalError If OpenCSD cannot make progress or recover.
   */
  void push(const std::uint8_t* data, std::uint32_t size);
  /**
   * @brief Completes decoding and returns the consumed byte count.
   * @return Aggregate raw-input counters for the completed stream.
   * @throws OpenCsdFatalError If OpenCSD cannot finalize the stream.
   */
  OpenCsdItmDecodeResult finish();

private:
  std::unique_ptr<OpenCsdItmDecoderImpl> m_impl;
};

#endif  // CTRACE_SRC_DECODE_OPENCSDITMDECODER_H
