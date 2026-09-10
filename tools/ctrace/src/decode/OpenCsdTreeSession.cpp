/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "OpenCsdTreeSession.h"

#include "OpenCsdErrorController.h"
#include "common/ocsd_dcd_mngr_i.h"
#include "common/ocsd_dcd_tree.h"
#include "common/ocsd_dcd_tree_elem.h"
#include "common/trc_cs_config.h"
#include "common/trc_component.h"
#include "common/trc_frame_deformatter.h"
#include "interfaces/trc_abs_typed_base_i.h"
#include "interfaces/trc_data_rawframe_in_i.h"
#include "interfaces/trc_error_log_i.h"
#include "interfaces/trc_gen_elem_in_i.h"

#include <atomic>
#include <cstdint>

namespace {

// DecodeTree keeps both the alternate logger and its live-tree registry in
// process-global state. This lease serializes every tree owned by ctrace.
std::atomic_bool ctraceOwnsProcessGlobalDecodeTreeState{false};

} // namespace

/** @brief Holds exclusive use of OpenCSD's process-global tree and logger state. */
class OpenCsdTreeSession::LoggerLease final {
public:
  /** @brief Acquires exclusive tree use and installs the supplied logger. */
  explicit LoggerLease(ITraceErrorLog& errorLogger)
  {
    bool expected = false;
    if (!ctraceOwnsProcessGlobalDecodeTreeState.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
      throw OpenCsdTreeSessionError("another OpenCSD DecodeTree session is already active");
    }
    m_previousLogger = DecodeTree::getCurrentErrorLogI();
    DecodeTree::setAlternateErrorLogger(&errorLogger);
  }

  /** @brief Restores the previous logger and releases exclusive tree use. */
  ~LoggerLease() noexcept
  {
    DecodeTree::setAlternateErrorLogger(m_previousLogger);
    ctraceOwnsProcessGlobalDecodeTreeState.store(false, std::memory_order_release);
  }

  /** @brief Disables copying because the lease has unique process ownership. */
  LoggerLease(const LoggerLease&) = delete;
  /** @brief Disables copy assignment because the lease has unique process ownership. */
  LoggerLease& operator=(const LoggerLease&) = delete;

private:
  ITraceErrorLog* m_previousLogger = nullptr;
};

void OpenCsdSessionValidation::requireObject(const void* object, const char* message)
{
  if (object == nullptr) {
    throw OpenCsdTreeSessionError(message);
  }
}

void OpenCsdSessionValidation::requireSuccess(ocsd_err_t error, const char* message)
{
  if (error != OCSD_OK) {
    throw OpenCsdTreeSessionError(OpenCsdErrorController::describeApiError(error, message));
  }
}

void OpenCsdTreeSession::TreeDeleter::operator()(DecodeTree* tree) const noexcept
{
  destroy(tree);
}

OpenCsdTreeSession::TreeLifecycle OpenCsdTreeSession::defaultLifecycle()
{
  return {
      [](ocsd_dcd_tree_src_t sourceType, std::uint32_t formatterFlags) {
        return DecodeTree::CreateDecodeTree(sourceType, formatterFlags);
      },
      [](DecodeTree* tree) { DecodeTree::DestroyDecodeTree(tree); },
  };
}

ocsd_dcd_tree_src_t OpenCsdTreeSession::validateSourceType(ocsd_dcd_tree_src_t sourceType)
{
  if (sourceType != OCSD_TRC_SRC_SINGLE && sourceType != OCSD_TRC_SRC_FRAME_FORMATTED) {
    throw OpenCsdTreeSessionError("unsupported OpenCSD DecodeTree source type");
  }
  return sourceType;
}

OpenCsdTreeSession::OpenCsdTreeSession(ocsd_dcd_tree_src_t sourceType, std::uint32_t formatterFlags,
                                       ITraceErrorLog& errorLogger, ITrcGenElemIn& elementOutput)
  : OpenCsdTreeSession(sourceType, formatterFlags, errorLogger, elementOutput, defaultLifecycle())
{
}

OpenCsdTreeSession::OpenCsdTreeSession(ocsd_dcd_tree_src_t sourceType, std::uint32_t formatterFlags,
                                       ITraceErrorLog& errorLogger, ITrcGenElemIn& elementOutput,
                                       const TreeLifecycle& lifecycle)
  : m_sourceType(validateSourceType(sourceType)),
    m_errorLogger(errorLogger),
    m_loggerLease(std::make_unique<LoggerLease>(errorLogger)),
    m_tree(nullptr, TreeDeleter{lifecycle.destroy})
{
  if (!lifecycle.create) {
    throw OpenCsdTreeSessionError("OpenCSD DecodeTree creator is not configured");
  }
  if (!lifecycle.destroy) {
    throw OpenCsdTreeSessionError("OpenCSD DecodeTree destroyer is not configured");
  }
  m_tree.reset(lifecycle.create(m_sourceType, formatterFlags));
  OpenCsdSessionValidation::requireObject(m_tree.get(), "failed to create OpenCSD DecodeTree");
  m_tree->setGenTraceElemOutI(&elementOutput);
}

OpenCsdTreeSession::~OpenCsdTreeSession() noexcept = default;

void OpenCsdTreeSession::createDecoder(const std::string& decoderName, int createFlags, const CSConfig& config)
{
  validateChannel(config.getTraceID());
  OpenCsdSessionValidation::requireSuccess(m_tree->createDecoder(decoderName, createFlags, &config),
                                           "failed to create OpenCSD decoder");
}

void OpenCsdTreeSession::attachDecoderCallbacks(std::uint8_t channel, ITrcTypedBase& packetMonitor)
{
  validateChannel(channel);
  auto* element = m_tree->getDecoderElement(channel);
  OpenCsdSessionValidation::requireObject(element, "OpenCSD decoder element is not initialized");
  auto* manager = element->getDecoderMngr();
  OpenCsdSessionValidation::requireObject(manager, "OpenCSD decoder manager is not initialized");
  auto* component = element->getDecoderHandle();
  OpenCsdSessionValidation::requireObject(component, "OpenCSD full decoder component is not initialized");
  auto* packetProcessor = component->getAssocComponent();
  OpenCsdSessionValidation::requireObject(packetProcessor, "OpenCSD associated packet processor is not initialized");

  // DecodeTree::createDecoder already attaches the active alternate logger to
  // the full decoder. OpenCSD does not propagate it to the associated packet
  // processor, where ITM protocol errors originate.
  OpenCsdSessionValidation::requireSuccess(manager->attachErrorLogger(packetProcessor, &m_errorLogger),
                                           "failed to attach OpenCSD packet-processor error logger");
  OpenCsdSessionValidation::requireSuccess(manager->attachPktMonitor(component, &packetMonitor),
                                           "failed to attach OpenCSD packet monitor");
}

void OpenCsdTreeSession::attachRawFrameMonitor(ITrcRawFrameIn& frameMonitor)
{
  if (m_sourceType != OCSD_TRC_SRC_FRAME_FORMATTED) {
    throw OpenCsdTreeSessionError("OpenCSD raw frame monitor requires a formatted DecodeTree");
  }
  auto* deformatter = m_tree->getFrameDeformatter();
  OpenCsdSessionValidation::requireObject(deformatter, "OpenCSD frame deformatter is not initialized");
  auto* attachPoint = deformatter->getTrcRawFrameAttachPt();
  OpenCsdSessionValidation::requireObject(attachPoint, "OpenCSD raw-frame attach point is not initialized");
  OpenCsdSessionValidation::requireSuccess(attachPoint->attach(&frameMonitor),
                                           "failed to attach OpenCSD raw frame monitor");
}

void OpenCsdTreeSession::validateChannel(std::uint8_t channel) const
{
  if (m_sourceType == OCSD_TRC_SRC_SINGLE) {
    if (channel != 0U) {
      throw OpenCsdTreeSessionError("OpenCSD SINGLE tree requires decoder channel 0");
    }
    return;
  }
  if (!OCSD_IS_VALID_CS_SRC_ID(channel)) {
    throw OpenCsdTreeSessionError("OpenCSD formatted tree requires a decoder channel between 1 and 111");
  }
}

ocsd_datapath_resp_t OpenCsdTreeSession::traceDataIn(ocsd_datapath_op_t operation, ocsd_trc_index_t index,
                                                     std::uint32_t size, const std::uint8_t* data,
                                                     std::uint32_t* processed)
{
  return m_tree->TraceDataIn(operation, index, size, data, processed);
}
