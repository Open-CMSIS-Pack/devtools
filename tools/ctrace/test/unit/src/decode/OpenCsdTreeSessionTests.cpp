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
#include "OpenCsdPacketCollector.h"
#include "OpenCsdTreeSession.h"
#include "common/ocsd_dcd_tree.h"
#include "common/ocsd_dcd_tree_elem.h"
#include "common/trc_component.h"
#include "opencsd/itm/trc_cmp_cfg_itm.h"
#include "opencsd/ocsd_if_types.h"

#include <cstdint>
#include <string>

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
  OpenCsdPacketCollector collector{{}, sink};
  OpenCsdErrorController errors;
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
