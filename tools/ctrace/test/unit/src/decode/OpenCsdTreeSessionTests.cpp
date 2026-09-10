/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "OpenCsdTestSupport.h"
#include "TestSupport.h"

#include <gtest/gtest.h>

#include "OpenCsdErrorController.h"
#include "OpenCsdItmSession.h"
#include "OpenCsdPacketCollector.h"
#include "OpenCsdTreeSession.h"
#include "common/ocsd_dcd_tree.h"
#include "common/ocsd_dcd_tree_elem.h"
#include "common/trc_component.h"
#include "interfaces/trc_pkt_raw_in_i.h"
#include "opencsd/itm/trc_cmp_cfg_itm.h"
#include "opencsd/itm/trc_pkt_types_itm.h"
#include "opencsd/ocsd_if_types.h"

#include <cstdint>
#include <string>
#include <vector>

using OpenCsdTestSupport::CollectingOpenCsdElementSink;

namespace {

/** @brief Installs one foreign OpenCSD logger and restores its predecessor. */
class ScopedAlternateLogger final {
public:
  /** @brief Saves the current logger and installs the supplied replacement. */
  explicit ScopedAlternateLogger(ITraceErrorLog& logger)
    : m_previous(DecodeTree::getCurrentErrorLogI())
  {
    DecodeTree::setAlternateErrorLogger(&logger);
  }

  /** @brief Restores the logger that preceded this scope. */
  ~ScopedAlternateLogger()
  {
    DecodeTree::setAlternateErrorLogger(m_previous);
  }

  /** @brief Disables copying because this object owns a process-global scope. */
  ScopedAlternateLogger(const ScopedAlternateLogger&) = delete;
  /** @brief Disables copy assignment because this object owns a process-global scope. */
  ScopedAlternateLogger& operator=(const ScopedAlternateLogger&) = delete;

private:
  ITraceErrorLog* m_previous;
};

/** @brief Creates a collector bound to a collecting sink for one tree test. */
struct TreeTestContext {
  CollectingOpenCsdElementSink sink;
  OpenCsdPacketCollector collector{TraceRouteIdentity{}, sink};
  OpenCsdErrorController errors;
};

/** @brief Records reset notifications from one ITM packet processor. */
class ResetRecordingPacketMonitor final : public IPktRawDataMon<ItmTrcPacket> {
public:
  /** @brief Records route-local reset operations and their recovery indices. */
  void RawPacketDataMon(ocsd_datapath_op_t operation, ocsd_trc_index_t index, const ItmTrcPacket*, std::uint32_t,
                        const std::uint8_t*) override
  {
    if (operation == OCSD_OP_RESET) {
      m_resetIndices.push_back(index);
    }
  }

  /** @brief Returns the recovery indices observed for reset operations. */
  const std::vector<ocsd_trc_index_t>& resetIndices() const
  {
    return m_resetIndices;
  }

private:
  std::vector<ocsd_trc_index_t> m_resetIndices;
};

/** @brief Models the explicit one-route reset behavior of a SINGLE session. */
class SingleFallbackSession final : public OpenCsdItmSessionInterface {
public:
  /** @brief Accepts unused test input. */
  ocsd_datapath_resp_t pushData(ocsd_trc_index_t, std::uint32_t, const std::uint8_t*, std::uint32_t&) override
  {
    return OCSD_RESP_CONT;
  }

  /** @brief Accepts an unused flush. */
  ocsd_datapath_resp_t flush() override
  {
    return OCSD_RESP_CONT;
  }

  /** @brief Records the complete reset used for a SINGLE route. */
  ocsd_datapath_resp_t reset() override
  {
    ++resetCalls;
    return OCSD_RESP_WARN_CONT;
  }

  /** @brief Maps the synthetic SINGLE route to the complete one-decoder reset. */
  ocsd_datapath_resp_t resetRoute(std::uint8_t, ocsd_trc_index_t) override
  {
    return reset();
  }

  /** @brief Accepts an unused end-of-trace operation. */
  ocsd_datapath_resp_t endOfTrace() override
  {
    return OCSD_RESP_CONT;
  }

  std::uint32_t resetCalls = 0U;
};

} // namespace

TEST(CtraceUnitTests, testOpenCsdTreeSessionRestoresForeignLoggerAfterDestroyingTree)
{
  OpenCsdErrorController foreignLogger;
  ScopedAlternateLogger foreignLoggerScope(foreignLogger);
  TreeTestContext context;
  bool createdWithSessionLogger = false;
  bool destroyedWithSessionLogger = false;
  bool fullDecoderUsesSessionLogger = false;
  bool packetProcessorUsesSessionLogger = false;
  bool destroyed = false;
  ocsd_dcd_tree_src_t observedSourceType = OCSD_TRC_SRC_FRAME_FORMATTED;
  std::uint32_t observedFormatterFlags = 1U;

  const OpenCsdTreeSession::TreeLifecycle lifecycle{
      [&](ocsd_dcd_tree_src_t sourceType, std::uint32_t formatterFlags) {
        observedSourceType = sourceType;
        observedFormatterFlags = formatterFlags;
        createdWithSessionLogger = DecodeTree::getCurrentErrorLogI() == &context.errors;
        return DecodeTree::CreateDecodeTree(sourceType, formatterFlags);
      },
      [&](DecodeTree* tree) {
        destroyedWithSessionLogger = DecodeTree::getCurrentErrorLogI() == &context.errors;
        auto* element = tree->getDecoderElement(0U);
        auto* fullDecoder = element == nullptr ? nullptr : element->getDecoderHandle();
        auto* packetProcessor = fullDecoder == nullptr ? nullptr : fullDecoder->getAssocComponent();
        fullDecoderUsesSessionLogger =
            fullDecoder != nullptr && fullDecoder->getErrorLogAttachPt()->first() == &context.errors;
        packetProcessorUsesSessionLogger =
            packetProcessor != nullptr && packetProcessor->getErrorLogAttachPt()->first() == &context.errors;
        DecodeTree::DestroyDecodeTree(tree);
        destroyed = true;
      },
  };

  ITMConfig config;
  config.setTraceID(0U);
  {
    OpenCsdTreeSession session(OCSD_TRC_SRC_SINGLE, 0U, context.errors, context.collector, lifecycle);
    session.createDecoder(OCSD_BUILTIN_DCD_ITM, OCSD_CREATE_FLG_FULL_DECODER, config);
    session.attachDecoderCallbacks(0U, context.collector);
    EXPECT_EQ(DecodeTree::getCurrentErrorLogI(), &context.errors);
  }

  EXPECT_TRUE(createdWithSessionLogger);
  EXPECT_TRUE(destroyedWithSessionLogger);
  EXPECT_TRUE(fullDecoderUsesSessionLogger);
  EXPECT_TRUE(packetProcessorUsesSessionLogger);
  EXPECT_TRUE(destroyed);
  EXPECT_EQ(observedSourceType, OCSD_TRC_SRC_SINGLE);
  EXPECT_EQ(observedFormatterFlags, 0U);
  EXPECT_EQ(DecodeTree::getCurrentErrorLogI(), &foreignLogger);
}

TEST(CtraceUnitTests, testOpenCsdTreeSessionRejectsOverlapAndReleasesLease)
{
  OpenCsdErrorController foreignLogger;
  ScopedAlternateLogger foreignLoggerScope(foreignLogger);
  TreeTestContext first;
  TreeTestContext second;

  {
    OpenCsdTreeSession active(OCSD_TRC_SRC_SINGLE, 0U, first.errors, first.collector);
    EXPECT_THROW((void)OpenCsdTreeSession(OCSD_TRC_SRC_SINGLE, 0U, second.errors, second.collector),
                 OpenCsdTreeSessionError);
    EXPECT_EQ(DecodeTree::getCurrentErrorLogI(), &first.errors);
  }

  EXPECT_EQ(DecodeTree::getCurrentErrorLogI(), &foreignLogger);
  EXPECT_NO_THROW((void)OpenCsdTreeSession(OCSD_TRC_SRC_SINGLE, 0U, second.errors, second.collector));
  EXPECT_EQ(DecodeTree::getCurrentErrorLogI(), &foreignLogger);
}

TEST(CtraceUnitTests, testOpenCsdTreeSessionCleansUpConstructionFailures)
{
  OpenCsdErrorController foreignLogger;
  ScopedAlternateLogger foreignLoggerScope(foreignLogger);
  TreeTestContext context;
  const auto destroyTree = [](DecodeTree* tree) { DecodeTree::DestroyDecodeTree(tree); };

  const OpenCsdTreeSession::TreeLifecycle missingCreator{{}, destroyTree};
  EXPECT_THROW((void)OpenCsdTreeSession(OCSD_TRC_SRC_SINGLE, 0U, context.errors, context.collector, missingCreator),
               OpenCsdTreeSessionError);
  EXPECT_EQ(DecodeTree::getCurrentErrorLogI(), &foreignLogger);

  std::uint32_t missingDestroyerCreateCalls = 0U;
  const auto createWithoutDestroyer = [&](ocsd_dcd_tree_src_t, std::uint32_t) -> DecodeTree* {
    ++missingDestroyerCreateCalls;
    return nullptr;
  };
  const OpenCsdTreeSession::TreeLifecycle missingDestroyer{createWithoutDestroyer, {}};
  EXPECT_THROW((void)OpenCsdTreeSession(OCSD_TRC_SRC_SINGLE, 0U, context.errors, context.collector, missingDestroyer),
               OpenCsdTreeSessionError);
  EXPECT_EQ(missingDestroyerCreateCalls, 0U);
  EXPECT_EQ(DecodeTree::getCurrentErrorLogI(), &foreignLogger);

  const OpenCsdTreeSession::TreeLifecycle failedCreate{
      [](ocsd_dcd_tree_src_t, std::uint32_t) -> DecodeTree* { return nullptr; }, destroyTree};
  EXPECT_THROW((void)OpenCsdTreeSession(OCSD_TRC_SRC_SINGLE, 0U, context.errors, context.collector, failedCreate),
               OpenCsdTreeSessionError);
  EXPECT_EQ(DecodeTree::getCurrentErrorLogI(), &foreignLogger);

  const OpenCsdTreeSession::TreeLifecycle throwingCreate{
      [](ocsd_dcd_tree_src_t, std::uint32_t) -> DecodeTree* {
        throw OpenCsdTreeSessionError("synthetic DecodeTree creation failure");
      },
      destroyTree,
  };
  EXPECT_THROW((void)OpenCsdTreeSession(OCSD_TRC_SRC_SINGLE, 0U, context.errors, context.collector, throwingCreate),
               OpenCsdTreeSessionError);
  EXPECT_EQ(DecodeTree::getCurrentErrorLogI(), &foreignLogger);

  EXPECT_NO_THROW((void)OpenCsdTreeSession(OCSD_TRC_SRC_SINGLE, 0U, context.errors, context.collector));
  EXPECT_EQ(DecodeTree::getCurrentErrorLogI(), &foreignLogger);
}

TEST(CtraceUnitTests, testOpenCsdTreeSessionCleansUpPartialDecoderSetupFailure)
{
  OpenCsdErrorController foreignLogger;
  ScopedAlternateLogger foreignLoggerScope(foreignLogger);
  TreeTestContext context;
  ITMConfig config;
  std::uint32_t destroyCalls = 0U;
  const auto createTree = [](ocsd_dcd_tree_src_t sourceType, std::uint32_t formatterFlags) {
    return DecodeTree::CreateDecodeTree(sourceType, formatterFlags);
  };
  const auto destroyTree = [&](DecodeTree* tree) {
    ++destroyCalls;
    DecodeTree::DestroyDecodeTree(tree);
  };
  const OpenCsdTreeSession::TreeLifecycle lifecycle{createTree, destroyTree};

  const auto setupInvalidDecoder = [&] {
    OpenCsdTreeSession session(OCSD_TRC_SRC_SINGLE, 0U, context.errors, context.collector, lifecycle);
    session.createDecoder("missing-decoder", OCSD_CREATE_FLG_FULL_DECODER, config);
  };
  EXPECT_THROW(setupInvalidDecoder(), OpenCsdTreeSessionError);
  EXPECT_EQ(destroyCalls, 1U);
  EXPECT_EQ(DecodeTree::getCurrentErrorLogI(), &foreignLogger);

  EXPECT_NO_THROW((void)OpenCsdTreeSession(OCSD_TRC_SRC_SINGLE, 0U, context.errors, context.collector));
  EXPECT_EQ(DecodeTree::getCurrentErrorLogI(), &foreignLogger);
}

TEST(CtraceUnitTests, testOpenCsdTreeSessionResetsOnlyResolvedDecoder)
{
  constexpr std::uint32_t flags = OCSD_DFRMTR_FRAME_MEM_ALIGN | OCSD_DFRMTR_UNPACKED_RAW_OUT;
  TreeTestContext context;
  OpenCsdTreeSession session(OCSD_TRC_SRC_FRAME_FORMATTED, flags, context.errors, context.collector);
  ITMConfig config;
  ResetRecordingPacketMonitor firstMonitor;
  ResetRecordingPacketMonitor secondMonitor;

  config.setTraceID(1U);
  session.createDecoder(OCSD_BUILTIN_DCD_ITM, OCSD_CREATE_FLG_FULL_DECODER, config);
  session.attachDecoderCallbacks(1U, firstMonitor);
  config.setTraceID(111U);
  session.createDecoder(OCSD_BUILTIN_DCD_ITM, OCSD_CREATE_FLG_FULL_DECODER, config);
  session.attachDecoderCallbacks(111U, secondMonitor);

  EXPECT_EQ(session.resetDecoder(1U, 37U), OCSD_RESP_CONT);
  ASSERT_EQ(firstMonitor.resetIndices().size(), 1U);
  EXPECT_EQ(firstMonitor.resetIndices().front(), 37U);
  EXPECT_TRUE(secondMonitor.resetIndices().empty());

  EXPECT_THROW(session.resetDecoder(0U, 40U), OpenCsdTreeSessionError);
  EXPECT_THROW(session.resetDecoder(42U, 41U), OpenCsdTreeSessionError);
  EXPECT_THROW(session.resetDecoder(112U, 42U), OpenCsdTreeSessionError);
}

TEST(CtraceUnitTests, testOpenCsdTreeSessionResolvesSingleDecoderAtChannelZero)
{
  TreeTestContext context;
  OpenCsdTreeSession session(OCSD_TRC_SRC_SINGLE, 0U, context.errors, context.collector);
  ITMConfig config;
  ResetRecordingPacketMonitor monitor;

  config.setTraceID(0U);
  session.createDecoder(OCSD_BUILTIN_DCD_ITM, OCSD_CREATE_FLG_FULL_DECODER, config);
  session.attachDecoderCallbacks(0U, monitor);

  EXPECT_EQ(session.resetDecoder(0U, 73U), OCSD_RESP_CONT);
  ASSERT_EQ(monitor.resetIndices().size(), 1U);
  EXPECT_EQ(monitor.resetIndices().front(), 73U);
  EXPECT_THROW(session.resetDecoder(1U, 74U), OpenCsdTreeSessionError);
}

TEST(CtraceUnitTests, testOpenCsdSessionRouteResetFallsBackForSingleInput)
{
  SingleFallbackSession session;

  EXPECT_EQ(session.resetRoute(0U, 37U), OCSD_RESP_WARN_CONT);
  EXPECT_EQ(session.resetCalls, 1U);
}
