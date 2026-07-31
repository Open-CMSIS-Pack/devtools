/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#pragma once

#include "OpenCsdTestSupport.hpp"

#include "OpenCsdErrorController.hpp"
#include "OpenCsdItmDecoder.hpp"
#include "OpenCsdItmSession.hpp"
#include "OpenCsdPacketCollector.hpp"
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

struct ScriptedError {
  ocsd_err_severity_t severity = OCSD_ERR_SEV_ERROR;
  ocsd_err_t code = OCSD_ERR_FAIL;
  std::uint64_t index = 0U;
  std::string message;
};

struct SessionStep {
  SessionStep(ocsd_datapath_resp_t stepResponse = OCSD_RESP_CONT,
              std::optional<std::uint32_t> stepProcessed = std::nullopt, bool stepEmitSync = false,
              std::vector<ScriptedError> stepErrors = {})
    : response(stepResponse), processed(stepProcessed), emitSync(stepEmitSync), errors(std::move(stepErrors))
  {
  }

  ocsd_datapath_resp_t response = OCSD_RESP_CONT;
  std::optional<std::uint32_t> processed;
  bool emitSync = false;
  std::vector<ScriptedError> errors;
};

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

class ScriptedOpenCsdSession final : public OpenCsdItmSessionInterface {
public:
  ScriptedOpenCsdSession(std::shared_ptr<SessionScript> script, OpenCsdPacketCollector& collector,
                         OpenCsdErrorController& errors)
    : script_(std::move(script)), collector_(collector), errors_(errors)
  {
  }

  ocsd_datapath_resp_t pushData(ocsd_trc_index_t index, std::uint32_t size, const std::uint8_t*,
                                std::uint32_t& processed) override
  {
    ++script_->pushCalls;
    auto step = take(script_->pushes, SessionStep{});
    processed = step.processed.value_or(size);
    apply(step, index);
    return step.response;
  }

  ocsd_datapath_resp_t flush() override
  {
    ++script_->flushCalls;
    auto step = take(script_->flushes, SessionStep{script_->defaultFlushResponse});
    apply(step, 0U);
    return step.response;
  }

  ocsd_datapath_resp_t reset() override
  {
    ++script_->resetCalls;
    auto step = take(script_->resets, SessionStep{});
    apply(step, 0U);
    return step.response;
  }

  ocsd_datapath_resp_t endOfTrace() override
  {
    auto step = take(script_->ends, SessionStep{});
    apply(step, 0U);
    return step.response;
  }

private:
  static SessionStep take(std::deque<SessionStep>& steps, SessionStep fallback)
  {
    if (steps.empty()) {
      return fallback;
    }
    auto step = std::move(steps.front());
    steps.pop_front();
    return step;
  }

  void apply(const SessionStep& step, ocsd_trc_index_t index)
  {
    for (const auto& error : step.errors) {
      const ocsdError reported(error.severity, error.code, error.index, 1U, error.message);
      errors_.LogError(0U, &reported);
    }
    if (step.emitSync) {
      ItmTrcPacket packet;
      packet.setPktType(ITM_PKT_ASYNC);
      collector_.RawPacketDataMon(OCSD_OP_DATA, index, &packet, 0U, nullptr);
    }
  }

  std::shared_ptr<SessionScript> script_;
  OpenCsdPacketCollector& collector_;
  OpenCsdErrorController& errors_;
};

inline OpenCsdItmSessionFactory scriptedFactory(const std::shared_ptr<SessionScript>& script)
{
  return [script](OpenCsdPacketCollector& collector,
                  OpenCsdErrorController& errors) -> std::unique_ptr<OpenCsdItmSessionInterface> {
    return std::make_unique<ScriptedOpenCsdSession>(script, collector, errors);
  };
}

class ScriptedDecoderHarness {
public:
  ScriptedDecoderHarness() : script(std::make_shared<SessionScript>()), decoder(sink, scriptedFactory(script)) {}

  void push(std::uint32_t size)
  {
    input_.assign(size, 0U);
    decoder.push(input_.data(), size);
  }

  OpenCsdTestSupport::CollectingOpenCsdElementSink sink;
  std::shared_ptr<SessionScript> script;
  OpenCsdItmDecoder decoder;

private:
  std::vector<std::uint8_t> input_;
};

} // namespace OpenCsdSessionTestSupport
