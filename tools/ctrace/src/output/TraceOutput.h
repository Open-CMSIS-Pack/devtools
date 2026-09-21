/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_OUTPUT_TRACEOUTPUT_H
#define CTRACE_SRC_OUTPUT_TRACEOUTPUT_H

#include "TraceEvent.h"

#include <string>
#include <string_view>

/**
 * @brief Defines the lifecycle and event interface of a trace output backend.
 *
 * A caller invokes start(), writes events synchronously in decode order, and
 * then invokes stop(). If any operation fails, abort() removes incomplete output.
 */
class TraceOutput {
public:
  /** @brief Destroys an output backend through its interface. */
  virtual ~TraceOutput() = default;

  /**
   * @brief Returns the stable backend name used in diagnostics.
   * @return Non-owning backend identifier with static lifetime.
   */
  virtual std::string_view backendName() const noexcept
  {
    return "trace";
  }
  /**
   * @brief Returns the primary output target path used in diagnostics.
   * @return Displayable path, or an empty string when no target exists.
   */
  virtual std::string targetPath() const
  {
    return {};
  }

  /** @brief Prepares a new final output target before the first event. */
  void start();
  /**
   * @brief Completes output, or applies the backend's policy for a fatal decode abort.
   * @param decodeAbort Optional input failure, consumed synchronously without retaining its address.
   */
  void stop(const TraceDecodeAbort* decodeAbort = nullptr);
  /** @brief Discards an incomplete active output without committing partial data. */
  void abort();
  /**
   * @brief Writes one event synchronously in decode order.
   * @param event Decoded event whose lifetime extends through this call.
   */
  void writeEvent(const TraceEvent& event);
  /** @brief Writes skipped-byte accounting without an associated timestamp. */
  void writeByteSkip(const TraceByteSkip& skipped);

protected:
  /** @brief Validates and prepares targets before this output owns incomplete artifacts. */
  virtual void prepareOutput() = 0;
  /** @brief Opens backend resources after the output becomes active. */
  virtual void startOutput() = 0;
  /** @brief Flushes and commits backend resources while the output remains active. */
  virtual void stopOutput() = 0;
  /** @brief Discards partial artifacts unless a backend can explicitly mark and retain them. */
  virtual void stopAfterDecodeAbortOutput(const TraceDecodeAbort&) { abortOutput(); }
  /** @brief Releases backend resources and removes incomplete artifacts. */
  virtual void abortOutput() = 0;
  /** @brief Writes one event to an active backend. */
  virtual void writeOutput(const TraceEvent& event) = 0;
  /** @brief Optionally retains skipped-byte accounting in an active backend. */
  virtual void writeByteSkipOutput(const TraceByteSkip&) {}

  /** @brief Aborts an active output while suppressing every cleanup exception. */
  void abortNoexcept() noexcept;

private:
  /** @brief Applies the active-output guard and cleanup to either kind of write. */
  template <typename Write> void writeActive(const Write& write);

  bool m_active = false;
};

inline void TraceOutput::start()
{
  abort();
  prepareOutput();
  m_active = true;
  try {
    startOutput();
  } catch (...) {
    abort();
    throw;
  }
}

inline void TraceOutput::stop(const TraceDecodeAbort* decodeAbort)
{
  if (!m_active) {
    return;
  }
  try {
    if (decodeAbort == nullptr) {
      stopOutput();
    } else {
      stopAfterDecodeAbortOutput(*decodeAbort);
    }
  } catch (...) {
    abort();
    throw;
  }
  m_active = false;
}

inline void TraceOutput::abort()
{
  if (!m_active) {
    return;
  }
  abortOutput();
  m_active = false;
}

inline void TraceOutput::writeEvent(const TraceEvent& event)
{
  writeActive([&] { writeOutput(event); });
}

inline void TraceOutput::writeByteSkip(const TraceByteSkip& skipped)
{
  writeActive([&] { writeByteSkipOutput(skipped); });
}

template <typename Write> inline void TraceOutput::writeActive(const Write& write)
{
  if (!m_active) {
    return;
  }
  try {
    write();
  } catch (...) {
    abort();
    throw;
  }
}

inline void TraceOutput::abortNoexcept() noexcept
{
  try {
    abort();
  } catch (...) {
    // Destruction cannot report cleanup failures safely.
    (void)0;
  }
}

#endif  // CTRACE_SRC_OUTPUT_TRACEOUTPUT_H
