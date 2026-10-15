/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "SvdItem.h"
#include "SvdCluster.h"
#include "SvdDevice.h"
#include "SvdDimension.h"
#include "SvdInterrupt.h"
#include "SvdPeripheral.h"
#include "SvdRegister.h"
#include "ErrLog.h"
#include "XMLTree.h"

#include "gtest/gtest.h"
#include <algorithm>
#include <limits>
#include <string>
#include <vector>

using namespace std;

namespace {

class SvdDimensionBoundsTest : public testing::Test, public IErrConsumer {
protected:
  void SetUp() override {
    auto log = ErrLog::Get();
    m_previousConsumer = log->SetErrConsumer(this);
    m_previousQuiet = log->IsQuietMode();
    m_previousErrors = log->GetErrCnt();
    m_previousWarnings = log->GetWarnCnt();
    log->SetQuietMode(false);
    log->ResetMsgCount();
  }

  void TearDown() override {
    auto log = ErrLog::Get();
    log->SetErrConsumer(m_previousConsumer);
    log->SetQuietMode(m_previousQuiet);
    log->ResetMsgCount();
    for(int i = 0; i < m_previousErrors; i++) { log->IncErrCnt(); }
    for(int i = 0; i < m_previousWarnings; i++) { log->IncWarnCnt(); }
  }

  bool Consume(const PdscMsg& msg, const string&) override {
    m_messages.push_back(msg.GetMsgNum());
    return true;
  }

  static SvdDimension* Dimension(SvdItem& item, uint32_t count, uint32_t increment = 4,
                                const string& indices = "") {
    item.SetName("ITEM%s");
    auto dim = new SvdDimension(&item);
    item.SetDimension(dim);
    dim->SetDim(count);
    dim->SetDimIncrement(increment);
    dim->SetDimIndex(indices);
    return dim;
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

TEST_F(SvdDimensionBoundsTest, NumericRangesIncludeMaximumEndpointsWithoutOverflow) {
  for(const auto end : {uint32_t(INT32_MAX), UINT32_MAX}) {
    for(const uint32_t count : {1U, 2U}) {
      SCOPED_TRACE(end);
      SCOPED_TRACE(count);
      SvdRegister reg(nullptr);
      const auto first = end - count + 1;
      const auto dim = Dimension(reg, count, 4, to_string(first) + "-" + to_string(end));
      ASSERT_TRUE(dim->CalculateDim());
      ASSERT_EQ(count, dim->GetDimIndexList().size());
      EXPECT_EQ(to_string(first), dim->GetDimIndexList().front());
      EXPECT_EQ(to_string(end), dim->GetDimIndexList().back());
      EXPECT_FALSE(reg.HasExpansionError());
    }
  }
}

TEST_F(SvdDimensionBoundsTest, InvalidAndOversizedCountsDoNotAllocateIndices) {
  for(const uint32_t count : {0U, UINT32_MAX, uint32_t(SvdItem::MAX_EXPANSION_ITEMS + 1)}) {
    SCOPED_TRACE(count);
    SvdRegister reg(nullptr);
    const auto dim = Dimension(reg, count);
    EXPECT_FALSE(reg.CalculateDim());
    EXPECT_TRUE(dim->GetDimIndexList().empty());
    EXPECT_TRUE(dim->GetChildren().empty());
    EXPECT_FALSE(reg.IsValid());
  }
}

TEST_F(SvdDimensionBoundsTest, OversizedExplicitListIsStoppedBeforeExpansion) {
  SvdRegister reg(nullptr);
  string indices;
  for(size_t i = 0; i <= SvdItem::MAX_EXPANSION_ITEMS; i++) { indices += "A,"; }
  const auto dim = Dimension(reg, 2, 4, indices);
  EXPECT_FALSE(reg.CalculateDim());
  EXPECT_TRUE(reg.HasExpansionError());
  EXPECT_TRUE(dim->GetDimIndexList().empty());
  EXPECT_TRUE(dim->GetChildren().empty());
  EXPECT_EQ(1, count(m_messages.begin(), m_messages.end(), "M249"));
}

TEST_F(SvdDimensionBoundsTest, IndexBudgetIsSharedBySiblingDimensions) {
  SvdItem root(nullptr);
  SvdRegister first(&root), second(&root), third(&root);
  const auto half = uint32_t(SvdItem::MAX_EXPANSION_ITEMS / 2);
  ASSERT_TRUE(Dimension(first, half)->CalculateDimIndex());
  ASSERT_TRUE(Dimension(second, half)->CalculateDimIndex());
  EXPECT_FALSE(root.HasExpansionError());
  const auto last = Dimension(third, 1);
  EXPECT_FALSE(last->CalculateDimIndex());
  EXPECT_TRUE(last->GetDimIndexList().empty());
  EXPECT_TRUE(first.HasExpansionError());
  EXPECT_TRUE(root.HasExpansionError());
}

TEST_F(SvdDimensionBoundsTest, RecalculationDoesNotResetBudgetAndNewInputIsIndependent) {
  SvdRegister reg(nullptr);
  const auto dim = Dimension(reg, uint32_t(SvdItem::MAX_EXPANSION_ITEMS / 2 + 1));
  ASSERT_TRUE(dim->CalculateDimIndex());
  EXPECT_FALSE(dim->CalculateDimIndex());
  EXPECT_TRUE(reg.HasExpansionError());
  EXPECT_TRUE(dim->GetDimIndexList().empty());

  SvdRegister nextInput(nullptr);
  const auto nextDim = Dimension(nextInput, 2);
  EXPECT_TRUE(nextDim->CalculateDimIndex());
  EXPECT_EQ(2U, nextDim->GetDimIndexList().size());
  EXPECT_FALSE(nextInput.HasExpansionError());
}

TEST_F(SvdDimensionBoundsTest, NestedDimensionsShareCopiedDescendantBudget) {
  XMLTreeElement element(nullptr, "cluster");
  element.CreateElement("name", "CLUSTER[%s]");
  element.CreateElement("description", "Cluster array.");
  element.CreateElement("addressOffset", "0");
  element.CreateElement("dim", "1000");
  element.CreateElement("dimIncrement", "4096");
  auto reg = element.CreateElement("register");
  reg->CreateElement("name", "REG[%s]");
  reg->CreateElement("description", "Register array.");
  reg->CreateElement("addressOffset", "0");
  reg->CreateElement("size", "32");
  reg->CreateElement("access", "read-write");
  reg->CreateElement("dim", "1000");
  reg->CreateElement("dimIncrement", "4");
  SvdCluster cluster(nullptr);

  EXPECT_FALSE(cluster.Construct(&element));
  EXPECT_TRUE(cluster.HasExpansionError());
  ASSERT_NE(nullptr, cluster.GetDimension());
  EXPECT_LT(cluster.GetDimension()->GetChildCount(), 1000U);
  EXPECT_EQ(1, count(m_messages.begin(), m_messages.end(), "M249"));
}

TEST_F(SvdDimensionBoundsTest, IncrementMultiplicationUsesWideIntermediate) {
  SvdDevice device(nullptr);
  XMLTreeElement unitBits(nullptr, "addressUnitBits");
  unitBits.SetText("32");
  ASSERT_TRUE(device.ProcessXmlElement(&unitBits));
  SvdRegister reg(&device);
  const auto dim = Dimension(reg, 2, UINT32_MAX - 1);
  EXPECT_EQ(uint64_t(UINT32_MAX - 1) * 4, dim->CalcAddressIncrement());
}

TEST_F(SvdDimensionBoundsTest, RegisterStrideValidationDoesNotWrapProducts) {
  SvdDevice device(nullptr);
  XMLTreeElement unitBits(nullptr, "addressUnitBits");
  unitBits.SetText("8");
  ASSERT_TRUE(device.ProcessXmlElement(&unitBits));

  SvdRegister extended(&device);
  extended.SetBitWidth(32);
  extended.SetOffset(0);
  ASSERT_TRUE(Dimension(extended, 2, 0x20000000)->CalculateDim());
  extended.CheckItem();
  EXPECT_TRUE(extended.IsValid());
  EXPECT_FALSE(HasMessage("M366"));

  SvdRegister array(&device);
  array.SetBitWidth(32);
  array.SetOffset(0);
  const auto dim = Dimension(array, 2, 0x20000004);
  array.SetName("ITEM[%s]");
  ASSERT_TRUE(dim->CalculateDim());
  array.CheckItem();
  EXPECT_FALSE(array.IsValid());
  EXPECT_TRUE(HasMessage("M378"));
}

TEST_F(SvdDimensionBoundsTest, LastAddressIsCheckedBeforeGeneratingCopies) {
  for(const uint64_t start : {UINT64_MAX - 5, UINT64_MAX - 3}) {
    SvdRegister reg(nullptr);
    reg.SetBitWidth(32);
    reg.SetAccess(SvdTypes::Access::READWRITE);
    reg.SetOffset(start);
    const auto dim = Dimension(reg, 2, 4);
    const bool fits = start == UINT64_MAX - 5;
    EXPECT_EQ(fits, reg.CalculateDim());
    EXPECT_EQ(fits ? 2U : 0U, dim->GetChildCount());
    if(fits) {
      const auto last = dynamic_cast<SvdRegister*>(dim->GetChildren().back());
      ASSERT_NE(nullptr, last);
      EXPECT_EQ(UINT64_MAX - 1, last->GetOffset());
      EXPECT_TRUE(last->IsValid());
    }
    else {
      EXPECT_FALSE(reg.IsValid());
      EXPECT_TRUE(HasMessage("M250"));
    }
  }
}

TEST_F(SvdDimensionBoundsTest, ClusterAndPeripheralAddressWrapIsRejected) {
  SvdCluster cluster(nullptr);
  cluster.SetOffset(UINT64_MAX - 1);
  const auto clusterDim = Dimension(cluster, 2, 4);
  EXPECT_FALSE(cluster.CalculateDim());
  EXPECT_TRUE(clusterDim->GetChildren().empty());

  SvdPeripheral peripheral(nullptr);
  peripheral.SetAddress(UINT64_MAX - 1);
  const auto peripheralDim = Dimension(peripheral, 2, 4);
  EXPECT_FALSE(peripheral.CalculateDim());
  EXPECT_TRUE(peripheralDim->GetChildren().empty());
}

TEST_F(SvdDimensionBoundsTest, InterruptDimensionCannotWrapItsValue) {
  SvdPeripheral peripheral(nullptr);
  Dimension(peripheral, 3, 4);
  SvdInterrupt irq(&peripheral);
  irq.SetName("IRQ%s");
  irq.SetValue(UINT32_MAX - 2);
  EXPECT_FALSE(irq.CalculateDim());
  EXPECT_FALSE(irq.IsValid());
  ASSERT_NE(nullptr, irq.GetDimension());
  EXPECT_TRUE(irq.GetDimension()->GetChildren().empty());
}

} // namespace
