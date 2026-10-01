/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "SvdItem.h"
#include "SvdCpu.h"
#include "SvdInterrupt.h"
#include "SvdSauRegion.h"
#include "ErrLog.h"
#include "XMLTree.h"

#include "gtest/gtest.h"
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

using namespace std;

namespace {

class SvdCpuModelTest : public testing::Test, public IErrConsumer {
protected:
  void SetUp() override {
    const auto log = ErrLog::Get();
    m_previousConsumer = log->SetErrConsumer(this);
    m_previousQuiet = log->IsQuietMode();
    m_previousErrors = log->GetErrCnt();
    m_previousWarnings = log->GetWarnCnt();
    log->SetQuietMode(false);
    log->ResetMsgCount();
  }

  void TearDown() override {
    const auto log = ErrLog::Get();
    log->SetErrConsumer(m_previousConsumer);
    log->SetQuietMode(m_previousQuiet);
    log->ResetMsgCount();
    for(int i = 0; i < m_previousErrors; i++) {
      log->IncErrCnt();
    }
    for(int i = 0; i < m_previousWarnings; i++) {
      log->IncWarnCnt();
    }
  }

  bool Consume(const PdscMsg& msg, const string&) override {
    m_messages.push_back(msg.GetMsgNum());
    return true;
  }

  static void AddRequiredFields(XMLTreeElement& element, const char* name = "CM33",
                                const char* revision = "r0p0", const char* endian = "little",
                                const char* priorityBits = "4") {
    element.CreateElement("name", name);
    element.CreateElement("revision", revision);
    element.CreateElement("endian", endian);
    element.CreateElement("nvicPrioBits", priorityBits);
  }

  static XMLTreeElement* AddSauConfig(XMLTreeElement& cpuElement, const char* count) {
    if(count) {
      cpuElement.CreateElement("sauNumRegions", count);
    }
    const auto config = cpuElement.CreateElement("sauRegionsConfig");
    config->AddAttribute("enabled", "true");
    config->AddAttribute("protectionWhenDisabled", "n");
    const auto region = config->CreateElement("region");
    region->AddAttribute("name", "Application");
    region->CreateElement("base", "0x10000000");
    region->CreateElement("limit", "0x10000FFF");
    region->CreateElement("access", "n");
    return config;
  }

  bool HasMessage(const string& id) const {
    return find(m_messages.begin(), m_messages.end(), id) != m_messages.end();
  }

  vector<string> m_messages;

private:
  IErrConsumer* m_previousConsumer = nullptr;
  bool m_previousQuiet = false;
  int m_previousErrors = 0;
  int m_previousWarnings = 0;
};

TEST_F(SvdCpuModelTest, PreservesParsedRevisionAndEndian) {
  XMLTreeElement element(nullptr, "cpu");
  AddRequiredFields(element, "CM33", "r2p3", "big");
  SvdCpu cpu(nullptr);
  ASSERT_TRUE(cpu.Construct(&element));

  EXPECT_EQ(0x0203U, cpu.GetRevision());
  EXPECT_EQ("r2p3", cpu.GetRevisionStr());
  EXPECT_EQ(SvdTypes::Endian::BIG, cpu.GetEndian());
  EXPECT_TRUE(m_messages.empty());
}

TEST_F(SvdCpuModelTest, InvalidRevisionReportsParseAndMissingRevisionDiagnostics) {
  XMLTreeElement element(nullptr, "cpu");
  AddRequiredFields(element, "CM33", "r256p0");
  SvdCpu cpu(nullptr);
  ASSERT_TRUE(cpu.Construct(&element));

  EXPECT_TRUE(HasMessage("M204"));
  EXPECT_TRUE(HasMessage("M325"));
}

TEST_F(SvdCpuModelTest, MissingRequiredMetadataReportsDefaults) {
  XMLTreeElement element(nullptr, "cpu");
  element.CreateElement("name", "CM33");
  SvdCpu cpu(nullptr);
  ASSERT_TRUE(cpu.Construct(&element));

  EXPECT_TRUE(HasMessage("M325"));
  EXPECT_TRUE(HasMessage("M326"));
  EXPECT_TRUE(HasMessage("M327"));
  EXPECT_EQ(SvdTypes::Endian::LITTLE, cpu.GetEndian());
  EXPECT_EQ(4U, cpu.GetNvicPrioBits());
}

TEST_F(SvdCpuModelTest, ChecksNvicPriorityWidthBoundaries) {
  for(const auto bits : {1U, 2U, 8U, 9U}) {
    SCOPED_TRACE(bits);
    m_messages.clear();
    XMLTreeElement element(nullptr, "cpu");
    const auto value = to_string(bits);
    AddRequiredFields(element, "CM33", "r0p0", "little", value.c_str());
    SvdCpu cpu(nullptr);
    ASSERT_TRUE(cpu.Construct(&element));

    const bool valid = bits == 2U || bits == 8U;
    EXPECT_EQ(!valid, HasMessage("M327"));
    EXPECT_EQ(valid ? bits : 4U, cpu.GetNvicPrioBits());
  }
}

TEST_F(SvdCpuModelTest, OptionalFeaturesRetainExplicitEnableAndDisableDeclarations) {
  XMLTreeElement element(nullptr, "cpu");
  AddRequiredFields(element, "CM55");
  element.CreateElement("mpuPresent", "true");
  element.CreateElement("fpuPresent", "false");
  element.CreateElement("fpuDP", "false");
  element.CreateElement("icachePresent", "true");
  element.CreateElement("dcachePresent", "false");
  element.CreateElement("itcmPresent", "true");
  element.CreateElement("dtcmPresent", "false");
  element.CreateElement("dspPresent", "true");
  element.CreateElement("mvePresent", "true");
  element.CreateElement("mveFP", "false");
  SvdCpu cpu(nullptr);
  ASSERT_TRUE(cpu.Construct(&element));

  EXPECT_TRUE(cpu.GetMpuPresent());
  EXPECT_FALSE(cpu.GetFpuPresent());
  EXPECT_FALSE(cpu.GetFpuDP());
  EXPECT_TRUE(cpu.GetIcachePresent());
  EXPECT_FALSE(cpu.GetDcachePresent());
  EXPECT_TRUE(cpu.GetItcmPresent());
  EXPECT_FALSE(cpu.GetDtcmPresent());
  EXPECT_TRUE(cpu.GetDspPresent());
  EXPECT_TRUE(cpu.GetMvePresent());
  EXPECT_FALSE(cpu.GetMveFP());
  // Explicit false values must still override generated CMSIS feature defaults.
  const auto& force = cpu.GetCmsisCfgForce();
  EXPECT_TRUE(force.bMpuPresent);
  EXPECT_TRUE(force.bFpuPresent);
  EXPECT_TRUE(force.bFpuDP);
  EXPECT_TRUE(force.bIcachePresent);
  EXPECT_TRUE(force.bDcachePresent);
  EXPECT_TRUE(force.bItcmPresent);
  EXPECT_TRUE(force.bDtcmPresent);
  EXPECT_TRUE(force.bDspPresent);
  EXPECT_TRUE(force.bMvePresent);
  EXPECT_TRUE(force.bMveFP);
  EXPECT_FALSE(force.bPmuPresent);
  EXPECT_TRUE(m_messages.empty());
}

TEST_F(SvdCpuModelTest, MalformedFeatureAndNumericDeclarationsReportParseErrors) {
  for(const auto tag : {"mpuPresent", "fpuPresent", "icachePresent", "dcachePresent", "itcmPresent",
                         "dtcmPresent", "dspPresent", "mvePresent", "mveFP", "deviceNumInterrupts",
                         "sauNumRegions", "pmuNumEventCnt"}) {
    SCOPED_TRACE(tag);
    m_messages.clear();
    XMLTreeElement element(nullptr, "cpu");
    AddRequiredFields(element, "CM55");
    element.CreateElement(tag, "invalid");
    SvdCpu cpu(nullptr);
    ASSERT_TRUE(cpu.Construct(&element));
    EXPECT_TRUE(HasMessage("M202"));
  }
}

TEST_F(SvdCpuModelTest, AcceptsPmuCounterCountsForSupportedCpu) {
  for(const auto count : {2U, 31U}) {
    SCOPED_TRACE(count);
    m_messages.clear();
    XMLTreeElement element(nullptr, "cpu");
    AddRequiredFields(element, "CM55");
    element.CreateElement("pmuPresent", "true");
    element.CreateElement("pmuNumEventCnt", to_string(count));
    SvdCpu cpu(nullptr);
    ASSERT_TRUE(cpu.Construct(&element));

    EXPECT_TRUE(cpu.GetPmuPresent());
    EXPECT_EQ(count, cpu.GetPmuNumEventCounters());
    EXPECT_TRUE(cpu.GetCmsisCfgForce().bPmuPresent);
    EXPECT_TRUE(m_messages.empty());
  }
}

TEST_F(SvdCpuModelTest, InvalidPmuCounterCountsDisablePmu) {
  for(const auto count : {0U, 1U, 33U}) {
    SCOPED_TRACE(count);
    m_messages.clear();
    XMLTreeElement element(nullptr, "cpu");
    AddRequiredFields(element, "CM55");
    element.CreateElement("pmuPresent", "true");
    element.CreateElement("pmuNumEventCnt", to_string(count));
    SvdCpu cpu(nullptr);
    ASSERT_TRUE(cpu.Construct(&element));

    EXPECT_FALSE(cpu.GetPmuPresent());
    EXPECT_TRUE(HasMessage("M384"));
    EXPECT_FALSE(HasMessage("M385"));
  }
}

TEST_F(SvdCpuModelTest, UnsupportedCpuDisablesPmuAndItsHeaderOverride) {
  XMLTreeElement element(nullptr, "cpu");
  AddRequiredFields(element, "CM3");
  element.CreateElement("pmuPresent", "true");
  element.CreateElement("pmuNumEventCnt", "2");
  SvdCpu cpu(nullptr);
  ASSERT_TRUE(cpu.Construct(&element));

  EXPECT_FALSE(cpu.GetPmuPresent());
  EXPECT_FALSE(cpu.GetCmsisCfgForce().bPmuPresent);
  EXPECT_TRUE(HasMessage("M385"));
  EXPECT_FALSE(HasMessage("M384"));
}

TEST_F(SvdCpuModelTest, PmuCounterCountRequiresPmuPresence) {
  XMLTreeElement element(nullptr, "cpu");
  AddRequiredFields(element, "CM55");
  element.CreateElement("pmuNumEventCnt", "2");
  SvdCpu cpu(nullptr);
  ASSERT_TRUE(cpu.Construct(&element));

  EXPECT_FALSE(cpu.GetPmuPresent());
  EXPECT_TRUE(HasMessage("M383"));
}

TEST_F(SvdCpuModelTest, VendorSystickConfigurationOmitsOnlySystick) {
  for(const bool vendorSystick : {false, true}) {
    SCOPED_TRACE(vendorSystick);
    XMLTreeElement element(nullptr, "cpu");
    AddRequiredFields(element);
    element.CreateElement("vendorSystickConfig", vendorSystick ? "true" : "false");
    SvdCpu cpu(nullptr);
    ASSERT_TRUE(cpu.Construct(&element));

    const auto& interrupts = cpu.GetInterruptList();
    EXPECT_EQ(vendorSystick ? 0U : 1U, interrupts.count(15U));
    ASSERT_EQ(1U, interrupts.count(14U));
    EXPECT_EQ("PendSV", interrupts.at(14U)->GetName());
    ASSERT_EQ(1U, interrupts.count(7U));
    EXPECT_EQ("SecureFault", interrupts.at(7U)->GetName());
    EXPECT_TRUE(m_messages.empty());
  }
}

TEST_F(SvdCpuModelTest, BaselineCpuOmitsMainlineFaultHandlers) {
  XMLTreeElement element(nullptr, "cpu");
  AddRequiredFields(element, "CM0");
  SvdCpu cpu(nullptr);
  ASSERT_TRUE(cpu.Construct(&element));

  const auto& interrupts = cpu.GetInterruptList();
  ASSERT_EQ(1U, interrupts.count(3U));
  EXPECT_EQ("HardFault", interrupts.at(3U)->GetName());
  for(const auto number : {4U, 5U, 6U, 7U, 12U}) {
    EXPECT_EQ(0U, interrupts.count(number)) << number;
  }
  EXPECT_EQ(1U, interrupts.count(15U));
  EXPECT_TRUE(m_messages.empty());
}

TEST_F(SvdCpuModelTest, ParsesSauRegionAttributesAddressesAndAccess) {
  XMLTreeElement element(nullptr, "cpu");
  AddRequiredFields(element);
  const auto configElement = AddSauConfig(element, "2");
  const auto disabledRegion = configElement->CreateElement("region");
  disabledRegion->AddAttribute("name", "Gateway");
  disabledRegion->AddAttribute("enabled", "false");
  disabledRegion->CreateElement("base", "0x20000000");
  disabledRegion->CreateElement("limit", "0x20001FFF");
  disabledRegion->CreateElement("access", "c");
  SvdCpu cpu(nullptr);
  ASSERT_TRUE(cpu.Construct(&element));

  const auto config = cpu.GetSauRegionsConfig();
  ASSERT_NE(nullptr, config);
  EXPECT_TRUE(config->IsValid());
  EXPECT_TRUE(config->GetEnabled());
  EXPECT_EQ(SvdTypes::ProtectionType::NONSECURE, config->GetProtectionWhenDisabled());
  EXPECT_TRUE(cpu.GetCmsisCfgForce().bSauPresent);
  const auto& children = config->GetChildren();
  ASSERT_EQ(2U, children.size());
  const auto application = dynamic_cast<SvdSauRegion*>(children.front());
  const auto gateway = dynamic_cast<SvdSauRegion*>(children.back());
  ASSERT_NE(nullptr, application);
  ASSERT_NE(nullptr, gateway);
  EXPECT_TRUE(application->GetEnabled());
  EXPECT_EQ("Application", application->GetName());
  EXPECT_EQ(0x10000000U, application->GetBase());
  EXPECT_EQ(0x10000FFFU, application->GetLimit());
  EXPECT_EQ(SvdTypes::SauAccessType::NONSECURE, application->GetAccessType());
  EXPECT_FALSE(gateway->GetEnabled());
  EXPECT_EQ("Gateway", gateway->GetName());
  EXPECT_EQ(0x20000000U, gateway->GetBase());
  EXPECT_EQ(0x20001FFFU, gateway->GetLimit());
  EXPECT_EQ(SvdTypes::SauAccessType::SECURE, gateway->GetAccessType());
  EXPECT_TRUE(m_messages.empty());
}

TEST_F(SvdCpuModelTest, SauConfigurationRequiresRegionCount) {
  XMLTreeElement element(nullptr, "cpu");
  AddRequiredFields(element);
  AddSauConfig(element, nullptr);
  SvdCpu cpu(nullptr);
  ASSERT_TRUE(cpu.Construct(&element));

  ASSERT_NE(nullptr, cpu.GetSauRegionsConfig());
  EXPECT_FALSE(cpu.GetSauRegionsConfig()->IsValid());
  EXPECT_TRUE(HasMessage("M363"));
}

TEST_F(SvdCpuModelTest, SauConfigurationWithZeroRegionsIsInvalid) {
  XMLTreeElement element(nullptr, "cpu");
  AddRequiredFields(element);
  AddSauConfig(element, "0");
  SvdCpu cpu(nullptr);
  ASSERT_TRUE(cpu.Construct(&element));

  ASSERT_NE(nullptr, cpu.GetSauRegionsConfig());
  EXPECT_FALSE(cpu.GetSauRegionsConfig()->IsValid());
  EXPECT_TRUE(HasMessage("M387"));
}

TEST_F(SvdCpuModelTest, SauCountOverflowInvalidatesDependentConfiguration) {
  XMLTreeElement element(nullptr, "cpu");
  AddRequiredFields(element);
  AddSauConfig(element, "256");
  SvdCpu cpu(nullptr);
  ASSERT_TRUE(cpu.Construct(&element));

  ASSERT_NE(nullptr, cpu.GetSauRegionsConfig());
  EXPECT_FALSE(cpu.GetSauRegionsConfig()->IsValid());
  EXPECT_EQ(SvdItem::VALUE32_NOT_INIT, cpu.GetSauNumRegions());
  EXPECT_TRUE(HasMessage("M364"));
}

TEST_F(SvdCpuModelTest, ExcessSauRegionDefinitionsAreDiagnosed) {
  XMLTreeElement element(nullptr, "cpu");
  AddRequiredFields(element);
  const auto config = AddSauConfig(element, "1");
  const auto secondRegion = config->CreateElement("region");
  secondRegion->CreateElement("base", "0x20000000");
  secondRegion->CreateElement("limit", "0x20000FFF");
  secondRegion->CreateElement("access", "n");
  SvdCpu cpu(nullptr);
  ASSERT_TRUE(cpu.Construct(&element));

  EXPECT_TRUE(HasMessage("M391"));
}

} // namespace
