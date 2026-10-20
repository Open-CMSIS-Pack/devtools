/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "SvdItem.h"
#include "SvdAddressBlock.h"
#include "SvdCluster.h"
#include "SvdDerivedFrom.h"
#include "SvdDevice.h"
#include "SvdDimension.h"
#include "SvdEnum.h"
#include "SvdField.h"
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

  static SvdAddressBlock* AddressBlock(SvdPeripheral& peripheral, uint32_t offset = 0) {
    const auto block = new SvdAddressBlock(&peripheral);
    block->SetOffset(offset);
    block->SetSize(0x100);
    block->SetUsage(SvdTypes::AddrBlockUsage::REGISTERS);
    peripheral.AddAddressBlock(block);
    return block;
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
  for(size_t i = 0; i <= SvdItem::MAX_EXPANSION_ITEMS; i++) {
    indices += "A,";
  }
  const auto dim = Dimension(reg, 2, 4, indices);
  EXPECT_FALSE(reg.CalculateDim());
  EXPECT_TRUE(reg.HasExpansionError());
  EXPECT_TRUE(dim->GetDimIndexList().empty());
  EXPECT_TRUE(dim->GetChildren().empty());
  EXPECT_EQ(1, count(m_messages.begin(), m_messages.end(), "M249"));
}

TEST_F(SvdDimensionBoundsTest, FinalExplicitIndexCannotExceedTheSharedBudget) {
  SvdRegister reg(nullptr);
  ASSERT_TRUE(reg.ReserveExpansion(SvdItem::MAX_EXPANSION_ITEMS - 1));
  const auto dim = Dimension(reg, 2, 4, "A,B");

  EXPECT_FALSE(reg.CalculateDim());
  EXPECT_TRUE(reg.HasExpansionError());
  EXPECT_TRUE(dim->GetDimIndexList().empty());
  EXPECT_TRUE(dim->GetChildren().empty());
  EXPECT_EQ(1, count(m_messages.begin(), m_messages.end(), "M249"));
}

TEST_F(SvdDimensionBoundsTest, DirectRangeCalculationRejectsInvalidCountsBeforeAllocation) {
  for(const uint32_t count : {0U, UINT32_MAX, uint32_t(SvdItem::MAX_EXPANSION_ITEMS + 1)}) {
    SCOPED_TRACE(count);
    SvdRegister reg(nullptr);
    const auto dim = Dimension(reg, count);

    EXPECT_FALSE(dim->CalculateDimIndexFromTo());
    EXPECT_TRUE(dim->GetDimIndexList().empty());
    EXPECT_EQ(count == SvdItem::MAX_EXPANSION_ITEMS + 1, reg.HasExpansionError());
  }
}

TEST_F(SvdDimensionBoundsTest, AlphabeticRangeIsReservedBeforeAnyIndexIsAdded) {
  for(const size_t remaining : {size_t{1}, size_t{2}}) {
    SCOPED_TRACE(remaining);
    SvdRegister reg(nullptr);
    ASSERT_TRUE(reg.ReserveExpansion(SvdItem::MAX_EXPANSION_ITEMS - remaining));
    const auto dim = Dimension(reg, 2, 4, "A-B");

    EXPECT_EQ(remaining == 2, dim->CalculateDim());
    EXPECT_EQ(remaining == 1, reg.HasExpansionError());
    if(remaining == 2) {
      ASSERT_EQ(2U, dim->GetDimIndexList().size());
      EXPECT_EQ("A", dim->GetDimIndexList().front());
      EXPECT_EQ("B", dim->GetDimIndexList().back());
    }
    else {
      EXPECT_TRUE(dim->GetDimIndexList().empty());
    }
  }
}

TEST_F(SvdDimensionBoundsTest, ExhaustedBudgetRemainsStickyAcrossDescendants) {
  SvdItem root(nullptr);
  SvdRegister reg(&root);
  ASSERT_TRUE(root.ReserveExpansion(SvdItem::MAX_EXPANSION_ITEMS));
  EXPECT_FALSE(reg.ReserveExpansion(1));
  EXPECT_FALSE(root.ReserveExpansion(0));
  EXPECT_FALSE(reg.ReserveExpansion(1));
  EXPECT_TRUE(root.HasExpansionError());
  EXPECT_TRUE(reg.HasExpansionError());
  EXPECT_EQ(1, count(m_messages.begin(), m_messages.end(), "M249"));
}

TEST_F(SvdDimensionBoundsTest, InheritedMetadataMustFitTheDestinationBudget) {
  SvdItem source(nullptr);
  Dimension(source, 2);
  source.SetDerivedFrom(new SvdDerivedFrom(&source));

  for(const size_t remaining : {size_t{0}, size_t{1}, size_t{2}}) {
    SCOPED_TRACE(remaining);
    SvdItem target(nullptr);
    ASSERT_TRUE(target.ReserveExpansion(SvdItem::MAX_EXPANSION_ITEMS - remaining));

    EXPECT_EQ(remaining == 2, target.CopyItem(&source));
    EXPECT_EQ(remaining < 2, target.HasExpansionError());
    if(remaining > 0) {
      ASSERT_NE(nullptr, target.GetDerivedFrom());
      EXPECT_EQ(&source, target.GetDerivedFrom()->GetDerivedFromItem());
    }
    else {
      EXPECT_EQ(nullptr, target.GetDerivedFrom());
    }
    if(remaining == 2) {
      ASSERT_NE(nullptr, target.GetDimension());
      EXPECT_EQ(2U, target.GetDimension()->GetDim());
      EXPECT_EQ(&source, target.GetCopiedFrom());
    }
    else {
      EXPECT_EQ(nullptr, target.GetDimension());
      EXPECT_EQ(nullptr, target.GetCopiedFrom());
    }
  }
}

TEST_F(SvdDimensionBoundsTest, CopiedPeripheralDescendantsShareOneBudget) {
  SvdPeripheralContainer source(nullptr);
  const auto peripheral = new SvdPeripheral(&source);
  source.AddItem(peripheral);
  peripheral->SetName("PERIPHERAL");
  peripheral->SetAddress(0x40000000);
  const auto registers = new SvdRegisterContainer(peripheral);
  peripheral->AddItem(registers);
  const auto cluster = new SvdCluster(registers);
  registers->AddItem(cluster);
  cluster->SetName("GROUP");
  cluster->SetOffset(0);
  const auto reg = new SvdRegister(cluster);
  cluster->AddItem(reg);
  reg->SetName("DATA");
  reg->SetOffset(4);
  reg->SetBitWidth(32);

  for(const size_t remaining : {size_t{3}, size_t{4}}) {
    SCOPED_TRACE(remaining);
    SvdPeripheralContainer target(nullptr);
    ASSERT_TRUE(target.ReserveExpansion(SvdItem::MAX_EXPANSION_ITEMS - remaining));

    EXPECT_EQ(remaining == 4, target.CopyChilds(&source, &target));
    EXPECT_EQ(remaining == 3, target.HasExpansionError());
    ASSERT_EQ(1U, target.GetChildCount());
    const auto copiedPeripheral = target.GetChildren().front();
    ASSERT_EQ(1U, copiedPeripheral->GetChildCount());
    const auto copiedRegisters = copiedPeripheral->GetChildren().front();
    ASSERT_EQ(1U, copiedRegisters->GetChildCount());
    const auto copiedCluster = copiedRegisters->GetChildren().front();
    if(remaining == 4) {
      ASSERT_EQ(1U, copiedCluster->GetChildCount());
      const auto copiedRegister = dynamic_cast<SvdRegister*>(copiedCluster->GetChildren().front());
      ASSERT_NE(nullptr, copiedRegister);
      EXPECT_EQ("DATA", copiedRegister->GetName());
      EXPECT_EQ(4U, copiedRegister->GetOffset());
      EXPECT_EQ(32U, copiedRegister->GetEffectiveBitWidth());
      EXPECT_EQ(0x40000004U, copiedRegister->GetAbsoluteAddress());
    }
    else {
      EXPECT_TRUE(copiedCluster->GetChildren().empty());
    }
  }
}

TEST_F(SvdDimensionBoundsTest, DerivationStopsAtChildAndDimensionMetadataBudgetBoundaries) {
  SvdItem source(nullptr);
  Dimension(source, 2)->SetDimName("SOURCE");
  const auto reg = new SvdRegister(&source);
  source.AddItem(reg);
  reg->SetName("DATA");
  reg->SetOffset(4);
  reg->SetBitWidth(32);

  for(const size_t remaining : {size_t{0}, size_t{1}, size_t{2}}) {
    SCOPED_TRACE(remaining);
    SvdItem target(nullptr);
    const auto derived = new SvdDerivedFrom(&target);
    target.SetDerivedFrom(derived);
    ASSERT_TRUE(target.ReserveExpansion(SvdItem::MAX_EXPANSION_ITEMS - remaining));

    EXPECT_EQ(remaining == 2, derived->DeriveItem(&source));
    EXPECT_EQ(remaining < 2, target.HasExpansionError());
    EXPECT_EQ(&source, derived->GetDerivedFromItem());
    ASSERT_EQ(remaining > 0 ? 1U : 0U, target.GetChildCount());
    if(remaining == 2) {
      ASSERT_NE(nullptr, target.GetDimension());
      EXPECT_EQ(2U, target.GetDimension()->GetDim());
      EXPECT_TRUE(target.GetDimension()->GetDimName().empty());
      EXPECT_EQ(&source, target.GetCopiedFrom());
    }
    else {
      EXPECT_EQ(nullptr, target.GetDimension());
      EXPECT_EQ(nullptr, target.GetCopiedFrom());
    }
    if(remaining > 0) {
      EXPECT_EQ("DATA", target.GetChildren().front()->GetName());
      EXPECT_EQ(reg, target.GetChildren().front()->GetCopiedFrom());
    }
  }
}

TEST_F(SvdDimensionBoundsTest, CopiedFieldsAndRegistersPropagateDescendantBudgetFailures) {
  for(const bool isField : {false, true}) {
    SCOPED_TRACE(isField);
    SvdItem source(nullptr);
    SvdItem* child = nullptr;
    if(isField) {
      child = new SvdField(&source);
      child->AddItem(new SvdEnumContainer(child));
    }
    else {
      child = new SvdRegister(&source);
      child->AddItem(new SvdFieldContainer(child));
    }
    source.AddItem(child);
    child->SetName("DATA");
    child->SetBitWidth(isField ? 1 : 32);

    for(const size_t remaining : {size_t{1}, size_t{2}}) {
      SCOPED_TRACE(remaining);
      SvdItem target(nullptr);
      ASSERT_TRUE(target.ReserveExpansion(SvdItem::MAX_EXPANSION_ITEMS - remaining));

      EXPECT_EQ(remaining == 2, target.CopyChilds(&source, &target));
      EXPECT_EQ(remaining == 1, target.HasExpansionError());
      ASSERT_EQ(1U, target.GetChildCount());
      const auto copy = target.GetChildren().front();
      EXPECT_EQ(child->GetSvdLevel(), copy->GetSvdLevel());
      ASSERT_EQ(remaining - 1, copy->GetChildCount());
      EXPECT_EQ(remaining == 2 ? child : nullptr, copy->GetCopiedFrom());
      if(remaining == 2) {
        EXPECT_EQ("DATA", copy->GetName());
        EXPECT_EQ(child->GetChildren().front(), copy->GetChildren().front()->GetCopiedFrom());
      }
    }
  }
}

TEST_F(SvdDimensionBoundsTest, DirectlyBuiltEnumLeafSubtreesStillRespectTheCopyBudget) {
  // Direct model callers can attach children to leaf elements outside the XML schema.
  SvdField sourceField(nullptr);
  SvdEnumContainer source(&sourceField);
  const auto value = new SvdEnum(&source);
  source.AddItem(value);
  value->SetName("OUTER");
  value->SetValue(1);
  const auto nested = new SvdEnum(value);
  value->AddItem(nested);
  nested->SetName("INNER");
  nested->SetValue(2);

  for(const size_t remaining : {size_t{1}, size_t{2}}) {
    SCOPED_TRACE(remaining);
    SvdField targetField(nullptr);
    SvdEnumContainer target(&targetField);
    ASSERT_TRUE(target.ReserveExpansion(SvdItem::MAX_EXPANSION_ITEMS - remaining));

    EXPECT_EQ(remaining == 2, target.CopyChilds(&source, &target));
    EXPECT_EQ(remaining == 1, target.HasExpansionError());
    ASSERT_EQ(1U, target.GetChildCount());
    const auto copy = target.GetChildren().front();
    ASSERT_EQ(remaining - 1, copy->GetChildCount());
    EXPECT_EQ(remaining == 2 ? value : nullptr, copy->GetCopiedFrom());
    if(remaining == 2) {
      EXPECT_EQ("INNER", copy->GetChildren().front()->GetName());
      EXPECT_EQ(nested, copy->GetChildren().front()->GetCopiedFrom());
    }
  }
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

TEST_F(SvdDimensionBoundsTest, FieldDimensionsRejectInvalidCountsAndAddressWrapBeforeCopying) {
  for(const bool invalidCount : {false, true}) {
    SCOPED_TRACE(invalidCount);
    SvdField field(nullptr);
    field.SetOffset(invalidCount ? 0 : UINT64_MAX - 1);
    field.SetBitWidth(1);
    const auto dim = Dimension(field, invalidCount ? 0 : 2, 4);

    EXPECT_FALSE(field.CalculateDim());
    EXPECT_FALSE(field.IsValid());
    EXPECT_TRUE(dim->GetChildren().empty());
    EXPECT_FALSE(field.HasExpansionError());
  }
}

TEST_F(SvdDimensionBoundsTest, FieldAndRegisterExpansionStopsWhenInheritedMetadataCannotBeCopied) {
  for(const bool isField : {false, true}) {
    SCOPED_TRACE(isField);
    SvdRegister reg(nullptr);
    reg.SetBitWidth(32);
    reg.SetOffset(0);
    SvdFieldContainer fields(&reg);
    SvdField field(&fields);
    field.SetOffset(0);
    field.SetBitWidth(1);
    SvdItem& fieldItem = field;
    SvdItem& registerItem = reg;
    SvdItem& source = isField ? fieldItem : registerItem;
    const auto dim = Dimension(source, 2, isField ? 1 : 4);
    source.SetDerivedFrom(new SvdDerivedFrom(&source));
    // Two indices and one copied item fit; its inherited metadata does not.
    ASSERT_TRUE(source.ReserveExpansion(SvdItem::MAX_EXPANSION_ITEMS - 3));

    EXPECT_FALSE(source.CalculateDim());
    EXPECT_TRUE(source.HasExpansionError());
    ASSERT_EQ(1U, dim->GetChildCount());
    EXPECT_EQ(nullptr, dim->GetChildren().front()->GetDerivedFrom());
    EXPECT_EQ(nullptr, dim->GetChildren().front()->GetCopiedFrom());
  }
}

TEST_F(SvdDimensionBoundsTest, ClusterAndPeripheralExpansionCheckInheritedMetadataBudget) {
  for(const size_t remaining : {size_t{3}, size_t{6}}) {
    SCOPED_TRACE(remaining);
    SvdCluster cluster(nullptr);
    cluster.SetOffset(0);
    SvdPeripheral peripheral(nullptr);
    peripheral.SetAddress(0x40000000);
    SvdItem* sources[] = {&cluster, &peripheral};

    for(const auto source : sources) {
      SCOPED_TRACE(source->GetSvdLevel());
      const auto dim = Dimension(*source, 2);
      source->SetDerivedFrom(new SvdDerivedFrom(source));
      // Each copy needs one item and one inherited metadata object, after the two indices.
      ASSERT_TRUE(source->ReserveExpansion(SvdItem::MAX_EXPANSION_ITEMS - remaining));

      EXPECT_EQ(remaining == 6, source->CalculateDim());
      EXPECT_EQ(remaining == 3, source->HasExpansionError());
      ASSERT_EQ(remaining == 6 ? 2U : 1U, dim->GetChildCount());
      const auto first = dim->GetChildren().front();
      if(remaining == 6) {
        for(const auto copy : dim->GetChildren()) {
          ASSERT_NE(nullptr, copy->GetDerivedFrom());
          EXPECT_EQ(source, copy->GetDerivedFrom()->GetDerivedFromItem());
          EXPECT_EQ(source, copy->GetCopiedFrom());
        }
        EXPECT_EQ("ITEM0", first->GetName());
        EXPECT_EQ("ITEM1", dim->GetChildren().back()->GetName());
      }
      else {
        EXPECT_EQ(nullptr, first->GetDerivedFrom());
        EXPECT_EQ(nullptr, first->GetCopiedFrom());
      }
    }
  }
}

TEST_F(SvdDimensionBoundsTest, PeripheralDimensionStopsWhenAddressBlocksCannotBeCopied) {
  for(const size_t remaining : {size_t{3}, size_t{6}}) {
    SCOPED_TRACE(remaining);
    SvdPeripheral peripheral(nullptr);
    peripheral.SetAddress(0x40000000);
    const auto block = AddressBlock(peripheral, 0x20);
    const auto dim = Dimension(peripheral, 2, 0x1000);
    ASSERT_TRUE(peripheral.ReserveExpansion(SvdItem::MAX_EXPANSION_ITEMS - remaining));

    EXPECT_EQ(remaining == 6, peripheral.CalculateDim());
    EXPECT_EQ(remaining == 3, peripheral.HasExpansionError());
    ASSERT_EQ(remaining == 6 ? 2U : 1U, dim->GetChildCount());
    for(const auto child : dim->GetChildren()) {
      const auto copy = dynamic_cast<SvdPeripheral*>(child);
      ASSERT_NE(nullptr, copy);
      if(remaining == 6) {
        ASSERT_EQ(1U, copy->GetAddressBlock().size());
        const auto copiedBlock = copy->GetAddressBlock().front();
        EXPECT_NE(block, copiedBlock);
        EXPECT_EQ(0x20U, copiedBlock->GetOffset());
        EXPECT_EQ(0x100U, copiedBlock->GetSize());
        EXPECT_EQ(block, copiedBlock->GetCopiedFrom());
        EXPECT_EQ(&peripheral, copy->GetCopiedFrom());
      }
      else {
        EXPECT_TRUE(copy->GetAddressBlock().empty());
        EXPECT_EQ(nullptr, copy->GetCopiedFrom());
      }
    }
    EXPECT_EQ(1U, peripheral.GetAddressBlock().size());
  }
}

TEST_F(SvdDimensionBoundsTest, PeripheralCopyPropagatesDisableConditionBudgetFailure) {
  SvdCExpression expression;
  expression.SetText("CONTROL.ENABLE == 0");
  SvdPeripheral source(nullptr);
  source.SetName("PERIPHERAL");
  ASSERT_TRUE(source.CopyDisableCondition(&expression));

  for(const size_t remaining : {size_t{0}, size_t{1}}) {
    SCOPED_TRACE(remaining);
    SvdPeripheral target(nullptr);
    ASSERT_TRUE(target.ReserveExpansion(SvdItem::MAX_EXPANSION_ITEMS - remaining));

    target.CopyItem(&source);
    EXPECT_EQ(remaining == 0, target.HasExpansionError());
    if(remaining == 1) {
      ASSERT_NE(nullptr, target.GetDisableCondition());
      EXPECT_NE(source.GetDisableCondition(), target.GetDisableCondition());
      EXPECT_EQ(expression.GetText(), target.GetDisableCondition()->GetText());
      EXPECT_EQ(&source, target.GetCopiedFrom());
      EXPECT_EQ("PERIPHERAL", target.GetName());
    }
    else {
      EXPECT_EQ(nullptr, target.GetDisableCondition());
      EXPECT_EQ(nullptr, target.GetCopiedFrom());
    }
  }
}

TEST_F(SvdDimensionBoundsTest, AddressBlockSortingStopsAtTheSharedBudgetBoundary) {
  for(const size_t remaining : {size_t{1}, size_t{2}}) {
    SCOPED_TRACE(remaining);
    SvdPeripheral peripheral(nullptr);
    const auto upper = AddressBlock(peripheral, 0x200);
    const auto lower = AddressBlock(peripheral, 0);
    ASSERT_TRUE(peripheral.ReserveExpansion(SvdItem::MAX_EXPANSION_ITEMS - remaining));
    map<uint64_t, SvdAddressBlock*> sorted;

    EXPECT_EQ(remaining == 2, peripheral.SortAddressBlocks(sorted));
    EXPECT_EQ(remaining == 1, peripheral.HasExpansionError());
    EXPECT_EQ(remaining, sorted.size());
    const auto upperCopy = sorted.find(0x200);
    EXPECT_NE(sorted.end(), upperCopy);
    if(upperCopy != sorted.end()) {
      EXPECT_NE(upper, upperCopy->second);
      EXPECT_EQ(upper, upperCopy->second->GetCopiedFrom());
    }
    if(remaining == 2 && !sorted.empty()) {
      EXPECT_EQ(0U, sorted.begin()->first);
      EXPECT_EQ(lower, sorted.begin()->second->GetCopiedFrom());
    }
    // The same cleanup used by MergeAddressBlocks releases unmerged temporary copies.
    EXPECT_TRUE(peripheral.CopyMergedAddressBlocks(sorted));
    EXPECT_EQ(2U, peripheral.GetAddressBlock().size());
  }
}

TEST_F(SvdDimensionBoundsTest, PeripheralExpansionPreservesAddressesAndStopsAtTheBudgetBoundary) {
  for(const size_t remaining : {size_t{3}, size_t{4}}) {
    SCOPED_TRACE(remaining);
    SvdPeripheral peripheral(nullptr);
    peripheral.SetAddress(0x40000000);
    const auto dim = Dimension(peripheral, 2, 0x1000);
    ASSERT_TRUE(peripheral.ReserveExpansion(SvdItem::MAX_EXPANSION_ITEMS - remaining));

    EXPECT_EQ(remaining == 4, peripheral.CalculateDim());
    EXPECT_EQ(remaining == 3, peripheral.HasExpansionError());
    ASSERT_EQ(remaining - 2, dim->GetChildCount());
    const auto first = dynamic_cast<SvdPeripheral*>(dim->GetChildren().front());
    ASSERT_NE(nullptr, first);
    EXPECT_EQ("ITEM0", first->GetName());
    EXPECT_EQ(0x40000000U, first->GetAddress());
    if(remaining == 4) {
      const auto last = dynamic_cast<SvdPeripheral*>(dim->GetChildren().back());
      ASSERT_NE(nullptr, last);
      EXPECT_EQ("ITEM1", last->GetName());
      EXPECT_EQ(0x40001000U, last->GetAddress());
    }
  }
}

TEST_F(SvdDimensionBoundsTest, InterruptDimensionPreservesTheLargestInitializedValue) {
  SvdPeripheral peripheral(nullptr);
  Dimension(peripheral, 2, 4);
  SvdInterrupt irq(&peripheral);
  irq.SetName("IRQ%s");
  irq.SetValue(UINT32_MAX - 2);

  ASSERT_TRUE(irq.CalculateDim());
  EXPECT_FALSE(irq.HasExpansionError());
  ASSERT_NE(nullptr, irq.GetDimension());
  const auto dim = irq.GetDimension();
  ASSERT_EQ(2U, dim->GetChildCount());
  const auto first = dynamic_cast<SvdInterrupt*>(dim->GetChildren().front());
  const auto last = dynamic_cast<SvdInterrupt*>(dim->GetChildren().back());
  ASSERT_NE(nullptr, first);
  ASSERT_NE(nullptr, last);
  EXPECT_EQ("IRQ0", first->GetName());
  EXPECT_EQ("IRQ1", last->GetName());
  EXPECT_EQ(UINT32_MAX - 2, first->GetValue());
  EXPECT_EQ(UINT32_MAX - 1, last->GetValue());
  EXPECT_FALSE(HasMessage("M250"));
}

TEST_F(SvdDimensionBoundsTest, InterruptDimensionAccountsForCopiedMetadataIndicesAndItems) {
  for(const size_t remaining : {size_t{0}, size_t{2}, size_t{3}, size_t{5}}) {
    SCOPED_TRACE(remaining);
    SvdPeripheral peripheral(nullptr);
    Dimension(peripheral, 2);
    SvdInterrupt irq(&peripheral);
    irq.SetName("IRQ%s");
    irq.SetValue(16);
    ASSERT_TRUE(peripheral.ReserveExpansion(SvdItem::MAX_EXPANSION_ITEMS - remaining));

    EXPECT_EQ(remaining == 5, irq.CalculateDim());
    EXPECT_EQ(remaining < 5, peripheral.HasExpansionError());
    const auto dim = irq.GetDimension();
    if(remaining == 0) {
      EXPECT_EQ(nullptr, dim);
      continue;
    }
    ASSERT_NE(nullptr, dim);
    EXPECT_EQ(remaining >= 3 ? 2U : 0U, dim->GetDimIndexList().size());
    ASSERT_EQ(remaining == 5 ? 2U : 0U, dim->GetChildCount());
    if(remaining == 5) {
      const auto last = dynamic_cast<SvdInterrupt*>(dim->GetChildren().back());
      ASSERT_NE(nullptr, last);
      EXPECT_EQ("IRQ1", last->GetName());
      EXPECT_EQ(17U, last->GetValue());
    }
  }
}

TEST_F(SvdDimensionBoundsTest, DirectlyBuiltInterruptLeafSubtreesStillRespectTheCopyBudget) {
  // The generic model API permits descendants even though interrupts are XML leaf elements.
  for(const size_t remaining : {size_t{3}, size_t{6}}) {
    SCOPED_TRACE(remaining);
    SvdInterrupt irq(nullptr);
    irq.SetValue(16);
    const auto dim = Dimension(irq, 2, 1);
    const auto value = new SvdEnum(&irq);
    irq.AddItem(value);
    value->SetName("CHILD");
    value->SetValue(1);
    ASSERT_TRUE(irq.ReserveExpansion(SvdItem::MAX_EXPANSION_ITEMS - remaining));

    EXPECT_EQ(remaining == 6, irq.CalculateDim());
    EXPECT_EQ(remaining == 3, irq.HasExpansionError());
    ASSERT_EQ(remaining == 6 ? 2U : 1U, dim->GetChildCount());
    uint32_t index = 0;
    for(const auto copy : dim->GetChildren()) {
      const auto copiedIrq = dynamic_cast<SvdInterrupt*>(copy);
      ASSERT_NE(nullptr, copiedIrq);
      ASSERT_EQ(remaining == 6 ? 1U : 0U, copy->GetChildCount());
      if(remaining == 6) {
        EXPECT_EQ("CHILD", copy->GetChildren().front()->GetName());
        EXPECT_EQ(value, copy->GetChildren().front()->GetCopiedFrom());
        EXPECT_EQ("ITEM" + to_string(index), copiedIrq->GetName());
        EXPECT_EQ(16U + index, copiedIrq->GetValue());
        EXPECT_EQ(index, copiedIrq->GetDimElementIndex());
      }
      else {
        EXPECT_TRUE(copiedIrq->GetName().empty());
        EXPECT_EQ(SvdItem::VALUE32_NOT_INIT, copiedIrq->GetValue());
        EXPECT_EQ(SvdItem::VALUE32_NOT_INIT, copiedIrq->GetDimElementIndex());
      }
      index++;
    }
  }
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
