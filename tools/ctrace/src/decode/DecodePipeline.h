/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_DECODE_DECODEPIPELINE_H
#define CTRACE_SRC_DECODE_DECODEPIPELINE_H

#include "CortexMStreamDecoder.h"
#include "TraceEvent.h"
#include "OpenCsdItmDecoder.h"

#include <cstddef>
#include <cstdint>
#include <vector>

/** @brief Provides a non-owning view of one raw trace byte chunk. */
struct RawByteView {
  const std::uint8_t* data = nullptr;
  std::size_t size = 0;

  /** @brief Reports whether the view contains no bytes. */
  constexpr bool empty() const
  {
    return size == 0U;
  }
};

/** @brief Summarizes raw bytes consumed and trace/diagnostic records produced before output filtering. */
struct DecodeResult {
  std::uint64_t bytesIn = 0;
  std::uint64_t eventsOut = 0;
};

/** @brief Connects raw OpenCSD decoding with Cortex-M semantic post-decoding. */
class DecodePipeline final {
public:
  /**
   * @brief Creates a pipeline for normalized SINGLE or formatted routes.
   * @param routes Route identities and timestamp prescalers used by the decoders.
   * @param inputMode Raw transport presented to OpenCSD.
   * @param eventSink Sink receiving decoded events synchronously.
   * @param unsupportedTraceIdSink Observer for unconfigured normal formatted IDs.
   */
  DecodePipeline(std::vector<CortexMDecodeRoute> routes, OpenCsdItmInputMode inputMode, TraceEventSink& eventSink,
                 OpenCsdUnsupportedTraceIdObserver unsupportedTraceIdSink = {});
  /**
   * @brief Creates a configured pipeline with an injected OpenCSD session.
   * @param routes Route identities and timestamp prescalers used by the decoders.
   * @param inputMode Decoder policy mode applied around the injected session.
   * @param eventSink Sink receiving decoded events synchronously.
   * @param sessionFactory Factory used to create the OpenCSD session.
   */
  DecodePipeline(std::vector<CortexMDecodeRoute> routes, OpenCsdItmInputMode inputMode, TraceEventSink& eventSink,
                 const OpenCsdItmSessionFactory& sessionFactory);

  /**
   * @brief Pushes the next contiguous chunk of raw trace bytes.
   * @param bytes Non-owning byte view that remains valid for this call.
   * @throws OpenCsdFatalError If the external decoder cannot continue safely.
   */
  void push(RawByteView bytes);
  /**
   * @brief Finalizes decoding and returns aggregate counters.
   * @return Total raw bytes consumed and trace/diagnostic records emitted before output filtering.
   * @throws OpenCsdFatalError If decoder finalization fails.
   */
  DecodeResult finish();

private:
  CortexMStreamDecoder m_streamDecoder;
  std::uint64_t m_byteSkipCount = 0U;
  OpenCsdItmDecoder m_decoder;
};

#endif // CTRACE_SRC_DECODE_DECODEPIPELINE_H
