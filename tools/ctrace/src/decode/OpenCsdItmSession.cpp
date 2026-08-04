/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "OpenCsdItmSession.h"

#include "OpenCsdErrorController.h"
#include "OpenCsdPacketCollector.h"
#include "common/ocsd_dcd_mngr_i.h"
#include "common/ocsd_lib_dcd_register.h"
#include "common/trc_component.h"
#include "interfaces/trc_data_raw_in_i.h"
#include "opencsd/itm/trc_pkt_types_itm.h"
#include "opencsd/ocsd_if_types.h"

#include <cstdint>

constexpr std::uint32_t kItmTcrSwoEnable = 1U << 4U;
constexpr ocsd_itm_cfg kItmConfig{kItmTcrSwoEnable};

OpenCsdItmSession::OpenCsdItmSession(OpenCsdPacketCollector& collector, OpenCsdErrorController& errorController)
  : OpenCsdItmSession(collector, errorController, &OcsdLibDcdRegister::getDecoderRegister)
{
}

OpenCsdItmSession::OpenCsdItmSession(OpenCsdPacketCollector& collector, OpenCsdErrorController& errorController,
                                     DecoderRegistryProvider registryProvider)
  : m_config(&kItmConfig)
{
  // Keep OpenCSD timestamps in raw ITM ticks. They are scaled after decode,
  // where the originating CoreSight stream and processor are known.
  createDecoder(collector, errorController, registryProvider);
}

OpenCsdItmSession::~OpenCsdItmSession() noexcept = default;

void OpenCsdSessionValidation::requireObject(const void* object, const char* message)
{
  if (object == nullptr) {
    throw OpenCsdItmSessionError(message);
  }
}

void OpenCsdSessionValidation::requireSuccess(ocsd_err_t error, const char* message)
{
  if (error != OCSD_OK) {
    throw OpenCsdItmSessionError(OpenCsdErrorController::describeApiError(error, message));
  }
}

void OpenCsdItmSession::DecoderDeleter::operator()(TraceComponent* component) const noexcept
{
  if (manager != nullptr) {
    manager->destroyDecoder(component);
  }
}

ocsd_datapath_resp_t OpenCsdItmSession::pushData(ocsd_trc_index_t index, std::uint32_t size, const std::uint8_t* data,
                                                 std::uint32_t& processed)
{
  return m_input->TraceDataIn(OCSD_OP_DATA, index, size, data, &processed);
}

ocsd_datapath_resp_t OpenCsdItmSession::flush()
{
  return m_input->TraceDataIn(OCSD_OP_FLUSH, 0, 0, nullptr, nullptr);
}

ocsd_datapath_resp_t OpenCsdItmSession::reset()
{
  return m_input->TraceDataIn(OCSD_OP_RESET, 0, 0, nullptr, nullptr);
}

ocsd_datapath_resp_t OpenCsdItmSession::endOfTrace()
{
  return m_input->TraceDataIn(OCSD_OP_EOT, 0, 0, nullptr, nullptr);
}

void OpenCsdItmSession::createDecoder(OpenCsdPacketCollector& collector, OpenCsdErrorController& errorController,
                                      DecoderRegistryProvider registryProvider)
{
  if (registryProvider == nullptr) {
    throw OpenCsdItmSessionError("OpenCSD decoder registry provider is not configured");
  }
  auto* registry = registryProvider();
  if (registry == nullptr) {
    throw OpenCsdItmSessionError("OpenCSD decoder registry is not initialized");
  }

  auto error = registry->getDecoderMngrByName(OCSD_BUILTIN_DCD_ITM, &m_manager);
  OpenCsdSessionValidation::requireSuccess(error, "failed to get OpenCSD ITM decoder manager");
  OpenCsdSessionValidation::requireObject(m_manager, "OpenCSD ITM decoder manager is not initialized");

  TraceComponent* component = nullptr;
  error = m_manager->createDecoder(OCSD_CREATE_FLG_FULL_DECODER, 0, &m_config, &component);
  m_component.get_deleter().manager = m_manager;
  m_component.reset(component);
  OpenCsdSessionValidation::requireSuccess(error, "failed to create OpenCSD ITM decoder");
  OpenCsdSessionValidation::requireObject(m_component.get(), "OpenCSD ITM decoder component is not initialized");

  error = m_manager->attachErrorLogger(m_component.get(), &errorController);
  OpenCsdSessionValidation::requireSuccess(error, "failed to attach OpenCSD packet-decoder error logger");
  if (m_component->getAssocComponent() != nullptr) {
    error = m_manager->attachErrorLogger(m_component->getAssocComponent(), &errorController);
    OpenCsdSessionValidation::requireSuccess(error, "failed to attach OpenCSD packet-processor error logger");
  }

  error = m_manager->attachOutputSink(m_component.get(), &collector);
  OpenCsdSessionValidation::requireSuccess(error, "failed to attach OpenCSD ITM output sink");

  error = m_manager->getDataInputI(m_component.get(), &m_input);
  OpenCsdSessionValidation::requireSuccess(error, "failed to get OpenCSD ITM input interface");
  OpenCsdSessionValidation::requireObject(m_input, "OpenCSD ITM input interface is not initialized");

  error = m_manager->attachPktMonitor(m_component.get(), &collector);
  OpenCsdSessionValidation::requireSuccess(error, "failed to attach OpenCSD ITM packet monitor");
}
