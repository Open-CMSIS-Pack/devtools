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
class OcsdLibDcdRegister;
class TraceComponent;

/** @brief Abstracts one OpenCSD ITM session for production use and tests. */
class OpenCsdItmSessionInterface {
public:
  /** @brief Destroys an OpenCSD session through its interface. */
  virtual ~OpenCsdItmSessionInterface() = default;

  /**
   * @brief Pushes raw bytes into OpenCSD and reports how many were consumed.
   * @param index Absolute raw-stream offset of the first byte.
   * @param size Number of bytes available at data.
   * @param data Contiguous raw trace bytes.
   * @param processed Receives the number of bytes consumed by OpenCSD.
   * @return OpenCSD data-path response controlling further input.
   */
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
  /**
   * @brief Rejects a null OpenCSD API object with a session error.
   * @param object Required external API object.
   * @param message Failure text used when object is null.
   * @throws OpenCsdItmSessionError If object is null.
   */
  static void requireObject(const void* object, const char* message);
  /**
   * @brief Rejects an unsuccessful OpenCSD API result with a session error.
   * @param error OpenCSD result to validate.
   * @param message Failure text used for an error result.
   * @throws OpenCsdItmSessionError If error does not report success.
   */
  static void requireSuccess(ocsd_err_t error, const char* message);

private:
  /** @brief Prevents construction of this stateless validation utility. */
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
  /** @brief Supplies the OpenCSD decoder registry to a session. */
  using DecoderRegistryProvider = OcsdLibDcdRegister* (*)();

  /**
   * @brief Creates and connects an OpenCSD ITM decoder session.
   * @param collector Callback target for decoded packets and elements.
   * @param errorController Callback target for OpenCSD errors.
   * @throws OpenCsdItmSessionError If external session setup fails.
   */
  OpenCsdItmSession(OpenCsdPacketCollector& collector, OpenCsdErrorController& errorController);
  /**
   * @brief Creates a session with an injectable decoder-registry provider.
   * @param collector Callback target for decoded packets and elements.
   * @param errorController Callback target for OpenCSD errors.
   * @param registryProvider Provider used to retrieve the decoder registry.
   * @throws OpenCsdItmSessionError If external session setup fails.
   */
  OpenCsdItmSession(OpenCsdPacketCollector& collector, OpenCsdErrorController& errorController,
                    DecoderRegistryProvider registryProvider);
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
    /** @brief Destroys a component through the manager that created it. */
    void operator()(TraceComponent* component) const noexcept;
  };

  /** @brief Creates the ITM decoder and attaches callbacks and input interfaces. */
  void createDecoder(OpenCsdPacketCollector& collector, OpenCsdErrorController& errorController,
                     DecoderRegistryProvider registryProvider);

  ITMConfig m_config;
  IDecoderMngr* m_manager = nullptr;
  std::unique_ptr<TraceComponent, DecoderDeleter> m_component{nullptr, DecoderDeleter{}};
  ITrcDataIn* m_input = nullptr;
};

#endif  // CTRACE_SRC_DECODE_OPENCSDITMSESSION_H
