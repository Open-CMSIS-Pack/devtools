/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "SvdItem.h"
#include "SvdCpu.h"
#include "SvdDevice.h"
#include "SvdInterrupt.h"
#include "ErrLog.h"
#include "XMLTree.h"

#include "gtest/gtest.h"
#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using namespace std;

namespace {

struct InterruptLimit {
  const char* name;
  SvdTypes::CpuType cpuType;
  uint32_t limit;
};

class SvdInterruptDiagnosticTest : public testing::Test, public IErrConsumer {
protected:
  void SetUp() override {
    const auto log = ErrLog::Get();
    m_previousConsumer = log->SetErrConsumer(this);
    log->ResetMsgCount();
  }

  void TearDown() override {
    const auto log = ErrLog::Get();
    log->SetErrConsumer(m_previousConsumer);
    log->ResetMsgCount();
  }

  bool Consume(const PdscMsg& msg, const string&) override {
    m_messages.push_back(msg.GetMsgNum());
    return true;
  }

  void ConfigureCpu(const char* name) {
    XMLTreeElement cpuElement(nullptr, "cpu");
    cpuElement.CreateElement("name", name);
    cpuElement.CreateElement("revision", "r0p0");
    cpuElement.CreateElement("endian", "little");
    cpuElement.CreateElement("nvicPrioBits", "4");
    ASSERT_TRUE(m_device.ProcessXmlElement(&cpuElement));
    ASSERT_NE(nullptr, m_device.GetCpu());
  }

  void SetDeviceLimit(uint32_t limit) {
    XMLTreeElement element(nullptr, "deviceNumInterrupts");
    element.SetText(to_string(limit));
    ASSERT_TRUE(m_device.GetCpu()->ProcessXmlElement(&element));
    ASSERT_EQ(limit, m_device.GetCpu()->GetDeviceNumInterrupts());
  }

  bool CheckInterrupt(optional<uint32_t> value = nullopt) {
    SvdInterrupt interrupt(&m_device);
    interrupt.SetName("TEST");
    interrupt.SetDescription("Test peripheral interrupt.");
    if(value) {
      interrupt.SetValue(*value);
    }
    // Check the individual number, independently of the device's sparse-interrupt-list warning.
    interrupt.CheckItem();
    return interrupt.IsValid();
  }

  bool HasMessage(const string& id) const {
    return find(m_messages.begin(), m_messages.end(), id) != m_messages.end();
  }

  SvdDevice m_device{nullptr};
  vector<string> m_messages;
  IErrConsumer* m_previousConsumer = nullptr;
};

class SvdInterruptTest : public SvdInterruptDiagnosticTest, public testing::WithParamInterface<InterruptLimit> {
protected:
  void SetUp() override {
    SvdInterruptDiagnosticTest::SetUp();
    ConfigureCpu(GetParam().name);
    ASSERT_NE(nullptr, m_device.GetCpu());
    ASSERT_TRUE(m_device.GetCpu()->IsValid());
    ASSERT_EQ(GetParam().cpuType, m_device.GetCpu()->GetType());
    ASSERT_EQ(GetParam().limit, SvdTypes::GetCpuFeatures(GetParam().cpuType).NUMEXTIRQ);
    ASSERT_TRUE(m_messages.empty());
  }
};

TEST_P(SvdInterruptTest, AcceptsLastSupportedInterrupt) {
  EXPECT_TRUE(CheckInterrupt(GetParam().limit - 1U));
  EXPECT_TRUE(m_messages.empty());
}

TEST_P(SvdInterruptTest, RejectsFirstUnsupportedInterrupt) {
  EXPECT_FALSE(CheckInterrupt(GetParam().limit));
  EXPECT_TRUE(HasMessage("M331"));
  EXPECT_FALSE(HasMessage("M381"));
  EXPECT_FALSE(HasMessage("M389"));
}

TEST_P(SvdInterruptTest, AcceptsDeviceLimitAtArchitectureBoundary) {
  SetDeviceLimit(GetParam().limit);
  EXPECT_TRUE(CheckInterrupt(GetParam().limit - 1U));
  EXPECT_TRUE(m_messages.empty());
}

TEST_P(SvdInterruptTest, RejectsInterruptAtDeviceLimit) {
  SetDeviceLimit(GetParam().limit - 1U);
  EXPECT_FALSE(CheckInterrupt(GetParam().limit - 1U));
  EXPECT_TRUE(HasMessage("M381"));
  EXPECT_FALSE(HasMessage("M331"));
  EXPECT_FALSE(HasMessage("M389"));
}

TEST_P(SvdInterruptTest, RejectsDeviceLimitAboveArchitectureLimit) {
  SetDeviceLimit(GetParam().limit + 1U);
  EXPECT_FALSE(CheckInterrupt(0U));
  EXPECT_TRUE(HasMessage("M389"));
  EXPECT_FALSE(HasMessage("M331"));
  EXPECT_FALSE(HasMessage("M381"));
}

INSTANTIATE_TEST_SUITE_P(CpuInterruptLimits, SvdInterruptTest,
  testing::Values(
    InterruptLimit{"CA5", SvdTypes::CpuType::CA5, 256U},
    InterruptLimit{"CA7", SvdTypes::CpuType::CA7, 512U},
    InterruptLimit{"CA9", SvdTypes::CpuType::CA9, 256U},
    InterruptLimit{"CA15", SvdTypes::CpuType::CA15, 256U},
    InterruptLimit{"CM3", SvdTypes::CpuType::CM3, 240U},
    InterruptLimit{"CM33", SvdTypes::CpuType::CM33, 480U}),
  [](const testing::TestParamInfo<InterruptLimit>& info) { return info.param.name; });

class SvdNoCpuInterruptLimitTest : public SvdInterruptTest {};

TEST_P(SvdNoCpuInterruptLimitTest, AcceptsInterruptsWithoutCpuLimit) {
  for(const auto value : {0U, 249U, 1019U, 8192U}) {
    SCOPED_TRACE(value);
    EXPECT_TRUE(CheckInterrupt(value));
    EXPECT_TRUE(m_messages.empty());
  }
}

TEST_P(SvdNoCpuInterruptLimitTest, AcceptsDeviceLimitWithoutCpuMaximum) {
  SetDeviceLimit(8193U);
  EXPECT_TRUE(CheckInterrupt(8192U));
  EXPECT_TRUE(m_messages.empty());
}

TEST_P(SvdNoCpuInterruptLimitTest, EnforcesExplicitDeviceLimit) {
  SetDeviceLimit(249U);
  EXPECT_TRUE(CheckInterrupt(248U));
  EXPECT_TRUE(m_messages.empty());
  EXPECT_FALSE(CheckInterrupt(249U));
  EXPECT_TRUE(HasMessage("M381"));
  EXPECT_FALSE(HasMessage("M331"));
  EXPECT_FALSE(HasMessage("M389"));
}

TEST_P(SvdNoCpuInterruptLimitTest, RejectsMissingInterruptValue) {
  EXPECT_FALSE(CheckInterrupt());
  EXPECT_TRUE(HasMessage("M330"));
  EXPECT_FALSE(HasMessage("M331"));
  EXPECT_FALSE(HasMessage("M381"));
  EXPECT_FALSE(HasMessage("M389"));
}

INSTANTIATE_TEST_SUITE_P(NoCpuInterruptLimit, SvdNoCpuInterruptLimitTest,
  testing::Values(
    InterruptLimit{"CA8", SvdTypes::CpuType::CA8, CpuFeature::NO_CPU_INTERRUPT_LIMIT},
    InterruptLimit{"CA17", SvdTypes::CpuType::CA17, CpuFeature::NO_CPU_INTERRUPT_LIMIT},
    InterruptLimit{"CA53", SvdTypes::CpuType::CA53, CpuFeature::NO_CPU_INTERRUPT_LIMIT},
    InterruptLimit{"CA55", SvdTypes::CpuType::CA55, CpuFeature::NO_CPU_INTERRUPT_LIMIT},
    InterruptLimit{"CA57", SvdTypes::CpuType::CA57, CpuFeature::NO_CPU_INTERRUPT_LIMIT},
    InterruptLimit{"CA72", SvdTypes::CpuType::CA72, CpuFeature::NO_CPU_INTERRUPT_LIMIT},
    InterruptLimit{"OTHER", SvdTypes::CpuType::OTHER, CpuFeature::NO_CPU_INTERRUPT_LIMIT}),
  [](const testing::TestParamInfo<InterruptLimit>& info) { return info.param.name; });

TEST(SvdCpuTypeTest, CortexA55Names) {
  EXPECT_EQ("CA55", SvdTypes::GetCpuType(SvdTypes::CpuType::CA55));
  EXPECT_EQ("ARM Cortex-A55", SvdTypes::GetCpuName(SvdTypes::CpuType::CA55));
}

TEST_F(SvdInterruptDiagnosticTest, MissingCpuPreservesFallbackBoundary) {
  EXPECT_TRUE(CheckInterrupt(479U));
  EXPECT_TRUE(HasMessage("M390"));
  EXPECT_FALSE(HasMessage("M331"));

  m_messages.clear();
  EXPECT_FALSE(CheckInterrupt(480U));
  EXPECT_TRUE(HasMessage("M390"));
  EXPECT_TRUE(HasMessage("M331"));
}

TEST_F(SvdInterruptDiagnosticTest, UnknownCpuPreservesCortexM3Fallback) {
  ConfigureCpu("UNRECOGNIZED_CPU");
  ASSERT_NE(nullptr, m_device.GetCpu());
  EXPECT_FALSE(m_device.GetCpu()->IsValid());
  EXPECT_EQ(SvdTypes::CpuType::CM3, m_device.GetCpu()->GetType());
  EXPECT_TRUE(HasMessage("M202"));
  EXPECT_TRUE(HasMessage("M329"));

  m_messages.clear();
  EXPECT_TRUE(CheckInterrupt(239U));
  EXPECT_TRUE(m_messages.empty());
  EXPECT_FALSE(CheckInterrupt(240U));
  EXPECT_TRUE(HasMessage("M331"));
  EXPECT_FALSE(HasMessage("M390"));
}

} // namespace
