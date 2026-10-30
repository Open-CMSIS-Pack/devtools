/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "SvdItem.h"
#include "SvdAddressBlock.h"
#include "SvdDimension.h"
#include "SvdEnum.h"
#include "SvdField.h"
#include "SvdRegister.h"
#include "ErrLog.h"
#include "XMLTree.h"

#include "gtest/gtest.h"
#include <algorithm>
#include <cstdint>
#include <list>
#include <map>
#include <string>
#include <utility>
#include <vector>

using namespace std;

namespace {

class SvdModelValidationTest : public testing::Test, public IErrConsumer {
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
    if(msg.GetMsgNum() == "M313") {
      m_rangeWidth = msg.GetSubstitute("NUM");
    }
    return true;
  }

  bool HasMessage(const string& id) const {
    return find(m_messages.begin(), m_messages.end(), id) != m_messages.end();
  }

  static void AddAddressBlockFields(XMLTreeElement& element, const char* offset = "0",
                                     const char* size = "4", const char* usage = "registers") {
    for(const auto& [tag, value] : {pair{"offset", offset}, pair{"size", size}, pair{"usage", usage}}) {
      if(value) {
        element.CreateElement(tag, value);
      }
    }
  }

  static void AddEnum(XMLTreeElement& container, const char* value, const char* isDefault = nullptr) {
    const auto element = container.CreateElement("enumeratedValue");
    element->CreateElement("name", "MODE");
    element->CreateElement("description", "Operating mode.");
    if(value) {
      element->CreateElement("value", value);
    }
    if(isDefault) {
      element->CreateElement("isDefault", isDefault);
    }
  }

  static void ConfigureDimension(SvdRegister& reg, const char* count, const char* indices) {
    reg.SetName("CHANNEL%s");
    reg.SetDescription("Control channel %s.");
    for(const auto& [tag, value] : {pair{"dim", count}, pair{"dimIncrement", "4"}, pair{"dimIndex", indices}}) {
      XMLTreeElement element(nullptr, tag);
      element.SetText(value);
      ASSERT_TRUE(reg.ProcessXmlElement(&element));
    }
  }

  vector<string> m_messages;
  string m_rangeWidth;

private:
  IErrConsumer* m_previousConsumer = nullptr;
  bool m_previousQuiet = false;
  int m_previousErrors = 0;
  int m_previousWarnings = 0;
};

TEST_F(SvdModelValidationTest, AddressBlockRequiresOffsetSizeAndUsage) {
  struct MissingField {
    const char* offset;
    const char* size;
    const char* usage;
    const char* diagnostic;
  };
  for(const auto& values : {MissingField{nullptr, "4", "registers", "M314"},
                            MissingField{"0", nullptr, "registers", "M314"},
                            MissingField{"0", "4", nullptr, "M359"}}) {
    SCOPED_TRACE(values.diagnostic);
    m_messages.clear();
    XMLTreeElement element(nullptr, "addressBlock");
    AddAddressBlockFields(element, values.offset, values.size, values.usage);
    SvdAddressBlock block(nullptr);
    ASSERT_TRUE(block.Construct(&element));
    EXPECT_FALSE(block.IsValid());
    EXPECT_TRUE(HasMessage(values.diagnostic));
  }
}

TEST_F(SvdModelValidationTest, AddressBlockRejectsZeroSize) {
  XMLTreeElement element(nullptr, "addressBlock");
  AddAddressBlockFields(element, "0", "0");
  SvdAddressBlock block(nullptr);
  ASSERT_TRUE(block.Construct(&element));

  EXPECT_FALSE(block.IsValid());
  EXPECT_TRUE(HasMessage("M315"));
}

TEST_F(SvdModelValidationTest, AddressBlockReportsMalformedFields) {
  for(const auto* tag : {"offset", "size", "usage"}) {
    SCOPED_TRACE(tag);
    m_messages.clear();
    XMLTreeElement element(nullptr, "addressBlock");
    AddAddressBlockFields(element, string(tag) == "offset" ? "invalid" : "0",
                          string(tag) == "size" ? "invalid" : "4",
                          string(tag) == "usage" ? "invalid" : "registers");
    SvdAddressBlock block(nullptr);
    ASSERT_TRUE(block.Construct(&element));
    EXPECT_TRUE(HasMessage("M202"));
  }
}

TEST_F(SvdModelValidationTest, FieldRejectsReversedBitRanges) {
  for(const bool useBitRange : {false, true}) {
    for(const auto& [lsb, width] : {pair{1U, "0"}, pair{2U, "-1"}, pair{63U, "-62"},
                                   pair{UINT32_MAX - 1, "-4294967293"}}) {
      SCOPED_TRACE(useBitRange ? "bitRange" : "lsb/msb");
      SCOPED_TRACE(lsb);
      m_messages.clear();
      m_rangeWidth.clear();
      XMLTreeElement element(nullptr, "field");
      element.CreateElement("name", "BAD");
      element.CreateElement("description", "Reversed bit range.");
      if(useBitRange) {
        element.CreateElement("bitRange", "[0:" + to_string(lsb) + "]");
      }
      else {
        element.CreateElement("lsb", to_string(lsb));
        element.CreateElement("msb", "0");
      }
      SvdField field(nullptr);

      ASSERT_TRUE(field.Construct(&element));

      ASSERT_FALSE(field.IsValid());
      EXPECT_EQ(1, count(m_messages.begin(), m_messages.end(), "M313"));
      EXPECT_EQ(width, m_rangeWidth);
      EXPECT_FALSE(HasMessage("M311"));
      EXPECT_GT(field.GetBitWidth(), 0);
    }
  }
}

TEST_F(SvdModelValidationTest, FieldDimensionDoesNotExpandReversedBitRanges) {
  for(const bool useBitRange : {false, true}) {
    SCOPED_TRACE(useBitRange ? "bitRange" : "lsb/msb");
    m_messages.clear();
    SvdRegister reg(nullptr);
    reg.SetBitWidth(32);
    reg.SetAccess(SvdTypes::Access::READWRITE);
    SvdFieldContainer fields(&reg);
    SvdField field(&fields);
    XMLTreeElement element(nullptr, "field");
    element.CreateElement("dim", "2");
    element.CreateElement("dimIncrement", "1");
    element.CreateElement("name", "BAD%s");
    element.CreateElement("description", "Reversed bit range.");
    if(useBitRange) {
      element.CreateElement("bitRange", "[0:1]");
    }
    else {
      element.CreateElement("lsb", "1");
      element.CreateElement("msb", "0");
    }

    ASSERT_TRUE(field.Construct(&element));

    ASSERT_FALSE(field.IsValid());
    EXPECT_EQ(1, count(m_messages.begin(), m_messages.end(), "M313"));
    EXPECT_FALSE(HasMessage("M311"));
    ASSERT_NE(nullptr, field.GetDimension());
    EXPECT_TRUE(field.GetDimension()->GetChildren().empty());
  }
}

TEST_F(SvdModelValidationTest, FieldAcceptsValidBitRangesIncludingSingleBits) {
  for(const bool useBitRange : {false, true}) {
    for(const auto& [lsb, msb] : {pair{0U, 0U}, pair{0U, 31U}, pair{5U, 12U}, pair{63U, 63U}}) {
      SCOPED_TRACE(useBitRange ? "bitRange" : "lsb/msb");
      SCOPED_TRACE(lsb);
      SCOPED_TRACE(msb);
      m_messages.clear();
      XMLTreeElement element(nullptr, "field");
      element.CreateElement("name", "VALID");
      element.CreateElement("description", "Valid bit range.");
      if(useBitRange) {
        element.CreateElement("bitRange", "[" + to_string(msb) + ":" + to_string(lsb) + "]");
      }
      else {
        element.CreateElement("lsb", to_string(lsb));
        element.CreateElement("msb", to_string(msb));
      }
      SvdField field(nullptr);

      ASSERT_TRUE(field.Construct(&element));

      EXPECT_TRUE(field.IsValid());
      EXPECT_EQ(lsb, field.GetOffset());
      EXPECT_EQ(msb - lsb + 1, field.GetBitWidth());
      EXPECT_FALSE(HasMessage("M313"));
      EXPECT_FALSE(HasMessage("M311"));
    }
  }
}

TEST_F(SvdModelValidationTest, LargeAddressBlocksWarnWithoutLosingCompatibility) {
  for(const bool testOffset : {false, true}) {
    for(const auto value : {0x1000000U, 0x1000001U}) {
      SCOPED_TRACE(testOffset ? "offset" : "size");
      SCOPED_TRACE(value);
      m_messages.clear();
      XMLTreeElement element(nullptr, "addressBlock");
      const auto text = to_string(value);
      AddAddressBlockFields(element, testOffset ? text.c_str() : "0", testOffset ? "4" : text.c_str());
      SvdAddressBlock block(nullptr);
      ASSERT_TRUE(block.Construct(&element));

      EXPECT_TRUE(block.IsValid());
      EXPECT_EQ(value > 0x1000000U, HasMessage("M360"));
      EXPECT_EQ(value, testOffset ? block.GetOffset() : block.GetSize());
    }
  }
}

TEST_F(SvdModelValidationTest, NonDefaultEnumRequiresValue) {
  SvdField field(nullptr);
  SvdEnumContainer container(&field);
  XMLTreeElement element(nullptr, "enumeratedValues");
  AddEnum(element, nullptr);
  ASSERT_TRUE(container.Construct(&element));

  ASSERT_EQ(1U, container.GetChildCount());
  EXPECT_FALSE(container.GetChildren().front()->IsValid());
  EXPECT_TRUE(HasMessage("M369"));
}

TEST_F(SvdModelValidationTest, DefaultEnumNeedsNoNumericValueAndRegistersWithContainer) {
  SvdField field(nullptr);
  SvdEnumContainer container(&field);
  XMLTreeElement element(nullptr, "enumeratedValues");
  AddEnum(element, nullptr, "true");
  ASSERT_TRUE(container.Construct(&element));

  ASSERT_EQ(1U, container.GetChildCount());
  const auto value = dynamic_cast<SvdEnum*>(container.GetChildren().front());
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->IsValid());
  EXPECT_TRUE(value->IsDefault());
  EXPECT_FALSE(value->GetValue().bValid);
  EXPECT_EQ(value, container.GetDefaultValue());
  EXPECT_TRUE(m_messages.empty());
}

TEST_F(SvdModelValidationTest, MalformedEnumValueIsInvalid) {
  SvdField field(nullptr);
  SvdEnumContainer container(&field);
  XMLTreeElement element(nullptr, "enumeratedValues");
  AddEnum(element, "invalid");
  ASSERT_TRUE(container.Construct(&element));

  ASSERT_EQ(1U, container.GetChildCount());
  EXPECT_FALSE(container.GetChildren().front()->IsValid());
  EXPECT_TRUE(HasMessage("M202"));
}

TEST_F(SvdModelValidationTest, DimensionArrayIndexDisallowsDefaultEnum) {
  SvdRegister reg(nullptr);
  SvdEnumContainer container(&reg);
  XMLTreeElement element(nullptr, "dimArrayIndex");
  AddEnum(element, nullptr, "true");
  ASSERT_TRUE(container.Construct(&element));

  ASSERT_EQ(1U, container.GetChildCount());
  EXPECT_FALSE(container.GetChildren().front()->IsValid());
  EXPECT_EQ(nullptr, container.GetDefaultValue());
  EXPECT_TRUE(HasMessage("M231"));
}

TEST_F(SvdModelValidationTest, WildcardEnumExpandsIntoDistinctNamedValues) {
  SvdField field(nullptr);
  SvdEnumContainer container(&field);
  XMLTreeElement element(nullptr, "enumeratedValues");
  AddEnum(element, "#10x");
  ASSERT_TRUE(container.Construct(&element));

  ASSERT_EQ(2U, container.GetChildCount());
  map<string, uint32_t> values;
  for(const auto child : container.GetChildren()) {
    const auto value = dynamic_cast<SvdEnum*>(child);
    ASSERT_NE(nullptr, value);
    EXPECT_TRUE(value->IsValid());
    EXPECT_TRUE(value->GetValue().bValid);
    EXPECT_EQ("Operating mode.", value->GetDescription());
    values.emplace(value->GetName(), value->GetValue().u32);
  }
  const map<string, uint32_t> expected{{"MODE_4", 4U}, {"MODE_5", 5U}};
  EXPECT_EQ(expected, values);
  EXPECT_TRUE(m_messages.empty());
}

TEST_F(SvdModelValidationTest, ExplicitDimensionIndicesGenerateNames) {
  SvdRegister reg(nullptr);
  ASSERT_NO_FATAL_FAILURE(ConfigureDimension(reg, "3", "LEFT, CENTER, RIGHT"));
  const auto dim = reg.GetDimension();
  ASSERT_NE(nullptr, dim);
  ASSERT_TRUE(dim->CalculateDim());

  const list<string> expected{"LEFT", "CENTER", "RIGHT"};
  EXPECT_EQ(expected, dim->GetDimIndexList());
  EXPECT_EQ("CHANNELCENTER", dim->CreateName("CENTER"));
  EXPECT_EQ("Control channel CENTER.", dim->CreateDescription("CENTER"));
  EXPECT_TRUE(reg.IsValid());
  EXPECT_TRUE(m_messages.empty());
}

TEST_F(SvdModelValidationTest, AlphabeticDimensionRangeExpandsInOrder) {
  SvdRegister reg(nullptr);
  ASSERT_NO_FATAL_FAILURE(ConfigureDimension(reg, "3", "A-C"));
  const auto dim = reg.GetDimension();
  ASSERT_NE(nullptr, dim);
  ASSERT_TRUE(dim->CalculateDim());

  const list<string> expected{"A", "B", "C"};
  EXPECT_EQ(expected, dim->GetDimIndexList());
  EXPECT_EQ("CHANNELA", dim->CreateName("A"));
  EXPECT_EQ("CHANNELC", dim->CreateName("C"));
  EXPECT_TRUE(m_messages.empty());
}

TEST_F(SvdModelValidationTest, InvalidDimensionRangesDoNotProduceIndices) {
  for(const auto* indices : {"AA-AC", "C-A", "A-D", "0-3"}) {
    SCOPED_TRACE(indices);
    SvdRegister reg(nullptr);
    ASSERT_NO_FATAL_FAILURE(ConfigureDimension(reg, "3", indices));
    const auto dim = reg.GetDimension();
    ASSERT_NE(nullptr, dim);
    EXPECT_FALSE(dim->CalculateDimIndex());
    EXPECT_TRUE(dim->GetDimIndexList().empty());
  }
}

TEST_F(SvdModelValidationTest, DimensionListsDiagnoseCountMismatchAndDuplicateIndices) {
  for(const auto& [indices, diagnostic] : {pair{"A,B", "M308"}, pair{"A,A,B", "M336"}}) {
    SCOPED_TRACE(indices);
    m_messages.clear();
    SvdRegister reg(nullptr);
    ASSERT_NO_FATAL_FAILURE(ConfigureDimension(reg, "3", indices));
    const auto dim = reg.GetDimension();
    ASSERT_NE(nullptr, dim);
    ASSERT_TRUE(dim->CalculateDim());
    EXPECT_TRUE(HasMessage(diagnostic));
  }
}

} // namespace
