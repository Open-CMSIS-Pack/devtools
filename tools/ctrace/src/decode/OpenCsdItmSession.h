/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_DECODE_OPENCSDITMSESSION_H
#define CTRACE_SRC_DECODE_OPENCSDITMSESSION_H

#include "opencsd/itm/trc_cmp_cfg_itm.h"
#include "opencsd/ocsd_if_types.h"

#include <cstdint>
#include <memory>
#include <stdexcept>

class IDecoderMngr;
class ITrcDataIn;
class OpenCsdErrorController;
class OpenCsdPacketCollector;
class TraceComponent;

/** @brief Abstracts one OpenCSD ITM session for production use and tests. */
class OpenCsdItmSessionInterface {
public:
  /** @brief Destroys an OpenCSD session through its interface. */
  virtual ~OpenCsdItmSessionInterface() = default;

  /** @brief Pushes raw bytes into OpenCSD and reports how many were consumed. */
  virtual ocsd_datapath_resp_t pushData(ocsd_trc_index_t index, std::uint32_t size, const std::uint8_t* data,
                                        std::uint32_t& processed) = 0;
  /** @brief Flushes pending OpenCSD decoder work. */
  virtual ocsd_datapath_resp_t flush() = 0;
  /** @brief Resets OpenCSD decoder state for stream recovery. */
  virtual ocsd_datapath_resp_t reset() = 0;
  /** @brief Signals the end of the current trace stream. */
  virtual ocsd_datapath_resp_t endOfTrace() = 0;
};

/** @brief Reports an OpenCSD session creation or API failure. */
class OpenCsdItmSessionError final : public std::runtime_error {
public:
  /** @brief Inherits standard runtime-error construction. */
  using std::runtime_error::runtime_error;
};

/** @brief Validates pointers and results returned by OpenCSD session setup APIs. */
class OpenCsdSessionValidation final {
public:
  /** @brief Rejects a null OpenCSD API object with a session error. */
  static void requireObject(const void* object, const char* message);
  /** @brief Rejects an unsuccessful OpenCSD API result with a session error. */
  static void requireSuccess(ocsd_err_t error, const char* message);

private:
  OpenCsdSessionValidation() = delete;
};

/**
 * @brief Owns one fully wired OpenCSD ITM callback decoder.
 *
 * Feed and recovery policy stays in OpenCsdItmDecoder; this class only manages
 * the external decoder session.
 */
class OpenCsdItmSession final : public OpenCsdItmSessionInterface {
public:
  /** @brief Creates and connects an OpenCSD ITM decoder session. */
  OpenCsdItmSession(OpenCsdPacketCollector& collector, OpenCsdErrorController& errorController);
  /** @brief Disconnects and destroys the OpenCSD session without throwing. */
  ~OpenCsdItmSession() noexcept;

  /** @brief Disables copying because a session owns external decoder state. */
  OpenCsdItmSession(const OpenCsdItmSession&) = delete;
  /** @brief Disables copy assignment because a session owns external decoder state. */
  OpenCsdItmSession& operator=(const OpenCsdItmSession&) = delete;

  /** @brief Pushes raw bytes into the external decoder. */
  ocsd_datapath_resp_t pushData(ocsd_trc_index_t index, std::uint32_t size, const std::uint8_t* data,
                                std::uint32_t& processed) override;
  /** @brief Flushes the external decoder. */
  ocsd_datapath_resp_t flush() override;
  /** @brief Resets the external decoder. */
  ocsd_datapath_resp_t reset() override;
  /** @brief Signals end of trace to the external decoder. */
  ocsd_datapath_resp_t endOfTrace() override;

private:
  /** @brief Destroys an OpenCSD decoder component through its owning manager. */
  struct DecoderDeleter {
    IDecoderMngr* manager = nullptr;
    void operator()(TraceComponent* component) const noexcept;
  };

  void createDecoder(OpenCsdPacketCollector& collector, OpenCsdErrorController& errorController);

  ITMConfig config_;
  IDecoderMngr* manager_ = nullptr;
  std::unique_ptr<TraceComponent, DecoderDeleter> component_{nullptr, DecoderDeleter{}};
  ITrcDataIn* input_ = nullptr;
};

#endif  // CTRACE_SRC_DECODE_OPENCSDITMSESSION_H
