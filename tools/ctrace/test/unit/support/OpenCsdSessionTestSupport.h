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
#include "TraceRoute.h"
#include "common/trc_gen_elem.h"
#include "common/ocsd_error.h"
#include "opencsd/ocsd_if_types.h"
#include "opencsd/itm/trc_pkt_elem_itm.h"
#include "opencsd/itm/trc_pkt_types_itm.h"
#include "opencsd/trc_gen_elem_types.h"

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace OpenCsdSessionTestSupport {

/** @brief Describes one OpenCSD error emitted by a scripted session step. */
struct ScriptedError {
  ocsd_err_severity_t severity = OCSD_ERR_SEV_ERROR;
  ocsd_err_t code = OCSD_ERR_FAIL;
  std::uint64_t index = 0U;
  std::string message;
  std::optional<std::uint8_t> channel = 1U;
};

/** @brief Describes one generic trace-element callback emitted by a scripted operation. */
struct ScriptedGenericCallback {
  std::uint8_t channel = 0U;
  ocsd_trc_index_t index = 0U;
  OcsdTraceElement element;
};

/** @brief Describes one route-bound raw-packet callback emitted by a scripted operation. */
struct ScriptedRawPacketCallback {
  TraceRouteIdentity route;
  ocsd_datapath_op_t operation = OCSD_OP_DATA;
  ocsd_trc_index_t index = 0U;
  ocsd_itm_pkt_type packetType = ITM_PKT_NOTSYNC;
};

/** @brief Stores one ordered OpenCSD observation from a scripted data-path operation. */
using ScriptedObservation = std::variant<ScriptedError, ScriptedGenericCallback, ScriptedRawPacketCallback>;

/** @brief Creates one ordered OpenCSD error observation. */
inline ScriptedObservation errorObservation(ocsd_err_t code, std::uint64_t index,
                                            std::optional<std::uint8_t> channel = 1U, std::string message = {},
                                            ocsd_err_severity_t severity = OCSD_ERR_SEV_ERROR)
{
  return ScriptedError{severity, code, index, std::move(message), channel};
}

/** @brief Creates one ordered generic-element callback. */
inline ScriptedObservation genericCallback(std::uint8_t channel, ocsd_trc_index_t index, OcsdTraceElement element)
{
  return ScriptedGenericCallback{channel, index, std::move(element)};
}

/** @brief Creates one routed ITM software callback. */
inline ScriptedObservation softwareCallback(const TraceRouteIdentity& route, ocsd_trc_index_t index,
                                            std::uint8_t source, std::uint32_t value, std::uint8_t size = 1U)
{
  OcsdTraceElement element;
  element.setType(OCSD_GEN_TRC_ELEM_ITMTRACE);
  swt_itm_info info{};
  info.pkt_type = SWIT_PAYLOAD;
  info.payload_src_id = source;
  info.payload_size = size;
  info.value = value;
  element.setSWT_ITMInfo(info);
  return genericCallback(route.traceBusId.value_or(0U), index, std::move(element));
}

/** @brief Creates one ordered route-bound raw-packet callback. */
inline ScriptedObservation rawPacketCallback(TraceRouteIdentity route, ocsd_trc_index_t index,
                                             ocsd_itm_pkt_type packetType, ocsd_datapath_op_t operation = OCSD_OP_DATA)
{
  return ScriptedRawPacketCallback{std::move(route), operation, index, packetType};
}

/** @brief Creates one ordered route-bound ITM hardware-sync callback. */
inline ScriptedObservation syncCallback(TraceRouteIdentity route, ocsd_trc_index_t index)
{
  return rawPacketCallback(std::move(route), index, ITM_PKT_ASYNC);
}

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

  /** @brief Creates a scripted response with a precisely ordered observation batch. */
  SessionStep(ocsd_datapath_resp_t stepResponse, std::optional<std::uint32_t> stepProcessed,
              std::vector<ScriptedObservation> stepObservations)
    : response(stepResponse),
      processed(stepProcessed),
      observations(std::move(stepObservations))
  {
  }

  ocsd_datapath_resp_t response = OCSD_RESP_CONT;
  std::optional<std::uint32_t> processed;
  bool emitSync = false;
  std::vector<ScriptedError> errors;
  std::vector<ScriptedObservation> observations;
};

/** @brief Stores the queued operations and call counters of a scripted session. */
struct SessionScript {
  std::deque<SessionStep> pushes;
  std::deque<SessionStep> flushes;
  std::deque<SessionStep> ends;
  std::map<std::uint8_t, std::deque<SessionStep>> routeResets;
  ocsd_datapath_resp_t defaultFlushResponse = OCSD_RESP_CONT;
  std::uint32_t pushCalls = 0U;
  std::uint32_t flushCalls = 0U;
  std::uint32_t endCalls = 0U;
  std::map<std::uint8_t, std::uint32_t> routeResetCalls;
  std::vector<std::uint8_t> routeResetOrder;
  std::vector<ocsd_trc_index_t> routeResetIndexes;
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

  /** @brief Applies the next scripted reset response for one decoder route. */
  ocsd_datapath_resp_t resetRoute(std::uint8_t channel, ocsd_trc_index_t index) override
  {
    ++m_script->routeResetCalls[channel];
    m_script->routeResetOrder.push_back(channel);
    m_script->routeResetIndexes.push_back(index);
    auto found = m_script->routeResets.find(channel);
    auto step = found == m_script->routeResets.end() ? SessionStep{} : take(found->second, SessionStep{});
    apply(step, index);
    return step.response;
  }

  /** @brief Applies the next scripted end-of-trace response. */
  ocsd_datapath_resp_t endOfTrace() override
  {
    ++m_script->endCalls;
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

  /** @brief Applies scripted callbacks in their declared observation order. */
  void applyObservation(const ScriptedObservation& observation)
  {
    std::visit(
        [this](const auto& item) {
          using Item = std::decay_t<decltype(item)>;
          if constexpr (std::is_same_v<Item, ScriptedError>) {
            if (item.channel.has_value()) {
              const ocsdError reported(item.severity, item.code, item.index, *item.channel, item.message);
              m_errors.LogError(0U, &reported);
            } else {
              const ocsdError reported(item.severity, item.code, item.index, item.message);
              m_errors.LogError(0U, &reported);
            }
          } else if constexpr (std::is_same_v<Item, ScriptedGenericCallback>) {
            static_cast<void>(m_collector.TraceElemIn(item.index, item.channel, item.element));
          } else {
            ItmTrcPacket packet;
            packet.setPktType(item.packetType);
            m_collector.rawPacketForRoute(item.route, item.operation, item.index, &packet, 0U, nullptr);
          }
        },
        observation);
  }

  /** @brief Applies legacy side effects and an optional ordered callback batch. */
  void apply(const SessionStep& step, ocsd_trc_index_t index)
  {
    for (const auto& error : step.errors) {
      applyObservation(error);
    }
    for (const auto& observation : step.observations) {
      applyObservation(observation);
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
      m_decoder(std::vector<TraceRouteIdentity>{TraceRouteIdentity{}}, OpenCsdItmInputMode::Single, m_sink,
                scriptedFactory(m_script))
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

#endif // CTRACE_TEST_UNIT_SUPPORT_OPENCSDSESSIONTESTSUPPORT_H
