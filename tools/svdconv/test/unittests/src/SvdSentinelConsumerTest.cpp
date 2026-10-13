/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "SvdItem.h"
#include "SvdAddressBlock.h"
#include "SvdCpu.h"
#include "SvdDevice.h"
#include "SvdDimension.h"
#include "SvdPeripheral.h"
#include "SvdRegister.h"
#include "HeaderData.h"
#include "PartitionData.h"
#include "ErrLog.h"
#include "XMLTree.h"

#include "gtest/gtest.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

namespace {

class SvdSentinelConsumerTest : public testing::Test, public IErrConsumer {
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
    if(!m_outputDirectory.empty()) {
      error_code ec;
      for(const auto& name : {"input.svd", "device.h", "partition.h"}) {
        filesystem::remove(m_outputDirectory / name, ec);
      }
      filesystem::remove(m_outputDirectory, ec);
    }
  }

  bool Consume(const PdscMsg& msg, const string&) override {
    m_messages.push_back(msg.GetMsgNum());
    if(msg.GetMsgNum() == "M344") {
      m_addressBlockDiagnostic = msg.GetSubstitute("TEXT");
    }
    return true;
  }

  bool HasMessage(const string& id) const {
    return find(m_messages.begin(), m_messages.end(), id) != m_messages.end();
  }

  static void ConfigureCpu(SvdDevice& device, const char* regionCount) {
    XMLTreeElement cpuElement(nullptr, "cpu");
    cpuElement.CreateElement("name", "CM33");
    cpuElement.CreateElement("revision", "r0p0");
    cpuElement.CreateElement("endian", "little");
    cpuElement.CreateElement("nvicPrioBits", "4");
    if(regionCount) {
      cpuElement.CreateElement("sauNumRegions", regionCount);
    }
    ASSERT_TRUE(device.ProcessXmlElement(&cpuElement));
    ASSERT_NE(nullptr, device.GetCpu());
  }

  void CreateOutputDirectory() {
    const auto nonce = chrono::steady_clock::now().time_since_epoch().count();
    for(unsigned i = 0; i < 100; i++) {
      const auto candidate = filesystem::temp_directory_path() /
        ("svdconv-sentinel-" + to_string(nonce) + "-" + to_string(i));
      if(filesystem::create_directory(candidate)) {
        m_outputDirectory = candidate;
        break;
      }
    }
    ASSERT_FALSE(m_outputDirectory.empty());
    ofstream input(m_outputDirectory / "input.svd");
    ASSERT_TRUE(input.is_open());
    input << "<device/>\n";
  }

  string ReadOutput(const char* name) const {
    ifstream input(m_outputDirectory / name);
    EXPECT_TRUE(input.is_open());
    return string(istreambuf_iterator<char>(input), istreambuf_iterator<char>());
  }

  static string DefineValue(const string& text, const string& name) {
    istringstream lines(text);
    string line;
    while(getline(lines, line)) {
      istringstream tokens(line);
      string directive, symbol, value;
      if(tokens >> directive >> symbol >> value && directive == "#define" && symbol == name) {
        return value;
      }
    }
    return {};
  }

  vector<string> m_messages;
  string m_addressBlockDiagnostic;
  filesystem::path m_outputDirectory;

private:
  IErrConsumer* m_previousConsumer = nullptr;
  bool m_previousQuiet = false;
  int m_previousErrors = 0;
  int m_previousWarnings = 0;
};

TEST_F(SvdSentinelConsumerTest, DimensionCopyInheritsOnlyUnsetCountAndIncrement) {
  const auto unset = numeric_limits<uint32_t>::max();
  SvdDimension source(nullptr);
  source.SetDim(4);
  source.SetDimIncrement(8);
  for(const auto count : {unset, 0U, 1U, unset - 1U}) {
    for(const auto increment : {unset, 0U, 1U, unset - 1U}) {
      SCOPED_TRACE(count);
      SCOPED_TRACE(increment);
      SvdDimension destination(nullptr);
      destination.SetDim(count);
      destination.SetDimIncrement(increment);
      ASSERT_TRUE(destination.CopyItem(&source));
      EXPECT_EQ(count == unset ? 4U : count, destination.GetDim());
      EXPECT_EQ(increment == unset ? 8U : increment, destination.GetDimIncrement());
    }
  }
}

TEST_F(SvdSentinelConsumerTest, DimensionCacheUsesDeviceAddressUnitsAndByteConversion) {
  SvdDevice device(nullptr);
  XMLTreeElement unitBits(nullptr, "addressUnitBits");
  unitBits.SetText("16");
  ASSERT_TRUE(device.ProcessXmlElement(&unitBits));
  SvdItem container(&device);
  SvdRegister reg(&container);
  SvdDimension dimension(&reg);
  dimension.SetDimIncrement(3);

  EXPECT_EQ(16, dimension.GetAddressBitsUnits());
  EXPECT_EQ(6, dimension.CalcAddressIncrement());
  EXPECT_EQ(16, dimension.GetAddressBitsUnits());
  EXPECT_EQ(6, dimension.CalcAddressIncrement());
}

TEST_F(SvdSentinelConsumerTest, DimensionWithoutDeviceDefaultsToEightAddressBits) {
  SvdRegister reg(nullptr);
  SvdDimension dimension(&reg);
  dimension.SetDimIncrement(3);

  EXPECT_EQ(8, dimension.GetAddressBitsUnits());
  EXPECT_EQ(3, dimension.CalcAddressIncrement());
  EXPECT_EQ(8, dimension.GetAddressBitsUnits());
}

TEST_F(SvdSentinelConsumerTest, CpuDistinguishesMissingAndValidSauCountsWithoutConfiguration) {
  for(const auto* count : {static_cast<const char*>(nullptr), "0", "1", "255"}) {
    SCOPED_TRACE(count ? count : "absent");
    m_messages.clear();
    SvdDevice device(nullptr);
    ConfigureCpu(device, count);
    ASSERT_NE(nullptr, device.GetCpu());
    EXPECT_EQ(count ? stoul(count) : numeric_limits<uint32_t>::max(), device.GetCpu()->GetSauNumRegions());
    EXPECT_TRUE(m_messages.empty());
  }
}

TEST_F(SvdSentinelConsumerTest, InvalidAddressBlockDiagnosticsDistinguishUnsetAndZeroValues) {
  struct BlockCase {
    const char* offset;
    const char* size;
    const char* expected;
  };
  SvdPeripheral peripheral(nullptr);
  peripheral.SetName("PORT");
  SvdRegister reg(&peripheral);
  reg.SetName("CTRL");
  reg.SetOffset(0);
  reg.SetBitWidth(32);

  for(const auto& values : {BlockCase{nullptr, nullptr, "Offs:  ---  , Size:  ---  "},
                            BlockCase{nullptr, "4", "Offs:  ---  , Size: 0x0004"},
                            BlockCase{"0", nullptr, "Offs: 0x0000, Size:  ---  "},
                            BlockCase{"0", "0", "Offs: 0x0000, Size: 0x0000"}}) {
    SCOPED_TRACE(values.expected);
    XMLTreeElement element(nullptr, "addressBlock");
    if(values.offset) {
      element.CreateElement("offset", values.offset);
    }
    if(values.size) {
      element.CreateElement("size", values.size);
    }
    element.CreateElement("usage", "registers");
    SvdAddressBlock block(&peripheral);
    ASSERT_TRUE(block.Construct(&element));
    ASSERT_FALSE(block.IsValid());
    m_messages.clear();
    m_addressBlockDiagnostic.clear();

    EXPECT_TRUE(peripheral.CheckRegisterAddress(&reg, {&block}));
    EXPECT_TRUE(HasMessage("M344"));
    EXPECT_NE(string::npos, m_addressBlockDiagnostic.find("Invalid AddressBlock"));
    EXPECT_NE(string::npos, m_addressBlockDiagnostic.find(values.expected));
    EXPECT_EQ(string::npos, m_addressBlockDiagnostic.find("FFFFFFFF"));
  }
}

TEST_F(SvdSentinelConsumerTest, HeaderAndPartitionGenerationDistinguishMissingZeroAndPositiveSauCounts) {
  CreateOutputDirectory();
  ASSERT_FALSE(m_outputDirectory.empty());
  SvdOptions options;
  FileHeaderInfo fileHeader;
  fileHeader.svdFileName = (m_outputDirectory / "input.svd").generic_string();

  for(const auto* count : {static_cast<const char*>(nullptr), "0", "1", "2", "255", "256"}) {
    SCOPED_TRACE(count ? count : "absent");
    m_messages.clear();
    SvdDevice device(nullptr);
    device.SetName("SentinelDevice");
    ConfigureCpu(device, count);
    ASSERT_NE(nullptr, device.GetCpu());
    const bool missing = !count || string(count) == "256";
    const auto expectedCount = missing ? 0U : static_cast<uint32_t>(stoul(count));
    EXPECT_EQ(count && string(count) == "256", HasMessage("M364"));

    // Destruction flushes FileIo before the generated text is inspected.
    {
      HeaderData header(fileHeader, options);
      ASSERT_TRUE(header.Create(&device, (m_outputDirectory / "device.h").generic_string()));
    }
    {
      PartitionData partition(fileHeader, options);
      ASSERT_TRUE(partition.Create(&device, (m_outputDirectory / "partition.h").generic_string()));
    }
    const auto header = ReadOutput("device.h");
    const auto partition = ReadOutput("partition.h");
    EXPECT_EQ(expectedCount ? "1" : "0", DefineValue(header, "__SAUREGION_PRESENT"));
    EXPECT_EQ(to_string(expectedCount), DefineValue(partition, "SAU_REGIONS_MAX"));
    EXPECT_EQ(missing, partition.find("SAU Regions Config: Number of SAU regions not set") != string::npos);
    EXPECT_EQ(missing, partition.find("SAU Setup: Number of SAU regions not set") != string::npos);
    for(uint32_t index = 0; index <= expectedCount; index++) {
      const auto call = "SAU_INIT_REGION(" + to_string(index) + ");";
      EXPECT_EQ(index < expectedCount, partition.find(call) != string::npos) << call;
    }
    EXPECT_EQ(string::npos, partition.find("4294967295"));
  }
}

} // namespace
