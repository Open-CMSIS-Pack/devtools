/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "SvdItem.h"
#include "SvdAddressBlock.h"
#include "SvdField.h"
#include "SvdRegister.h"
#include "ErrLog.h"
#include "XMLTree.h"

#include "gtest/gtest.h"
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

using namespace std;

namespace {

class SvdSentinelModelTest : public testing::Test, public IErrConsumer {
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

  bool HasMessage(const string& id) const {
    return find(m_messages.begin(), m_messages.end(), id) != m_messages.end();
  }

  static void SetIdentity(SvdItem& item) {
    item.SetName("CONTROL");
    item.SetDescription("Control configuration.");
  }

  static void ReadFieldElement(SvdField& field, const char* tag, const char* value) {
    XMLTreeElement element(nullptr, tag);
    element.SetText(value);
    ASSERT_TRUE(field.ProcessXmlElement(&element));
  }

  vector<string> m_messages;

private:
  IErrConsumer* m_previousConsumer = nullptr;
  bool m_previousQuiet = false;
  int m_previousErrors = 0;
  int m_previousWarnings = 0;
};

TEST_F(SvdSentinelModelTest, RegisterCopyInheritsMissingOffsetIncludingZeroAnd64BitValues) {
  for(const uint64_t value : {UINT64_C(0), UINT64_C(0x20), UINT64_C(0x100000020), UINT64_MAX - 1}) {
    SCOPED_TRACE(value);
    SvdRegister source(nullptr);
    source.SetOffset(value);
    SvdRegister target(nullptr);

    target.CopyItem(&source);

    EXPECT_EQ(value, target.GetOffset());
  }
}

TEST_F(SvdSentinelModelTest, RegisterCopyPreservesExplicitOffsetIncludingZeroAnd64BitValues) {
  SvdRegister source(nullptr);
  source.SetOffset(0x40);
  for(const uint64_t value : {UINT64_C(0), UINT64_C(0x20), UINT64_C(0x100000020), UINT64_MAX - 1}) {
    SCOPED_TRACE(value);
    SvdRegister target(nullptr);
    target.SetOffset(value);

    target.CopyItem(&source);

    EXPECT_EQ(value, target.GetOffset());
  }
}

TEST_F(SvdSentinelModelTest, RegisterMissingDonorOffsetRemainsAvailableForLaterInheritance) {
  SvdRegister missing(nullptr);
  SvdRegister target(nullptr);
  target.CopyItem(&missing);
  EXPECT_EQ(UINT64_MAX, target.GetOffset());

  SvdRegister source(nullptr);
  source.SetOffset(0);
  target.CopyItem(&source);
  EXPECT_EQ(0U, target.GetOffset());

  target.CopyItem(&missing);
  EXPECT_EQ(0U, target.GetOffset());
}

TEST_F(SvdSentinelModelTest, RegisterValidationDistinguishesMissingOffsetFromZeroAnd64BitOffset) {
  for(const uint64_t value : {UINT64_MAX, UINT64_C(0), UINT64_C(0x20), UINT64_C(0x100000020)}) {
    SCOPED_TRACE(value);
    m_messages.clear();
    SvdRegister reg(nullptr);
    SetIdentity(reg);
    reg.SetBitWidth(32);
    if(value != UINT64_MAX) {
      reg.SetOffset(value);
    }

    ASSERT_TRUE(reg.CheckItem());

    EXPECT_EQ(value != UINT64_MAX, reg.IsValid());
    EXPECT_EQ(value == UINT64_MAX, HasMessage("M370"));
    EXPECT_EQ(value, reg.GetOffset());
  }
}

TEST_F(SvdSentinelModelTest, FieldCalculateConvertsCompleteBitBoundsIncludingBitZero) {
  struct Range {
    const char* lsb;
    const char* msb;
    uint64_t offset;
    int32_t width;
  };
  for(const auto& range : {Range{"0", "0", 0, 1}, Range{"0", "31", 0, 32},
                            Range{"5", "12", 5, 8}, Range{"63", "63", 63, 1}}) {
    SCOPED_TRACE(range.lsb);
    SCOPED_TRACE(range.msb);
    SvdField field(nullptr);
    ReadFieldElement(field, "lsb", range.lsb);
    ReadFieldElement(field, "msb", range.msb);

    ASSERT_TRUE(field.Calculate());

    EXPECT_EQ(range.offset, field.GetOffset());
    EXPECT_EQ(range.width, field.GetBitWidth());
  }
}

TEST_F(SvdSentinelModelTest, FieldCalculateLeavesIncompleteBitBoundsUnset) {
  for(const auto* tag : {"", "lsb", "msb"}) {
    SCOPED_TRACE(tag);
    SvdField field(nullptr);
    if(*tag) {
      ReadFieldElement(field, tag, "0");
    }

    ASSERT_TRUE(field.Calculate());

    EXPECT_EQ(UINT64_MAX, field.GetOffset());
    EXPECT_EQ(-1, field.GetBitWidth());
  }
}

TEST_F(SvdSentinelModelTest, FieldCalculatePreservesExplicitOffsetAndWidth) {
  for(const uint64_t offset : {UINT64_C(0), UINT64_C(4)}) {
    SCOPED_TRACE(offset);
    SvdField field(nullptr);
    field.SetLsb(12);
    field.SetMsb(15);
    field.SetOffset(offset);
    field.SetBitWidth(2);

    ASSERT_TRUE(field.Calculate());

    EXPECT_EQ(offset, field.GetOffset());
    EXPECT_EQ(2, field.GetBitWidth());
  }
}

TEST_F(SvdSentinelModelTest, FieldCopyInheritsUnsetPropertiesAndPreservesIndependentOverrides) {
  SvdField source(nullptr);
  source.SetOffset(8);
  source.SetLsb(8);
  source.SetMsb(15);
  source.SetBitWidth(8);
  for(unsigned overrides = 0; overrides < 8; overrides++) {
    for(const uint32_t value : {0U, 3U}) {
      SCOPED_TRACE(overrides);
      SCOPED_TRACE(value);
      SvdField target(nullptr);
      if(overrides & 1) {
        target.SetOffset(value);
      }
      if(overrides & 2) {
        target.SetLsb(value);
      }
      if(overrides & 4) {
        target.SetMsb(value);
      }

      ASSERT_TRUE(target.CopyItem(&source));

      EXPECT_EQ((overrides & 1) ? value : 8U, target.GetOffset());
      EXPECT_EQ((overrides & 2) ? value : 8U, target.GetLsb());
      EXPECT_EQ((overrides & 4) ? value : 15U, target.GetMsb());
      EXPECT_EQ(8, target.GetBitWidth());
    }
  }
}

TEST_F(SvdSentinelModelTest, FieldCopyRetains64BitOffsetUntilValidationReportsOutOfRange) {
  SvdField source(nullptr);
  source.SetOffset(UINT64_C(0x100000000));
  source.SetBitWidth(1);
  SetIdentity(source);
  SvdField target(nullptr);

  ASSERT_TRUE(target.CopyItem(&source));
  EXPECT_EQ(UINT64_C(0x100000000), target.GetOffset());
  ASSERT_TRUE(target.CheckItem());

  EXPECT_FALSE(target.IsValid());
  EXPECT_TRUE(HasMessage("M309"));
  EXPECT_FALSE(HasMessage("M311"));
}

TEST_F(SvdSentinelModelTest, FieldMissingDonorPropertiesRemainAvailableForLaterInheritance) {
  SvdField missing(nullptr);
  SvdField target(nullptr);
  ASSERT_TRUE(target.CopyItem(&missing));
  EXPECT_EQ(UINT64_MAX, target.GetOffset());
  EXPECT_EQ(UINT32_MAX, target.GetLsb());
  EXPECT_EQ(UINT32_MAX, target.GetMsb());
  EXPECT_EQ(-1, target.GetBitWidth());

  SvdField source(nullptr);
  source.SetOffset(0);
  source.SetLsb(0);
  source.SetMsb(0);
  source.SetBitWidth(1);
  ASSERT_TRUE(target.CopyItem(&source));
  ASSERT_TRUE(target.CopyItem(&missing));

  EXPECT_EQ(0U, target.GetOffset());
  EXPECT_EQ(0U, target.GetLsb());
  EXPECT_EQ(0U, target.GetMsb());
  EXPECT_EQ(1, target.GetBitWidth());
}

TEST_F(SvdSentinelModelTest, FieldValidationRequiresOffsetAndWidthIndependently) {
  for(unsigned present = 0; present < 4; present++) {
    SCOPED_TRACE(present);
    m_messages.clear();
    SvdField field(nullptr);
    SetIdentity(field);
    if(present & 1) {
      field.SetOffset(0);
    }
    if(present & 2) {
      field.SetBitWidth(1);
    }

    ASSERT_TRUE(field.Calculate());
    ASSERT_TRUE(field.CheckItem());

    EXPECT_EQ(present == 3, field.IsValid());
    EXPECT_EQ(present != 3, HasMessage("M311"));
    EXPECT_FALSE(HasMessage("M309"));
    EXPECT_FALSE(HasMessage("M310"));
    EXPECT_EQ(0U, field.GetOffset());
    EXPECT_EQ(1, field.GetBitWidth());
  }
}

TEST_F(SvdSentinelModelTest, AddressBlockCopyInheritsUnsetValuesAndPreservesIndependentOverrides) {
  SvdAddressBlock source(nullptr);
  source.SetOffset(0x40);
  source.SetSize(0x20);
  source.SetUsage(SvdTypes::AddrBlockUsage::REGISTERS);
  for(unsigned overrides = 0; overrides < 4; overrides++) {
    for(const uint32_t value : {0U, 4U, UINT32_MAX - 1}) {
      SCOPED_TRACE(overrides);
      SCOPED_TRACE(value);
      SvdAddressBlock target(nullptr);
      if(overrides & 1) {
        target.SetOffset(value);
      }
      if(overrides & 2) {
        target.SetSize(value);
      }

      ASSERT_TRUE(target.CopyItem(&source));

      EXPECT_EQ((overrides & 1) ? value : 0x40U, target.GetOffset());
      EXPECT_EQ((overrides & 2) ? value : 0x20U, target.GetSize());
      EXPECT_EQ(SvdTypes::AddrBlockUsage::REGISTERS, target.GetUsage());
      EXPECT_TRUE(target.IsCopied());
    }
  }
}

TEST_F(SvdSentinelModelTest, AddressBlockMissingDonorValuesRemainAvailableForLaterInheritance) {
  SvdAddressBlock missing(nullptr);
  SvdAddressBlock target(nullptr);
  ASSERT_TRUE(target.CopyItem(&missing));
  EXPECT_EQ(UINT32_MAX, target.GetOffset());
  EXPECT_EQ(UINT32_MAX, target.GetSize());

  SvdAddressBlock source(nullptr);
  source.SetOffset(0);
  source.SetSize(4);
  source.SetUsage(SvdTypes::AddrBlockUsage::REGISTERS);
  ASSERT_TRUE(target.CopyItem(&source));
  ASSERT_TRUE(target.CopyItem(&missing));
  ASSERT_TRUE(target.CheckItem());

  EXPECT_EQ(0U, target.GetOffset());
  EXPECT_EQ(4U, target.GetSize());
  EXPECT_TRUE(target.IsValid());
  EXPECT_FALSE(HasMessage("M314"));
}

TEST_F(SvdSentinelModelTest, AddressBlockValidationReportsMissingValuesInheritedFromDonor) {
  for(unsigned present = 0; present < 4; present++) {
    SCOPED_TRACE(present);
    m_messages.clear();
    SvdAddressBlock source(nullptr);
    source.SetUsage(SvdTypes::AddrBlockUsage::REGISTERS);
    if(present & 1) {
      source.SetOffset(0);
    }
    if(present & 2) {
      source.SetSize(4);
    }
    SvdAddressBlock target(nullptr);

    ASSERT_TRUE(target.CopyItem(&source));
    ASSERT_TRUE(target.CheckItem());

    EXPECT_EQ(present == 3, target.IsValid());
    EXPECT_EQ(present != 3, HasMessage("M314"));
    EXPECT_FALSE(HasMessage("M315"));
    EXPECT_FALSE(HasMessage("M359"));
  }
}

} // namespace
