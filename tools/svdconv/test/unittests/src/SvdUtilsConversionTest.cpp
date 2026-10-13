/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "SvdUtils.h"
#include "ErrLog.h"

#include "gtest/gtest.h"
#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <list>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace std;

namespace {

class SvdUtilsConversionTest : public testing::Test, public IErrConsumer {
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

  template<typename Enum>
  void CheckEnumConversion(bool (*convert)(const string&, Enum&, uint32_t),
                          initializer_list<pair<const char*, Enum>> values) {
    for(const auto& [text, expected] : values) {
      SCOPED_TRACE(text);
      auto value = Enum::UNDEF;
      m_messages.clear();
      EXPECT_TRUE(convert(text, value, 7U));
      EXPECT_EQ(expected, value);
      EXPECT_TRUE(m_messages.empty());

      string upper = text;
      SvdUtils::ToUpper(upper);
      value = Enum::UNDEF;
      EXPECT_TRUE(convert(upper, value, 7U));
      EXPECT_EQ(expected, value);
      EXPECT_EQ(m_messages, vector<string>{"M225"});
    }
    auto value = Enum::UNDEF;
    EXPECT_FALSE(convert("", value, 7U));
    EXPECT_FALSE(convert("unknown", value, 7U));
    EXPECT_EQ(Enum::UNDEF, value);
  }

  vector<string> m_messages;
  IErrConsumer* m_previousConsumer = nullptr;
  bool m_previousQuiet = false;
  int m_previousErrors = 0;
  int m_previousWarnings = 0;
};

TEST_F(SvdUtilsConversionTest, ConvertsNumericRepresentationsWithoutLosingWidth) {
  const vector<pair<string, uint64_t>> cases{
    {"0", 0U}, {"4294967296", 0x100000000ULL}, {"0xFFFFFFFFFFFFFFFF", numeric_limits<uint64_t>::max()},
    {"0XAbCd", 0xabcdU}, {"0b101010", 42U}, {"#101010", 42U}, {"TRUE", 1U}, {"False", 0U},
  };
  for(const auto& [text, expected] : cases) {
    SCOPED_TRACE(text);
    uint64_t value = 99U;
    EXPECT_TRUE(SvdUtils::ConvertNumber(text, value));
    EXPECT_EQ(expected, value);
  }
  uint32_t unsignedValue = 0U;
  EXPECT_TRUE(SvdUtils::ConvertNumber("0xffffffff", unsignedValue));
  EXPECT_EQ(numeric_limits<uint32_t>::max(), unsignedValue);
  int32_t signedValue = 0;
  EXPECT_TRUE(SvdUtils::ConvertNumber("2147483647", signedValue));
  EXPECT_EQ(numeric_limits<int32_t>::max(), signedValue);
}

TEST_F(SvdUtilsConversionTest, RejectsMalformedNumbersAndUnsupportedBases) {
  for(const auto* text : {"", "0x", "0b", "#", "0xfg", "0b102", "#102", "0b1x", "#1x", "12a", "-1"}) {
    SCOPED_TRACE(text);
    uint64_t value = 17U;
    EXPECT_FALSE(SvdUtils::ConvertNumber(text, value));
    EXPECT_EQ(17U, value);
  }
  uint64_t value = 0U;
  EXPECT_FALSE(SvdUtils::ConvertNumber("12", value, 8U));
  EXPECT_TRUE(SvdUtils::ConvertNumber("ff", value, 16U));
  EXPECT_EQ(255U, value);
}

TEST_F(SvdUtilsConversionTest, ConvertsBooleanValuesAndRejectsNonBooleans) {
  for(const auto* text : {"true", "TRUE", "True", "1"}) {
    bool value = false;
    EXPECT_TRUE(SvdUtils::ConvertNumber(text, value)) << text;
    EXPECT_TRUE(value);
  }
  for(const auto* text : {"false", "FALSE", "False", "0"}) {
    bool value = true;
    EXPECT_TRUE(SvdUtils::ConvertNumber(text, value)) << text;
    EXPECT_FALSE(value);
  }
  for(const auto* text : {"", "2", "yes", "0x1", " true"}) {
    bool value = true;
    EXPECT_FALSE(SvdUtils::ConvertNumber(text, value)) << text;
    EXPECT_TRUE(value);
  }
}

TEST_F(SvdUtilsConversionTest, AccumulatesUniqueNumericValues) {
  set<uint64_t> values{1U};
  EXPECT_TRUE(SvdUtils::ConvertNumber("0x100000000", values));
  EXPECT_TRUE(SvdUtils::ConvertNumber("4294967296", values));
  EXPECT_FALSE(SvdUtils::ConvertNumber("invalid", values));
  EXPECT_FALSE(SvdUtils::ConvertNumber("", values));
  EXPECT_EQ(values, (set<uint64_t>{1U, 0x100000000ULL}));
}

TEST_F(SvdUtilsConversionTest, ExpandsBinaryWildcardsAndEnforcesExpansionLimit) {
  set<uint32_t> values{99U};
  ASSERT_TRUE(SvdUtils::ConvertNumberXBin("#10x", values));
  EXPECT_EQ(values, (set<uint32_t>{4U, 5U}));
  ASSERT_TRUE(SvdUtils::ConvertNumberXBin("0B1X0X", values));
  EXPECT_EQ(values, (set<uint32_t>{8U, 9U, 12U, 13U}));
  ASSERT_TRUE(SvdUtils::ConvertNumberXBin("0x10", values));
  EXPECT_EQ(values, (set<uint32_t>{16U}));
  ASSERT_TRUE(SvdUtils::ConvertNumberXBin("#xxxxxxxx", values));
  EXPECT_EQ(values.size(), 256U);
  EXPECT_EQ(*values.begin(), 0U);
  EXPECT_EQ(*values.rbegin(), 255U);
  EXPECT_FALSE(SvdUtils::ConvertNumberXBin("#xxxxxxxxx", values));
  EXPECT_TRUE(values.empty());
  EXPECT_EQ(m_messages, vector<string>{"M379"});
  for(const auto* text : {"", "0b", "#", "invalid"}) {
    EXPECT_FALSE(SvdUtils::ConvertNumberXBin(text, values)) << text;
  }
}

TEST_F(SvdUtilsConversionTest, ParsesBitRangeAndRejectsMalformedDelimiters) {
  uint32_t msb = 0U;
  uint32_t lsb = 0U;
  EXPECT_TRUE(SvdUtils::ConvertBitRange("[31:16]", msb, lsb));
  EXPECT_EQ(msb, 31U);
  EXPECT_EQ(lsb, 16U);
  EXPECT_TRUE(SvdUtils::ConvertBitRange("[7:7]", msb, lsb));
  EXPECT_EQ(msb, 7U);
  EXPECT_EQ(lsb, 7U);
  for(const auto* text : {"", "31:0]", "[31:0", "[[31:0]", "[31:0]]", "[31]", "[:0]", "[31:]",
                          "[x:0]", "[31:x]"}) {
    EXPECT_FALSE(SvdUtils::ConvertBitRange(text, msb, lsb)) << text;
  }
}

TEST_F(SvdUtilsConversionTest, ConvertsCpuRevisionWithoutMixingMajorAndMinor) {
  const vector<pair<string, uint32_t>> cases{
    {"r0p0", 0U}, {"r1p2", 0x0102U}, {"R2P1", 0x0201U},
    {"r255p0", 0xff00U}, {"r0p255", 0x00ffU}, {"r255p255", 0xffffU},
  };
  for(const auto& [text, expected] : cases) {
    SCOPED_TRACE(text);
    uint32_t revision = 0U;
    EXPECT_TRUE(SvdUtils::ConvertCpuRevision(text, revision));
    EXPECT_EQ(expected, revision);
  }
}

TEST_F(SvdUtilsConversionTest, RejectsMalformedAndOutOfRangeCpuRevisions) {
  for(const auto* text : {"", "1p2", "r1", "rp1", "r1p", "rAp1", "r1pA", "r1p2p3",
                          "r256p0", "r0p256", "r-1p0"}) {
    SCOPED_TRACE(text);
    uint32_t revision = 0x1234U;
    EXPECT_FALSE(SvdUtils::ConvertCpuRevision(text, revision));
    EXPECT_EQ(0x1234U, revision);
  }
}

TEST_F(SvdUtilsConversionTest, NormalizesCpuTypeNamesAndCortexM0PlusAlias) {
  using Cpu = SvdTypes::CpuType;
  const vector<pair<string, Cpu>> cases{
    {"CM0", Cpu::CM0}, {"cm0+", Cpu::CM0PLUS}, {"Cm0Plus", Cpu::CM0PLUS},
    {"cm33", Cpu::CM33}, {"armv81mml", Cpu::V81MML}, {"other", Cpu::OTHER},
  };
  for(const auto& [text, expected] : cases) {
    SCOPED_TRACE(text);
    auto cpu = Cpu::UNDEF;
    EXPECT_TRUE(SvdUtils::ConvertCpuType(text, cpu));
    EXPECT_EQ(expected, cpu);
  }
  for(const auto* text : {"", "CM999", "not-a-cpu"}) {
    auto cpu = Cpu::UNDEF;
    EXPECT_FALSE(SvdUtils::ConvertCpuType(text, cpu)) << text;
  }
}

TEST_F(SvdUtilsConversionTest, ConvertsCpuEndianAndDiagnosesNoncanonicalCase) {
  using Endian = SvdTypes::Endian;
  const vector<pair<string, Endian>> cases{
    {"little", Endian::LITTLE}, {"big", Endian::BIG},
    {"selectable", Endian::SELECTABLE}, {"other", Endian::OTHER},
  };
  for(const auto& [text, expected] : cases) {
    SCOPED_TRACE(text);
    auto endian = Endian::UNDEF;
    m_messages.clear();
    EXPECT_TRUE(SvdUtils::ConvertCpuEndian(text, endian, 7U));
    EXPECT_EQ(expected, endian);
    EXPECT_TRUE(m_messages.empty());

    string upper = text;
    SvdUtils::ToUpper(upper);
    endian = Endian::UNDEF;
    EXPECT_TRUE(SvdUtils::ConvertCpuEndian(upper, endian, 7U));
    EXPECT_EQ(expected, endian);
    EXPECT_EQ(m_messages, vector<string>{"M225"});
  }
  for(const auto* text : {"", "unknown"}) {
    auto endian = Endian::UNDEF;
    EXPECT_FALSE(SvdUtils::ConvertCpuEndian(text, endian, 7U)) << text;
  }
}

TEST_F(SvdUtilsConversionTest, ConvertsIntegerAndPointerDataTypes) {
  for(const auto* baseType : {"uint8_t", "uint16_t", "uint32_t", "uint64_t",
                              "int8_t", "int16_t", "int32_t", "int64_t"}) {
    for(const auto* suffix : {"", "*"}) {
      const string expected = string(baseType) + suffix;
      SCOPED_TRACE(expected);
      string dataType;
      EXPECT_TRUE(SvdUtils::ConvertDataType(expected, dataType, 7U));
      EXPECT_EQ(expected, dataType);

      string upper = expected;
      SvdUtils::ToUpper(upper);
      dataType.clear();
      EXPECT_TRUE(SvdUtils::ConvertDataType(upper, dataType, 7U));
      EXPECT_EQ(expected, dataType);
    }
  }
}

TEST_F(SvdUtilsConversionTest, RejectsUnsupportedDataTypesWithoutReplacingPreviousType) {
  for(const auto* text : {"", "float", "uint24_t", "int128_t", "uint32_t**", "uint32_tjunk"}) {
    SCOPED_TRACE(text);
    string dataType = "uint16_t";
    EXPECT_FALSE(SvdUtils::ConvertDataType(text, dataType, 7U));
    EXPECT_EQ("uint16_t", dataType);
  }
}

TEST_F(SvdUtilsConversionTest, DistinguishesGeneralProtectionFromSauProtection) {
  using Protection = SvdTypes::ProtectionType;
  for(const auto& [text, expected] : vector<pair<string, Protection>>{
        {"s", Protection::SECURE}, {"n", Protection::NONSECURE}, {"p", Protection::PRIVILEGED}}) {
    SCOPED_TRACE(text);
    auto protection = Protection::UNDEF;
    EXPECT_TRUE(SvdUtils::ConvertProtectionStringType(text, protection, 7U));
    EXPECT_EQ(expected, protection);

    protection = Protection::UNDEF;
    if(expected == Protection::PRIVILEGED) {
      EXPECT_FALSE(SvdUtils::ConvertSauProtectionStringType(text, protection, 7U));
    }
    else {
      EXPECT_TRUE(SvdUtils::ConvertSauProtectionStringType(text, protection, 7U));
      EXPECT_EQ(expected, protection);
    }
  }
  for(const auto* text : {"", "unknown", "secure"}) {
    auto protection = Protection::UNDEF;
    EXPECT_FALSE(SvdUtils::ConvertProtectionStringType(text, protection, 7U)) << text;
    EXPECT_FALSE(SvdUtils::ConvertSauProtectionStringType(text, protection, 7U)) << text;
  }
  EXPECT_TRUE(m_messages.empty());
}

TEST_F(SvdUtilsConversionTest, RejectsNoncanonicalProtectionWithCaseDiagnostic) {
  for(const auto* text : {"S", "N", "P"}) {
    SCOPED_TRACE(text);
    auto protection = SvdTypes::ProtectionType::UNDEF;
    m_messages.clear();
    EXPECT_FALSE(SvdUtils::ConvertProtectionStringType(text, protection, 7U));
    EXPECT_EQ(m_messages, vector<string>{"M225"});
  }
  for(const auto* text : {"S", "N"}) {
    SCOPED_TRACE(text);
    auto protection = SvdTypes::ProtectionType::UNDEF;
    m_messages.clear();
    EXPECT_FALSE(SvdUtils::ConvertSauProtectionStringType(text, protection, 7U));
    EXPECT_EQ(m_messages, vector<string>{"M225"});
  }
}

TEST_F(SvdUtilsConversionTest, ConvertsSauAccessAndRejectsEmptyOrNoncanonicalCase) {
  using Access = SvdTypes::SauAccessType;
  for(const auto& [text, expected] : vector<pair<string, Access>>{
        {"c", Access::SECURE}, {"n", Access::NONSECURE}}) {
    SCOPED_TRACE(text);
    auto access = Access::UNDEF;
    m_messages.clear();
    EXPECT_TRUE(SvdUtils::ConvertSauAccessType(text, access, 7U));
    EXPECT_EQ(expected, access);
    EXPECT_TRUE(m_messages.empty());

    string upper = text;
    SvdUtils::ToUpper(upper);
    EXPECT_FALSE(SvdUtils::ConvertSauAccessType(upper, access, 7U));
    EXPECT_EQ(m_messages, vector<string>{"M225"});
  }
  auto access = Access::UNDEF;
  EXPECT_FALSE(SvdUtils::ConvertSauAccessType("", access, 7U));
}

TEST_F(SvdUtilsConversionTest, ConvertsAddressBlockAndEnumUsage) {
  using Block = SvdTypes::AddrBlockUsage;
  CheckEnumConversion<Block>(SvdUtils::ConvertAddrBlockUsage,
    {{"registers", Block::REGISTERS}, {"buffer", Block::BUFFER}, {"reserved", Block::RESERVED}});
  using Usage = SvdTypes::EnumUsage;
  CheckEnumConversion<Usage>(SvdUtils::ConvertEnumUsage,
    {{"read", Usage::READ}, {"write", Usage::WRITE}, {"read-write", Usage::READWRITE}});
}

TEST_F(SvdUtilsConversionTest, ConvertsModifiedWriteValues) {
  using Write = SvdTypes::ModifiedWriteValue;
  CheckEnumConversion<Write>(SvdUtils::ConvertModifiedWriteValues, {
    {"oneToClear", Write::ONETOCLEAR}, {"oneToSet", Write::ONETOSET}, {"oneToToggle", Write::ONETOTOGGLE},
    {"zeroToClear", Write::ZEROTOCLEAR}, {"zeroToSet", Write::ZEROTOSET}, {"zeroToToggle", Write::ZEROTOTOGGLE},
    {"clear", Write::CLEAR}, {"set", Write::SET}, {"modify", Write::MODIFY},
  });
}

TEST_F(SvdUtilsConversionTest, ConvertsReadActions) {
  using Action = SvdTypes::ReadAction;
  CheckEnumConversion<Action>(SvdUtils::ConvertReadAction, {
    {"clear", Action::CLEAR}, {"set", Action::SET}, {"modify", Action::MODIFY}, {"modifyExternal", Action::MODIFEXT},
  });
}

TEST_F(SvdUtilsConversionTest, MatchesFieldReadWriteCapabilitiesToRegisterAccess) {
  using Access = SvdTypes::Access;
  const vector<pair<Access, unsigned>> capabilities{
    {Access::UNDEF, 0U}, {Access::READONLY, 1U}, {Access::WRITEONLY, 2U}, {Access::READWRITE, 3U},
  };
  for(const auto& [registerAccess, registerMask] : capabilities) {
    for(const auto& [fieldAccess, fieldMask] : capabilities) {
      SCOPED_TRACE(testing::Message() << "register=" << static_cast<int>(registerAccess)
                                    << ", field=" << static_cast<int>(fieldAccess));
      const bool expected = fieldMask != 0U && (fieldMask & registerMask) == fieldMask;
      EXPECT_EQ(expected, SvdUtils::IsMatchAccess(fieldAccess, registerAccess));
    }
  }
  EXPECT_TRUE(SvdUtils::IsMatchAccess(Access::WRITEONCE, Access::WRITEONLY));
  EXPECT_TRUE(SvdUtils::IsMatchAccess(Access::WRITEONCE, Access::WRITEONCE));
  EXPECT_TRUE(SvdUtils::IsMatchAccess(Access::READONLY, Access::READWRITEONCE));
  EXPECT_TRUE(SvdUtils::IsMatchAccess(Access::READWRITEONCE, Access::READWRITEONCE));
  EXPECT_FALSE(SvdUtils::IsMatchAccess(Access::READONLY, Access::WRITEONCE));
  EXPECT_FALSE(SvdUtils::IsMatchAccess(Access::WRITEONCE, Access::READONLY));
  EXPECT_FALSE(SvdUtils::IsMatchAccess(Access::UNDEF, Access::WRITEONCE));
  EXPECT_FALSE(SvdUtils::IsMatchAccess(Access::UNDEF, Access::READWRITEONCE));
}

TEST_F(SvdUtilsConversionTest, AggregatesFieldAccessWithoutRestrictingOtherFields) {
  using Access = SvdTypes::Access;
  const Access capabilities[]{Access::UNDEF, Access::READONLY, Access::WRITEONLY, Access::READWRITE};
  for(size_t registerMask = 0; registerMask < 4U; registerMask++) {
    for(size_t fieldMask = 0; fieldMask < 4U; fieldMask++) {
      SCOPED_TRACE(testing::Message() << "registerMask=" << registerMask << ", fieldMask=" << fieldMask);
      const auto expected = capabilities[registerMask | fieldMask];
      EXPECT_EQ(expected, SvdUtils::CalcAccessResult(capabilities[fieldMask], capabilities[registerMask]));
    }
  }
  for(const auto access : {Access::WRITEONCE, Access::READWRITEONCE}) {
    EXPECT_EQ(access, SvdUtils::CalcAccessResult(access, Access::UNDEF));
    EXPECT_EQ(access, SvdUtils::CalcAccessResult(Access::UNDEF, access));
    EXPECT_EQ(Access::READWRITE, SvdUtils::CalcAccessResult(access, Access::READWRITE));
    EXPECT_EQ(Access::READWRITE, SvdUtils::CalcAccessResult(Access::READWRITE, access));
  }
  EXPECT_EQ(Access::READWRITEONCE, SvdUtils::CalcAccessResult(Access::WRITEONCE, Access::READONLY));
  EXPECT_EQ(Access::READWRITEONCE, SvdUtils::CalcAccessResult(Access::READONLY, Access::WRITEONCE));
}

TEST_F(SvdUtilsConversionTest, SortsRegisterNamesNaturallyAndSupportsCaseInsensitiveComparison) {
  vector<string> names{"REG10", "REG2", "REG1B", "REG1", "REG1A", "REG"};
  sort(names.begin(), names.end(), SvdUtils::StringAlnumLess{});
  EXPECT_EQ(names, (vector<string>{"REG", "REG1", "REG1A", "REG1B", "REG2", "REG10"}));
  EXPECT_EQ(0, SvdUtils::AlnumCmp("REG12A", "reg12a", false));
  EXPECT_LT(SvdUtils::AlnumCmp("REG12A", "reg12a", true), 0);
  EXPECT_LT(SvdUtils::AlnumCmp("REG2A", "REG2B"), 0);
  EXPECT_GT(SvdUtils::AlnumCmp("REG2B", "REG2A"), 0);
  EXPECT_EQ(0, SvdUtils::AlnumCmp(string{}, string{}));
  EXPECT_LT(SvdUtils::AlnumCmp(string{}, string{"REG1"}), 0);
  EXPECT_GT(SvdUtils::AlnumCmp(string{"REG1"}, string{}), 0);
}

TEST_F(SvdUtilsConversionTest, ParsesDimensionExpressions) {
  struct Case {
    const char* text;
    SvdTypes::Expression kind;
    const char* name;
    uint32_t insertPosition;
  };
  using Expression = SvdTypes::Expression;
  const Case cases[] = {
    {"DATA", Expression::NONE, "DATA", numeric_limits<uint32_t>::max()},
    {"DATA[%s]", Expression::ARRAY, "DATA", 4U},
    {"CH%s_DATA", Expression::EXTEND, "CH_DATA", 2U},
  };
  for(const auto& test : cases) {
    SCOPED_TRACE(test.text);
    string name;
    uint32_t position = 0U;
    EXPECT_EQ(SvdUtils::ParseExpression(test.text, name, position), test.kind);
    EXPECT_EQ(name, test.name);
    EXPECT_EQ(position, test.insertPosition);
  }
}

TEST_F(SvdUtilsConversionTest, SplitsDerivedNamesAndMarksEmptyComponents) {
  const vector<pair<string, list<string>>> cases{
    {"PERIPHERAL", {"PERIPHERAL"}}, {"PERIPHERAL.CLUSTER.REG", {"PERIPHERAL", "CLUSTER", "REG"}},
    {"", {"!ERROR!"}}, {".REG", {"!ERROR!", "REG"}}, {"PERIPHERAL.", {"PERIPHERAL", "!ERROR!"}},
    {"PERIPHERAL..REG", {"PERIPHERAL", "!ERROR!", "REG"}},
  };
  for(const auto& [text, expected] : cases) {
    SCOPED_TRACE(text);
    list<string> names;
    EXPECT_TRUE(SvdUtils::ConvertDerivedNameHirachy(text, names));
    EXPECT_EQ(names, expected);
  }
}

} // namespace
