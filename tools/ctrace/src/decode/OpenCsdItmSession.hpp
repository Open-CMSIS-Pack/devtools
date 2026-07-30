/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#pragma once

#include "opencsd/itm/trc_cmp_cfg_itm.h"
#include "opencsd/ocsd_if_types.h"

#include <cstdint>
#include <stdexcept>

class IDecoderMngr;
class ITrcDataIn;
class OpenCsdErrorController;
class OpenCsdPacketCollector;
class TraceComponent;

class OpenCsdItmSessionError final : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

// Owns one fully wired OpenCSD ITM callback decoder. Feed/recovery policy stays
// in OpenCsdItmDecoder; this class only manages the external decoder session.
class OpenCsdItmSession final {
public:
  OpenCsdItmSession(OpenCsdPacketCollector& collector, OpenCsdErrorController& errorController);
  ~OpenCsdItmSession() noexcept;

  OpenCsdItmSession(const OpenCsdItmSession&) = delete;
  OpenCsdItmSession& operator=(const OpenCsdItmSession&) = delete;

  ocsd_datapath_resp_t pushData(ocsd_trc_index_t index, std::uint32_t size, const std::uint8_t* data,
                                std::uint32_t& processed);
  ocsd_datapath_resp_t flush();
  ocsd_datapath_resp_t reset();
  ocsd_datapath_resp_t endOfTrace();

private:
  static void checkOcsd(ocsd_err_t error, const char* message);

  void createDecoder(OpenCsdPacketCollector& collector, OpenCsdErrorController& errorController);
  void destroyDecoder() noexcept;

  ITMConfig config_;
  IDecoderMngr* manager_ = nullptr;
  TraceComponent* component_ = nullptr;
  ITrcDataIn* input_ = nullptr;
};
