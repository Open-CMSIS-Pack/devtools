/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#pragma once

#include "OpenCsdTraceElement.hpp"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

struct OpenCsdItmDecodeResult {
  std::uint64_t bytesIn = 0;
};

class OpenCsdFatalError final : public std::runtime_error {
public:
  OpenCsdFatalError(const std::string& message, std::uint64_t bytesProcessed)
    : std::runtime_error(message), bytesProcessed_(bytesProcessed)
  {
  }

  std::uint64_t bytesProcessed() const noexcept
  {
    return bytesProcessed_;
  }

private:
  std::uint64_t bytesProcessed_ = 0;
};

class OpenCsdItmDecoderImpl;

class OpenCsdItmDecoder {
public:
  OpenCsdItmDecoder(OpenCsdTraceElementSink& elementSink);
  ~OpenCsdItmDecoder();

  OpenCsdItmDecoder(const OpenCsdItmDecoder&) = delete;
  OpenCsdItmDecoder& operator=(const OpenCsdItmDecoder&) = delete;

  void push(const std::uint8_t* data, std::uint32_t size);
  OpenCsdItmDecodeResult finish();

private:
  std::unique_ptr<OpenCsdItmDecoderImpl> impl_;
};
