/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_DECODE_OPENCSDTREESESSION_H
#define CTRACE_SRC_DECODE_OPENCSDTREESESSION_H

#include "opencsd/ocsd_if_types.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>

class CSConfig;
class DecodeTree;
class ITraceErrorLog;
class ITrcGenElemIn;
class ITrcRawFrameIn;
class ITrcTypedBase;

/** @brief Reports an OpenCSD tree-session creation or API failure. */
class OpenCsdTreeSessionError final : public std::runtime_error {
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
   * @throws OpenCsdTreeSessionError If object is null.
   */
  static void requireObject(const void* object, const char* message);
  /**
   * @brief Rejects an unsuccessful OpenCSD API result with a session error.
   * @param error OpenCSD result to validate.
   * @param message Failure text used for an error result.
   * @throws OpenCsdTreeSessionError If error does not report success.
   */
  static void requireSuccess(ocsd_err_t error, const char* message);

private:
  /** @brief Prevents construction of this stateless validation utility. */
  OpenCsdSessionValidation() = delete;
};

/**
 * @brief Owns one exclusively active OpenCSD DecodeTree and its global logger lease.
 *
 * Decoder feed and recovery policy remains outside this infrastructure wrapper.
 */
class OpenCsdTreeSession final {
public:
  /** @brief Creates an OpenCSD tree. */
  using TreeCreator = std::function<DecodeTree*(ocsd_dcd_tree_src_t, std::uint32_t)>;
  /** @brief Destroys a tree created by TreeCreator without throwing. */
  using TreeDestroyer = std::function<void(DecodeTree*)>;

  /** @brief Supplies DecodeTree construction operations for lifecycle tests. */
  struct TreeLifecycle {
    TreeCreator create;
    TreeDestroyer destroy;
  };

  /**
   * @brief Creates a tree and installs its logger and generic-element output.
   * @param sourceType OpenCSD root input type.
   * @param formatterFlags OpenCSD frame-deformatter configuration flags.
   * @param errorLogger Logger kept active for the complete tree lifetime.
   * @param elementOutput Generic trace-element output shared by configured decoders.
   * @throws OpenCsdTreeSessionError If the process already owns a live tree or setup fails.
   */
  OpenCsdTreeSession(ocsd_dcd_tree_src_t sourceType, std::uint32_t formatterFlags, ITraceErrorLog& errorLogger,
                     ITrcGenElemIn& elementOutput);
  /**
   * @brief Creates a tree through injectable lifecycle operations.
   * @param sourceType OpenCSD root input type.
   * @param formatterFlags OpenCSD frame-deformatter configuration flags.
   * @param errorLogger Logger kept active for the complete tree lifetime.
   * @param elementOutput Generic trace-element output shared by configured decoders.
   * @param lifecycle Tree creation and destruction operations.
   * @throws OpenCsdTreeSessionError If the process already owns a live tree or setup fails.
   */
  OpenCsdTreeSession(ocsd_dcd_tree_src_t sourceType, std::uint32_t formatterFlags, ITraceErrorLog& errorLogger,
                     ITrcGenElemIn& elementOutput, const TreeLifecycle& lifecycle);
  /** @brief Destroys the tree before restoring the previous process-global logger. */
  ~OpenCsdTreeSession() noexcept;

  /** @brief Disables copying because a session owns process-global and external state. */
  OpenCsdTreeSession(const OpenCsdTreeSession&) = delete;
  /** @brief Disables copy assignment because a session owns process-global and external state. */
  OpenCsdTreeSession& operator=(const OpenCsdTreeSession&) = delete;

  /** @brief Creates one protocol decoder in the tree. */
  void createDecoder(const std::string& decoderName, int createFlags, const CSConfig& config);
  /**
   * @brief Attaches explicit error and packet callbacks to one full decoder pair.
   * @param channel OpenCSD transport channel used to resolve the decoder element.
   * @param packetMonitor Raw protocol-packet monitor attached to the packet processor.
   */
  void attachDecoderCallbacks(std::uint8_t channel, ITrcTypedBase& packetMonitor);
  /**
   * @brief Attaches the one raw-frame monitor supported by a formatted tree.
   * @param frameMonitor Monitor receiving deformatter observations.
   * @throws OpenCsdTreeSessionError If this is not a formatted tree or attachment fails.
   */
  void attachRawFrameMonitor(ITrcRawFrameIn& frameMonitor);
  /**
   * @brief Resets one decoder pair without resetting the DecodeTree frontend.
   * @param channel OpenCSD transport channel used to resolve the decoder element.
   * @param index Raw input offset associated with the recovery boundary.
   * @return Data-path response from the route's packet processor.
   * @throws OpenCsdTreeSessionError If the route cannot be resolved or reset.
   */
  ocsd_datapath_resp_t resetDecoder(std::uint8_t channel, ocsd_trc_index_t index);
  /** @brief Routes one data-path operation through the DecodeTree root. */
  ocsd_datapath_resp_t traceDataIn(ocsd_datapath_op_t operation, ocsd_trc_index_t index, std::uint32_t size,
                                   const std::uint8_t* data, std::uint32_t* processed);

private:
  class LoggerLease;

  /** @brief Destroys a DecodeTree through its configured lifecycle operation. */
  struct TreeDeleter {
    TreeDestroyer destroy;
    /** @brief Destroys a non-null tree through the no-throw lifecycle contract. */
    void operator()(DecodeTree* tree) const noexcept;
  };

  /** @brief Returns production DecodeTree construction operations. */
  static TreeLifecycle defaultLifecycle();
  /** @brief Rejects source types unsupported by the common ctrace tree wrapper. */
  static ocsd_dcd_tree_src_t validateSourceType(ocsd_dcd_tree_src_t sourceType);
  /** @brief Enforces the channel namespace of the configured tree source type. */
  void validateChannel(std::uint8_t channel) const;

  // Declaration order is intentional: reverse destruction removes the tree
  // before the global logger lease can restore its predecessor.
  ocsd_dcd_tree_src_t m_sourceType;
  ITraceErrorLog& m_errorLogger;
  std::unique_ptr<LoggerLease> m_loggerLease;
  std::unique_ptr<DecodeTree, TreeDeleter> m_tree;
};

#endif // CTRACE_SRC_DECODE_OPENCSDTREESESSION_H
