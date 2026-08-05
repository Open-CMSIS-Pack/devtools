/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_TEST_UNIT_SUPPORT_OPENCSDSESSIONTESTSUPPORT_H
#define CTRACE_TEST_UNIT_SUPPORT_OPENCSDSESSIONTESTSUPPORT_H

#include "OpenCsdTestSupport.h"

#include "OpenCsdErrorController.h"
#include "OpenCsdItmDecoder.h"
#include "OpenCsdItmSession.h"
#include "OpenCsdPacketCollector.h"
#include "common/ocsd_error.h"
#include "opencsd/itm/trc_pkt_elem_itm.h"
#include "opencsd/itm/trc_pkt_types_itm.h"

#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace OpenCsdSessionTestSupport {

/** @brief Describes one OpenCSD error emitted by a scripted session step. */
struct ScriptedError {
  ocsd_err_severity_t severity = OCSD_ERR_SEV_ERROR;
  ocsd_err_t code = OCSD_ERR_FAIL;
  std::uint64_t index = 0U;
  std::string message;
};

/** @brief Describes one response returned by a scripted OpenCSD session. */
struct SessionStep {
  /** @brief Creates a scripted response and its optional side effects. */
  SessionStep(ocsd_datapath_resp_t stepResponse = OCSD_RESP_CONT,
              std::optional<std::uint32_t> stepProcessed = std::nullopt, bool stepEmitSync = false,
              std::vector<ScriptedError> stepErrors = {})
    : response(stepResponse),
      processed(stepProcessed),
      emitSync(stepEmitSync),
      errors(std::move(stepErrors))
  {
  }

  ocsd_datapath_resp_t response = OCSD_RESP_CONT;
  std::optional<std::uint32_t> processed;
  bool emitSync = false;
  std::vector<ScriptedError> errors;
};

/** @brief Stores the queued operations and call counters of a scripted session. */
struct SessionScript {
  std::deque<SessionStep> pushes;
  std::deque<SessionStep> flushes;
  std::deque<SessionStep> resets;
  std::deque<SessionStep> ends;
  ocsd_datapath_resp_t defaultFlushResponse = OCSD_RESP_CONT;
  std::uint32_t pushCalls = 0U;
  std::uint32_t flushCalls = 0U;
  std::uint32_t resetCalls = 0U;
};

/** @brief Implements a deterministic OpenCSD session for decoder unit tests. */
class ScriptedOpenCsdSession final : public OpenCsdItmSessionInterface {
public:
  /** @brief Creates a session backed by shared scripted state. */
  ScriptedOpenCsdSession(std::shared_ptr<SessionScript> script, OpenCsdPacketCollector& collector,
                         OpenCsdErrorController& errors)
    : m_script(std::move(script)),
      m_collector(collector),
      m_errors(errors)
  {
  }

  /** @brief Applies the next scripted data response. */
  ocsd_datapath_resp_t pushData(ocsd_trc_index_t index, std::uint32_t size, const std::uint8_t*,
                                std::uint32_t& processed) override
  {
    ++m_script->pushCalls;
    auto step = take(m_script->pushes, SessionStep{});
    processed = step.processed.value_or(size);
    apply(step, index);
    return step.response;
  }

  /** @brief Applies the next scripted flush response. */
  ocsd_datapath_resp_t flush() override
  {
    ++m_script->flushCalls;
    auto step = take(m_script->flushes, SessionStep{m_script->defaultFlushResponse});
    apply(step, 0U);
    return step.response;
  }

  /** @brief Applies the next scripted reset response. */
  ocsd_datapath_resp_t reset() override
  {
    ++m_script->resetCalls;
    auto step = take(m_script->resets, SessionStep{});
    apply(step, 0U);
    return step.response;
  }

  /** @brief Applies the next scripted end-of-trace response. */
  ocsd_datapath_resp_t endOfTrace() override
  {
    auto step = take(m_script->ends, SessionStep{});
    apply(step, 0U);
    return step.response;
  }

private:
  /** @brief Removes and returns the next scripted step or a fallback. */
  static SessionStep take(std::deque<SessionStep>& steps, SessionStep fallback)
  {
    if (steps.empty()) {
      return fallback;
    }
    auto step = std::move(steps.front());
    steps.pop_front();
    return step;
  }

  /** @brief Applies scripted errors and synchronization side effects. */
  void apply(const SessionStep& step, ocsd_trc_index_t index)
  {
    for (const auto& error : step.errors) {
      const ocsdError reported(error.severity, error.code, error.index, 1U, error.message);
      m_errors.LogError(0U, &reported);
    }
    if (step.emitSync) {
      ItmTrcPacket packet;
      packet.setPktType(ITM_PKT_ASYNC);
      m_collector.RawPacketDataMon(OCSD_OP_DATA, index, &packet, 0U, nullptr);
    }
  }

  std::shared_ptr<SessionScript> m_script;
  OpenCsdPacketCollector& m_collector;
  OpenCsdErrorController& m_errors;
};

/** @brief Creates a session factory sharing the supplied script. */
inline OpenCsdItmSessionFactory scriptedFactory(const std::shared_ptr<SessionScript>& script)
{
  return [script](OpenCsdPacketCollector& collector,
                  OpenCsdErrorController& errors) -> std::unique_ptr<OpenCsdItmSessionInterface> {
    return std::make_unique<ScriptedOpenCsdSession>(script, collector, errors);
  };
}

/** @brief Bundles a scripted session, decoder, and collecting sink for tests. */
class ScriptedDecoderHarness {
public:
  /** @brief Creates a decoder connected to a new empty session script. */
  ScriptedDecoderHarness()
    : m_script(std::make_shared<SessionScript>()),
      m_decoder(m_sink, scriptedFactory(m_script))
  {
  }

  /** @brief Pushes a zero-filled raw byte block through the decoder. */
  void push(std::uint32_t size)
  {
    m_input.assign(size, 0U);
    m_decoder.push(m_input.data(), size);
  }

  /** @brief Returns the collecting element sink. */
  OpenCsdTestSupport::CollectingOpenCsdElementSink& sink()
  {
    return m_sink;
  }

  /** @brief Returns the mutable session script. */
  SessionScript& script()
  {
    return *m_script;
  }

  /** @brief Returns the decoder under test. */
  OpenCsdItmDecoder& decoder()
  {
    return m_decoder;
  }

private:
  OpenCsdTestSupport::CollectingOpenCsdElementSink m_sink;
  std::shared_ptr<SessionScript> m_script;
  OpenCsdItmDecoder m_decoder;
  std::vector<std::uint8_t> m_input;
};

} // namespace OpenCsdSessionTestSupport

#endif  // CTRACE_TEST_UNIT_SUPPORT_OPENCSDSESSIONTESTSUPPORT_H
