/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "OpenCsdItmSession.hpp"

#include "OpenCsdErrorController.hpp"
#include "OpenCsdPacketCollector.hpp"
#include "common/ocsd_dcd_mngr_i.h"
#include "common/ocsd_lib_dcd_register.h"
#include "common/trc_component.h"
#include "interfaces/trc_data_raw_in_i.h"
#include "opencsd/itm/trc_pkt_types_itm.h"
#include "opencsd/ocsd_if_types.h"

#include <cstdint>

namespace {

constexpr std::uint32_t kItmTcrSwoEnable = 1U << 4U;

} // namespace

OpenCsdItmSession::OpenCsdItmSession(OpenCsdPacketCollector& collector, OpenCsdErrorController& errorController)
{
  ocsd_itm_cfg config{};
  // Keep OpenCSD timestamps in raw ITM ticks. They are scaled after decode,
  // where the originating CoreSight stream and processor are known.
  config.reg_tcr = kItmTcrSwoEnable;
  config_ = &config;

  try {
    createDecoder(collector, errorController);
  } catch (...) {
    destroyDecoder();
    throw;
  }
}

OpenCsdItmSession::~OpenCsdItmSession() noexcept
{
  destroyDecoder();
}

ocsd_datapath_resp_t OpenCsdItmSession::pushData(ocsd_trc_index_t index, std::uint32_t size, const std::uint8_t* data,
                                                 std::uint32_t& processed)
{
  return input_->TraceDataIn(OCSD_OP_DATA, index, size, data, &processed);
}

ocsd_datapath_resp_t OpenCsdItmSession::flush()
{
  return input_->TraceDataIn(OCSD_OP_FLUSH, 0, 0, nullptr, nullptr);
}

ocsd_datapath_resp_t OpenCsdItmSession::reset()
{
  return input_->TraceDataIn(OCSD_OP_RESET, 0, 0, nullptr, nullptr);
}

ocsd_datapath_resp_t OpenCsdItmSession::endOfTrace()
{
  return input_->TraceDataIn(OCSD_OP_EOT, 0, 0, nullptr, nullptr);
}

void OpenCsdItmSession::checkOcsd(ocsd_err_t error, const char* message)
{
  if (error != OCSD_OK) {
    throw OpenCsdItmSessionError(OpenCsdErrorController::describeApiError(error, message));
  }
}

void OpenCsdItmSession::createDecoder(OpenCsdPacketCollector& collector, OpenCsdErrorController& errorController)
{
  auto* registry = OcsdLibDcdRegister::getDecoderRegister();
  if (registry == nullptr) {
    throw OpenCsdItmSessionError("OpenCSD decoder registry is not initialized");
  }

  auto error = registry->getDecoderMngrByName(OCSD_BUILTIN_DCD_ITM, &manager_);
  checkOcsd(error, "failed to get OpenCSD ITM decoder manager");

  error = manager_->createDecoder(OCSD_CREATE_FLG_FULL_DECODER, 0, &config_, &component_);
  checkOcsd(error, "failed to create OpenCSD ITM decoder");

  error = manager_->attachErrorLogger(component_, &errorController);
  checkOcsd(error, "failed to attach OpenCSD packet-decoder error logger");
  if (component_->getAssocComponent() != nullptr) {
    error = manager_->attachErrorLogger(component_->getAssocComponent(), &errorController);
    checkOcsd(error, "failed to attach OpenCSD packet-processor error logger");
  }

  error = manager_->attachOutputSink(component_, &collector);
  checkOcsd(error, "failed to attach OpenCSD ITM output sink");

  error = manager_->getDataInputI(component_, &input_);
  checkOcsd(error, "failed to get OpenCSD ITM input interface");

  error = manager_->attachPktMonitor(component_, &collector);
  checkOcsd(error, "failed to attach OpenCSD ITM packet monitor");
}

void OpenCsdItmSession::destroyDecoder() noexcept
{
  input_ = nullptr;
  if (manager_ != nullptr && component_ != nullptr) {
    manager_->destroyDecoder(component_);
    component_ = nullptr;
  }
}
