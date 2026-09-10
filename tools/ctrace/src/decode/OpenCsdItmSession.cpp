/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "OpenCsdItmSession.h"

#include "OpenCsdErrorController.h"
#include "OpenCsdPacketCollector.h"
#include "OpenCsdTreeSession.h"
#include "opencsd/itm/trc_pkt_types_itm.h"
#include "opencsd/ocsd_if_types.h"

#include <cstdint>

constexpr std::uint32_t kItmTcrSwoEnable = 1U << 4U;
constexpr ocsd_itm_cfg kItmConfig{kItmTcrSwoEnable};

OpenCsdItmSession::OpenCsdItmSession(OpenCsdPacketCollector& collector, OpenCsdErrorController& errorController)
  : m_config(&kItmConfig),
    m_treeSession(OCSD_TRC_SRC_SINGLE, 0U, errorController, collector)
{
  // Keep OpenCSD timestamps in raw ITM ticks. They are scaled after decode,
  // where the originating CoreSight stream and processor are known.
  // Channel 0 is OpenCSD's internal SINGLE transport channel. It is not an
  // architectural Trace Bus ID and the bound collector retains the synthetic route.
  m_config.setTraceID(0U);
  m_treeSession.createDecoder(OCSD_BUILTIN_DCD_ITM, OCSD_CREATE_FLG_FULL_DECODER, m_config);
  m_treeSession.attachDecoderCallbacks(0U, collector);
}

OpenCsdItmSession::~OpenCsdItmSession() noexcept = default;

ocsd_datapath_resp_t OpenCsdItmSession::pushData(ocsd_trc_index_t index, std::uint32_t size, const std::uint8_t* data,
                                                 std::uint32_t& processed)
{
  return m_treeSession.traceDataIn(OCSD_OP_DATA, index, size, data, &processed);
}

ocsd_datapath_resp_t OpenCsdItmSession::flush()
{
  return m_treeSession.traceDataIn(OCSD_OP_FLUSH, 0, 0, nullptr, nullptr);
}

ocsd_datapath_resp_t OpenCsdItmSession::reset()
{
  return m_treeSession.traceDataIn(OCSD_OP_RESET, 0, 0, nullptr, nullptr);
}

ocsd_datapath_resp_t OpenCsdItmSession::resetRoute(std::uint8_t channel, ocsd_trc_index_t index)
{
  static_cast<void>(channel);
  static_cast<void>(index);
  return reset();
}

ocsd_datapath_resp_t OpenCsdItmSession::endOfTrace()
{
  return m_treeSession.traceDataIn(OCSD_OP_EOT, 0, 0, nullptr, nullptr);
}
