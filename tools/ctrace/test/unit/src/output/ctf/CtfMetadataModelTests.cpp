/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CtfTestSupport.h"
#include "TestSupport.h"

#include <gtest/gtest.h>

#include "ctf/CtfMetadataModel.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>

/** @brief Creates one formatted ITM stream descriptor for model tests. */
static CtfStreamDescriptor modelStream(std::uint32_t routeId, std::uint8_t traceBusId, std::uint32_t clockDomainId)
{
  return {
      CtfStreamClassId{traceBusId}, {TraceRouteId{routeId}, traceBusId}, std::nullopt,
      CtfClockDomainId{clockDomainId},
  };
}

/** @brief Creates one clock-domain descriptor with a deterministic UUID. */
static CtfClockDomainDescriptor modelClock(std::uint32_t id, std::string name, std::uint8_t uuidDiscriminator,
                                           std::uint64_t frequencyHz = 1000000U)
{
  return {
      CtfClockDomainId{id}, std::move(name), CtfTestSupport::testUuid(uuidDiscriminator), frequencyHz, false,
  };
}

/** @brief Creates a valid two-route topology with independent equal-frequency domains. */
static CtfMetadataTopology independentModelTopology()
{
  const TraceRouteIdentity first{TraceRouteId{5U}, 1U};
  const TraceRouteIdentity second{TraceRouteId{70U}, 111U};
  return {
      {modelClock(9U, "clock_nine", 9U), modelClock(2U, "clock_two", 2U)},
      {modelStream(70U, 111U, 9U), modelStream(5U, 1U, 2U)},
      {
          {"dwt", 0U, second, std::string("second"), 0x2000U, "signed", 2U},
          {"dwt", 0U, first, std::string("first"), 0x1000U, "unsigned", 4U},
          {"itm", 1U, second, std::string("console"), std::nullopt, "unsigned", 4U},
          {"itm", 1U, first, std::string("console"), std::nullopt, "unsigned", 4U},
      },
  };
}

TEST(CtraceUnitTests, testCtfMetadataModelKeepsRouteClockAndSourceIdentityIndependent)
{
  CtfMetadataModel model(CtfTestSupport::testUuid(), independentModelTopology());
  const auto& topology = model.topology();

  ASSERT_EQ(topology.clockDomains.size(), 2U);
  EXPECT_EQ(topology.clockDomains[0].id, CtfClockDomainId{2U});
  EXPECT_EQ(topology.clockDomains[1].id, CtfClockDomainId{9U});
  ASSERT_EQ(topology.streams.size(), 2U);
  EXPECT_EQ(topology.streams[0].streamClassId, CtfStreamClassId{1U});
  EXPECT_EQ(topology.streams[1].streamClassId, CtfStreamClassId{111U});
  EXPECT_EQ(topology.clockDomains[0].frequencyHz, topology.clockDomains[1].frequencyHz)
      << "equal frequencies must not merge independent clock identities";
  EXPECT_NE(topology.clockDomains[0].uuid, topology.clockDomains[1].uuid);

  const TraceRouteIdentity first{TraceRouteId{5U}, 1U};
  const TraceRouteIdentity second{TraceRouteId{70U}, 111U};
  ASSERT_NE(model.streamForRoute(first), nullptr);
  ASSERT_NE(model.streamForRoute(second), nullptr);
  EXPECT_EQ(model.streamForRoute(first)->clockDomainId, CtfClockDomainId{2U});
  EXPECT_EQ(model.clockDomain(CtfClockDomainId{9U})->name, "clock_nine");
  EXPECT_EQ(model.streamForRoute({TraceRouteId{5U}, 2U}), nullptr);
  ASSERT_NE(model.source(first, "dwt", 0U), nullptr);
  ASSERT_NE(model.source(second, "dwt", 0U), nullptr);
  EXPECT_EQ(model.source(first, "dwt", 0U)->dataType, "unsigned");
  EXPECT_EQ(model.source(second, "dwt", 0U)->dataType, "signed");
  EXPECT_EQ(model.source(first, "dwt", 1U), nullptr);

  model.observeException(CtfStreamClassId{111U}, 54U);
  model.observeException(CtfStreamClassId{111U}, 16U);
  model.observeException(CtfStreamClassId{111U}, 54U);
  EXPECT_EQ(model.observedExceptions(CtfStreamClassId{111U}), (std::vector<ExceptionNumber>{16U, 54U}));
  EXPECT_TRUE(model.observedExceptions(CtfStreamClassId{1U}).empty());
  EXPECT_THROW(model.observeException(CtfStreamClassId{7U}, 1U), std::runtime_error);
  EXPECT_FALSE(model.isLegacySingleStreamLayout());
}

TEST(CtraceUnitTests, testCtfMetadataModelProjectsRuntimeObservationsToEmittedStreams)
{
  CtfMetadataModel model(CtfTestSupport::testUuid(), independentModelTopology());
  EXPECT_FALSE(model.observedGraphicalTopic(CtfStreamClassId{1U}, CtfGraphicalTopic::DwtValue));
  model.observeGraphicalTopic(CtfStreamClassId{1U}, CtfGraphicalTopic::DwtValue);
  model.observeGraphicalTopic(CtfStreamClassId{1U}, CtfGraphicalTopic::ProcessorState);
  model.observeGraphicalTopic(CtfStreamClassId{1U}, CtfGraphicalTopic::DwtValue);
  model.observeException(CtfStreamClassId{111U}, 16U);
  EXPECT_TRUE(model.observedGraphicalTopic(CtfStreamClassId{1U}, CtfGraphicalTopic::DwtValue));
  EXPECT_TRUE(model.observedGraphicalTopic(CtfStreamClassId{1U}, CtfGraphicalTopic::ProcessorState));
  EXPECT_FALSE(model.observedGraphicalTopic(CtfStreamClassId{111U}, CtfGraphicalTopic::DwtValue));
  EXPECT_FALSE(model.observedGraphicalTopic(CtfStreamClassId{1U}, CtfGraphicalTopic::DwtAddress));
  EXPECT_THROW(model.observeGraphicalTopic(CtfStreamClassId{7U}, CtfGraphicalTopic::DwtValue), std::runtime_error);

  const auto projected = model.projectToEmittedStreams({CtfStreamClassId{1U}});
  EXPECT_EQ(projected.topology().clockDomains.size(), 1U);
  ASSERT_EQ(projected.topology().streams.size(), 1U);
  EXPECT_EQ(projected.topology().sources.size(), 2U);
  EXPECT_EQ(projected.topology().streams.front().streamClassId, CtfStreamClassId{1U});
  EXPECT_TRUE(projected.observedExceptions(CtfStreamClassId{111U}).empty());
  EXPECT_TRUE(projected.observedGraphicalTopic(CtfStreamClassId{1U}, CtfGraphicalTopic::DwtValue));

  const auto unknownProjection = model.projectToEmittedStreams({CtfStreamClassId{7U}});
  EXPECT_TRUE(unknownProjection.topology().clockDomains.empty());
  EXPECT_TRUE(unknownProjection.topology().streams.empty());
  EXPECT_TRUE(unknownProjection.topology().sources.empty());
}

TEST(CtraceUnitTests, testCtfMetadataModelAcceptsSharedDomainsAndItmRouteBoundaries)
{
  CtfMetadataTopology shared{
      {modelClock(42U, "shared_clock", 42U, 240000000U)},
      {modelStream(8U, 1U, 42U), modelStream(99U, 111U, 42U)},
      {},
  };
  EXPECT_NO_THROW((void)CtfMetadataModel(CtfTestSupport::testUuid(), std::move(shared)));

  for (const auto traceBusId : {std::uint8_t{1U}, std::uint8_t{111U}}) {
    CtfMetadataTopology boundary{
        {modelClock(1U, "boundary_clock", 1U)},
        {modelStream(3U, traceBusId, 1U)},
        {},
    };
    EXPECT_NO_THROW((void)CtfMetadataModel(CtfTestSupport::testUuid(), std::move(boundary)));
  }

  CtfMetadataTopology unformatted{
      {modelClock(4U, "unformatted_clock", 4U)},
      {{CtfStreamClassId{0U},
        {TraceRouteId{88U}, std::nullopt},
        std::nullopt,
        CtfClockDomainId{4U}}},
      {},
  };
  CtfMetadataModel unformattedModel(CtfTestSupport::testUuid(), std::move(unformatted));
  EXPECT_FALSE(unformattedModel.isLegacySingleStreamLayout());

  CtfMetadataModel legacy(CtfTestSupport::testUuid(),
                          CtfTestSupport::legacyTopology(1000000U, {TraceRouteId{88U}, std::nullopt}));
  EXPECT_TRUE(legacy.isLegacySingleStreamLayout());

  auto identifiedLegacy = CtfTestSupport::legacyTopology(1000000U);
  identifiedLegacy.clockDomains.front().uuid = CtfTestSupport::testUuid(1U);
  EXPECT_TRUE(CtfMetadataModel(CtfTestSupport::testUuid(), std::move(identifiedLegacy)).isLegacySingleStreamLayout());

  CtfMetadataModel empty(CtfTestSupport::testUuid(), {});
  EXPECT_TRUE(empty.topology().clockDomains.empty());
  EXPECT_FALSE(empty.isLegacySingleStreamLayout());
}

TEST(CtraceUnitTests, testCtfMetadataModelRejectsInvalidClockDomains)
{
  const auto expectRejected = [](CtfMetadataTopology topology, const std::string& message) {
    EXPECT_TRUE(throwsWithMessage<std::invalid_argument>(
        [&] { (void)CtfMetadataModel(CtfTestSupport::testUuid(), std::move(topology)); }, message));
  };

  auto duplicateId = independentModelTopology();
  duplicateId.clockDomains.push_back(modelClock(2U, "other_clock", 3U));
  expectRejected(std::move(duplicateId), "duplicate clock-domain ID");

  auto duplicateName = independentModelTopology();
  duplicateName.clockDomains.push_back(modelClock(3U, "clock_two", 3U));
  expectRejected(std::move(duplicateName), "unique valid clock-domain names");

  auto invalidName = independentModelTopology();
  invalidName.clockDomains.front().name = "9-invalid";
  expectRejected(std::move(invalidName), "unique valid clock-domain names");

  auto zeroFrequency = independentModelTopology();
  zeroFrequency.clockDomains.front().frequencyHz = 0U;
  expectRejected(std::move(zeroFrequency), "non-zero clock-domain frequency");

  auto traceUuidCollision = independentModelTopology();
  traceUuidCollision.clockDomains.front().uuid = CtfTestSupport::testUuid();
  expectRejected(std::move(traceUuidCollision), "distinct from the trace");

  auto clockUuidCollision = independentModelTopology();
  clockUuidCollision.clockDomains.front().uuid = clockUuidCollision.clockDomains.back().uuid;
  expectRejected(std::move(clockUuidCollision), "distinct from the trace");

  auto missingClockUuid = independentModelTopology();
  missingClockUuid.clockDomains.front().uuid.reset();
  expectRejected(std::move(missingClockUuid), "require an explicit UUID");

  auto unknownClock = independentModelTopology();
  unknownClock.streams.front().clockDomainId = CtfClockDomainId{77U};
  expectRejected(std::move(unknownClock), "unknown clock domain");

  auto orphanClock = independentModelTopology();
  orphanClock.clockDomains.push_back(modelClock(77U, "orphan_clock", 77U));
  expectRejected(std::move(orphanClock), "without a referencing stream class");
}

TEST(CtraceUnitTests, testCtfMetadataModelRejectsInvalidStreamAndRouteIdentities)
{
  const auto expectRejected = [](CtfMetadataTopology topology, const std::string& message) {
    EXPECT_TRUE(throwsWithMessage<std::invalid_argument>(
        [&] { (void)CtfMetadataModel(CtfTestSupport::testUuid(), std::move(topology)); }, message));
  };

  for (const auto invalid : {std::uint8_t{0U}, std::uint8_t{112U}, std::uint8_t{127U}}) {
    auto topology = independentModelTopology();
    topology.streams.front().route.traceBusId = invalid;
    topology.streams.front().streamClassId = CtfStreamClassId{invalid};
    expectRejected(std::move(topology), "ATB trace ID between 1 and 111");
  }

  auto classMismatch = independentModelTopology();
  classMismatch.streams.front().streamClassId = CtfStreamClassId{17U};
  expectRejected(std::move(classMismatch), "does not match its normalized route identity");

  auto duplicateClass = independentModelTopology();
  duplicateClass.streams.back().route.traceBusId = duplicateClass.streams.front().route.traceBusId;
  duplicateClass.streams.back().streamClassId = duplicateClass.streams.front().streamClassId;
  expectRejected(std::move(duplicateClass), "duplicate stream-class ID");

  auto duplicateRoute = independentModelTopology();
  duplicateRoute.streams.back().route = duplicateRoute.streams.front().route;
  duplicateRoute.streams.back().streamClassId = duplicateRoute.streams.front().streamClassId;
  expectRejected(std::move(duplicateRoute), "duplicate normalized route");

  auto inconsistentRoute = independentModelTopology();
  inconsistentRoute.streams.back().route.id = inconsistentRoute.streams.front().route.id;
  expectRejected(std::move(inconsistentRoute), "inconsistent normalized route identities");
}

TEST(CtraceUnitTests, testCtfMetadataModelKeysSourcesByExactRouteTypeAndNumber)
{
  EXPECT_NO_THROW((void)CtfMetadataModel(CtfTestSupport::testUuid(), independentModelTopology()));

  const auto expectRejected = [](CtfMetadataTopology topology, const std::string& message) {
    EXPECT_TRUE(throwsWithMessage<std::invalid_argument>(
        [&] { (void)CtfMetadataModel(CtfTestSupport::testUuid(), std::move(topology)); }, message));
  };

  auto duplicate = independentModelTopology();
  duplicate.sources.push_back(duplicate.sources.front());
  expectRejected(std::move(duplicate), "duplicate source metadata");

  auto conflicting = independentModelTopology();
  auto conflict = conflicting.sources.front();
  conflict.label = "conflicting label";
  conflicting.sources.push_back(std::move(conflict));
  expectRejected(std::move(conflicting), "conflicting source metadata for one route");

  auto unknownRoute = independentModelTopology();
  unknownRoute.sources.front().route = {TraceRouteId{999U}, 1U};
  expectRejected(std::move(unknownRoute), "unknown normalized route");

  auto unsupportedType = independentModelTopology();
  unsupportedType.sources.front().type = "future";
  expectRejected(std::move(unsupportedType), "type must be 'itm' or 'dwt'");

  for (const auto invalidChannel : {0U, 32U}) {
    auto invalidItm = independentModelTopology();
    const auto itm = std::find_if(invalidItm.sources.begin(), invalidItm.sources.end(),
                                  [](const CtfSourceDescriptor& source) { return source.type == "itm"; });
    itm->source = invalidChannel;
    expectRejected(std::move(invalidItm), "channel between 1 and 31");
  }

  auto invalidComparator = independentModelTopology();
  invalidComparator.sources.front().source = 4U;
  expectRejected(std::move(invalidComparator), "comparator between 0 and 3");

  for (const auto& invalidMetadata :
       {std::pair<std::string, std::uint8_t>{"future", 4U}, std::pair<std::string, std::uint8_t>{"unsigned", 3U},
        std::pair<std::string, std::uint8_t>{"float", 2U}}) {
    auto invalidValue = independentModelTopology();
    invalidValue.sources.front().dataType = invalidMetadata.first;
    invalidValue.sources.front().dataSize = invalidMetadata.second;
    expectRejected(std::move(invalidValue), "invalid data-type/size combination");
  }

  auto overflowingAddress = independentModelTopology();
  overflowingAddress.sources.front().address = std::numeric_limits<std::uint64_t>::max();
  overflowingAddress.sources.front().dataSize = 4U;
  expectRejected(std::move(overflowingAddress), "address range exceeds");

  auto boundaryAddress = independentModelTopology();
  boundaryAddress.sources.front().address = std::numeric_limits<std::uint64_t>::max() - 3U;
  boundaryAddress.sources.front().dataSize = 4U;
  EXPECT_NO_THROW((void)CtfMetadataModel(CtfTestSupport::testUuid(), std::move(boundaryAddress)));

  auto irrelevantItmFields = independentModelTopology();
  const auto itm = std::find_if(irrelevantItmFields.sources.begin(), irrelevantItmFields.sources.end(),
                                [](const CtfSourceDescriptor& source) { return source.type == "itm"; });
  itm->dataType = "ignored";
  itm->dataSize = 0U;
  itm->address = std::numeric_limits<std::uint64_t>::max();
  EXPECT_NO_THROW((void)CtfMetadataModel(CtfTestSupport::testUuid(), std::move(irrelevantItmFields)));
}

TEST(CtraceUnitTests, testCtfUuidFormattingAndGeneration)
{
  EXPECT_EQ(CtfTestSupport::testUuid(0xabU).toString(), "10ab2233-4455-4677-8899-aabbccddeeff");
  const auto generated = CtfUuid::randomV4();
  EXPECT_EQ(generated.bytes()[6U] & 0xf0U, 0x40U);
  EXPECT_EQ(generated.bytes()[8U] & 0xc0U, 0x80U);
  EXPECT_EQ(generated.toString().size(), 36U);
}
