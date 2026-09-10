/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_DECODE_OPENCSDITMSESSION_H
#define CTRACE_SRC_DECODE_OPENCSDITMSESSION_H

#include "OpenCsdTreeSession.h"
#include "opencsd/itm/trc_cmp_cfg_itm.h"
#include "opencsd/ocsd_if_types.h"

#include <cstdint>

class OpenCsdErrorController;
class OpenCsdPacketCollector;

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
  /**
   * @brief Resets one transport route while preserving the formatted root frontend.
   *
   * Every session must state this behavior explicitly so a formatted implementation
   * cannot silently fall back to resetting the complete tree.
   */
  virtual ocsd_datapath_resp_t resetRoute(std::uint8_t channel, ocsd_trc_index_t index) = 0;
  /** @brief Signals the end of the current trace stream. */
  virtual ocsd_datapath_resp_t endOfTrace() = 0;
};

/** @brief Backward-compatible name for an OpenCSD tree-session setup failure. */
using OpenCsdItmSessionError = OpenCsdTreeSessionError;

/**
 * @brief Owns one fully wired OpenCSD ITM callback decoder.
 *
 * Feed and recovery policy stays in OpenCsdItmDecoder; this class only manages
 * the external decoder session.
 */
class OpenCsdItmSession final : public OpenCsdItmSessionInterface {
public:
  /**
   * @brief Creates and connects an OpenCSD SINGLE-tree ITM decoder session.
   * @param collector Callback target for decoded packets and elements.
   * @param errorController Callback target for OpenCSD errors.
   * @throws OpenCsdItmSessionError If external session setup fails.
   */
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
  /** @brief Resets the SINGLE decoder on channel 0 without resetting the tree root. */
  ocsd_datapath_resp_t reset() override;
  /** @brief Resets the synthetic SINGLE route without resetting the tree root. */
  ocsd_datapath_resp_t resetRoute(std::uint8_t channel, ocsd_trc_index_t index) override;
  /** @brief Signals end of trace to the external decoder. */
  ocsd_datapath_resp_t endOfTrace() override;

private:
  ITMConfig m_config;
  OpenCsdTreeSession m_treeSession;
};

#endif // CTRACE_SRC_DECODE_OPENCSDITMSESSION_H
