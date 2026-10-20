/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "SvdItem.h"
#include "SvdCExpression.h"
#include "SvdDevice.h"
#include "SvdDimension.h"
#include "SvdEnum.h"
#include "SvdField.h"
#include "SvdPeripheral.h"
#include "SvdRegister.h"
#include "SvdUtils.h"
#include "HeaderData.h"
#include "SfdData.h"
#include "ErrLog.h"
#include "XMLTree.h"

#include "gtest/gtest.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace std;

namespace {

class BitWidthHeaderData : public HeaderData {
public:
  using HeaderData::HeaderData;
  using HeaderData::AddFields;
  using HeaderData::CreateSortedFields;
  using HeaderData::CreateFieldPosMask;
};

class BitWidthHeaderGenerator : public HeaderGenerator {
public:
  using HeaderGenerator::HeaderGenerator;
  using HeaderGenerator::MakeFieldPosMask3;
};

class BitWidthSfdData : public SfdData {
public:
  using SfdData::SfdData;
  using SfdData::CreateFieldItem;
};

class SvdBitWidthTest : public testing::Test, public IErrConsumer {
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
      filesystem::remove(m_outputDirectory / "masks.h", ec);
      filesystem::remove(m_outputDirectory / "device.sfd", ec);
      filesystem::remove(m_outputDirectory / "input.svd", ec);
      filesystem::remove(m_outputDirectory, ec);
    }
  }

  bool Consume(const PdscMsg& msg, const string&) override {
    m_messages.push_back(msg.GetMsgNum());
    return true;
  }

  bool HasMessage(const string& id) const {
    return find(m_messages.begin(), m_messages.end(), id) != m_messages.end();
  }

  void CreateOutputDirectory() {
    const auto nonce = chrono::steady_clock::now().time_since_epoch().count();
    for(unsigned i = 0; i < 100; i++) {
      const auto candidate = filesystem::temp_directory_path() /
        ("svdconv-bit-width-" + to_string(nonce) + "-" + to_string(i));
      if(filesystem::create_directory(candidate)) {
        m_outputDirectory = candidate;
        break;
      }
    }
    ASSERT_FALSE(m_outputDirectory.empty());
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

  static XMLTreeElement* ConfigureRegister(XMLTreeElement& element, const string& width) {
    element.CreateElement("name", "CTRL");
    element.CreateElement("description", "Control register.");
    element.CreateElement("size", width);
    element.CreateElement("addressOffset", "0");
    element.CreateElement("access", "read-write");
    return element.CreateElement("fields");
  }

  static XMLTreeElement* AddField(XMLTreeElement* fields, const string& name,
                                  const string& offset, const string& width) {
    const auto field = fields->CreateElement("field");
    field->CreateElement("name", name);
    field->CreateElement("description", "Control field.");
    field->CreateElement("bitOffset", offset);
    field->CreateElement("bitWidth", width);
    return field;
  }

  static void AddEnum(XMLTreeElement* field, const string& value) {
    const auto container = field->CreateElement("enumeratedValues");
    const auto entry = container->CreateElement("enumeratedValue");
    entry->CreateElement("name", "VALUE");
    entry->CreateElement("description", "Control value.");
    entry->CreateElement("value", value);
  }

  static SvdField* AddUncheckedField(SvdRegister& reg, uint64_t offset, int32_t width) {
    auto fields = reg.GetFieldContainer();
    if(!fields) {
      fields = new SvdFieldContainer(&reg);
      reg.AddItem(fields);
    }
    const auto field = new SvdField(fields);
    fields->AddItem(field);
    field->SetName("FIELD");
    field->SetOffset(offset);
    field->SetBitWidth(width);
    field->SetAccess(SvdTypes::Access::READWRITE);
    return field;
  }

  static pair<SvdPeripheral*, SvdRegister*> AddExpressionRegister(SvdDevice& device) {
    const auto peripherals = new SvdPeripheralContainer(&device);
    device.AddItem(peripherals);
    const auto peripheral = new SvdPeripheral(peripherals);
    peripherals->AddItem(peripheral);
    peripheral->SetName("TEST");
    const auto registers = new SvdRegisterContainer(peripheral);
    peripheral->AddItem(registers);
    const auto reg = new SvdRegister(registers);
    registers->AddItem(reg);
    reg->SetName("CTRL");
    reg->SetBitWidth(32);
    reg->SetOffset(0);
    reg->SetAccess(SvdTypes::Access::READWRITE);
    return {peripheral, reg};
  }

  vector<string> m_messages;
  filesystem::path m_outputDirectory;

private:
  IErrConsumer* m_previousConsumer = nullptr;
  bool m_previousQuiet = false;
  int m_previousErrors = 0;
  int m_previousWarnings = 0;
};

TEST_F(SvdBitWidthTest, BitMasksUseWideIntermediatesWithoutAcceptingInvalidRanges) {
  EXPECT_EQ(uint64_t{UINT32_MAX}, SvdUtils::CreateBitMask(32));
  EXPECT_EQ(uint64_t{1} << 31, SvdUtils::CreateBitMask(1, 31));
  // Exercise the arithmetic helper's existing 64-bit preparation, not SVD input support.
  EXPECT_EQ(UINT64_MAX, SvdUtils::CreateBitMask(64));
  EXPECT_EQ(uint64_t{1} << 63, SvdUtils::CreateBitMask(1, 63));
  EXPECT_EQ(0U, SvdUtils::CreateBitMask(2, 63));
  EXPECT_EQ(0U, SvdUtils::CreateBitMask(1, UINT64_MAX));
  const int64_t widths[] = {INT64_MIN, -1, 0, 65, int64_t{UINT32_MAX} + 2, INT64_MAX};
  for(const auto width : widths) {
    SCOPED_TRACE(width);
    EXPECT_EQ(0U, SvdUtils::CreateBitMask(width));
  }
}

TEST_F(SvdBitWidthTest, FullWidthFieldsPreserveEnumsAndAccessMasks) {
  constexpr int32_t width = 32;
  XMLTreeElement element(nullptr, "register");
  const auto fields = ConfigureRegister(element, to_string(width));
  const auto fieldElement = AddField(fields, "FULL", "0", to_string(width));
  fieldElement->CreateElement("access", "read-only");
  AddEnum(fieldElement, "4294967295");
  SvdRegister reg(nullptr);

  ASSERT_TRUE(reg.Construct(&element));
  ASSERT_TRUE(reg.IsValid());
  ASSERT_NE(nullptr, reg.GetFieldContainer());
  ASSERT_EQ(1U, reg.GetFieldContainer()->GetChildCount());
  const auto field = dynamic_cast<SvdField*>(reg.GetFieldContainer()->GetChildren().front());
  ASSERT_NE(nullptr, field);
  EXPECT_TRUE(field->IsValid());
  ASSERT_EQ(1U, field->GetEnumContainer().size());
  const auto enumContainer = field->GetEnumContainer().front();
  ASSERT_EQ(1U, enumContainer->GetChildCount());
  EXPECT_TRUE(enumContainer->GetChildren().front()->IsValid());
  EXPECT_FALSE(HasMessage("M335"));

  const uint64_t expectedMask = UINT32_MAX;
  EXPECT_EQ(expectedMask, reg.GetAccessMask());
  EXPECT_EQ(expectedMask, reg.GetAccessMaskRead());
  EXPECT_EQ(0U, reg.GetAccessMaskWrite());
  EXPECT_EQ(SvdTypes::Access::READONLY, reg.GetAccessCalculated());
}

TEST_F(SvdBitWidthTest, UpperBoundaryFieldsProduceOnlyTheirOwnWriteBit) {
  constexpr int32_t width = 32;
  XMLTreeElement element(nullptr, "register");
  const auto fields = ConfigureRegister(element, to_string(width));
  AddField(fields, "TOP", to_string(width - 1), "1");
  SvdRegister reg(nullptr);

  ASSERT_TRUE(reg.Construct(&element));
  ASSERT_TRUE(reg.IsValid());
  ASSERT_NE(nullptr, reg.GetFieldContainer());
  EXPECT_TRUE(reg.GetFieldContainer()->GetChildren().front()->IsValid());
  EXPECT_EQ(uint64_t{1} << (width - 1), reg.GetAccessMaskWrite());
  EXPECT_EQ(uint64_t{UINT32_MAX}, reg.GetAccessMaskRead());
}

TEST_F(SvdBitWidthTest, FullWidth32BitWriteMaskDoesNotSignExtend) {
  XMLTreeElement element(nullptr, "register");
  const auto fields = ConfigureRegister(element, "32");
  AddField(fields, "FULL", "0", "32");
  SvdRegister reg(nullptr);

  ASSERT_TRUE(reg.Construct(&element));
  ASSERT_TRUE(reg.IsValid());
  EXPECT_EQ(uint64_t{UINT32_MAX}, reg.GetAccessMask());
  EXPECT_EQ(uint64_t{UINT32_MAX}, reg.GetAccessMaskRead());
  EXPECT_EQ(uint64_t{UINT32_MAX}, reg.GetAccessMaskWrite());
}

TEST_F(SvdBitWidthTest, InvalidFieldWidthsAreRejectedBeforeEnumChecks) {
  struct WidthCase {
    const char* width;
    const char* diagnostic;
  };
  for(const auto& input : {WidthCase{"0", "M310"}, WidthCase{"65", "M310"},
                           WidthCase{"129", "M310"}, WidthCase{"4294967294", "M313"},
                           WidthCase{"-2", "M310"}}) {
    SCOPED_TRACE(input.width);
    m_messages.clear();
    XMLTreeElement element(nullptr, "register");
    const auto fields = ConfigureRegister(element, "32");
    AddEnum(AddField(fields, "BAD", "0", input.width), "1");
    AddField(fields, "VALID", "1", "1");
    SvdRegister reg(nullptr);

    ASSERT_TRUE(reg.Construct(&element));
    ASSERT_TRUE(reg.IsValid());
    ASSERT_NE(nullptr, reg.GetFieldContainer());
    ASSERT_EQ(2U, reg.GetFieldContainer()->GetChildCount());
    EXPECT_FALSE(reg.GetFieldContainer()->GetChildren().front()->IsValid());
    EXPECT_TRUE(reg.GetFieldContainer()->GetChildren().back()->IsValid());
    EXPECT_TRUE(HasMessage(input.diagnostic));
    EXPECT_FALSE(HasMessage("M335"));
    EXPECT_EQ(2U, reg.GetAccessMaskWrite());
  }
}

TEST_F(SvdBitWidthTest, FieldsOutsideOffsetAndRegisterBoundsAreRejected) {
  struct RangeCase {
    const char* registerWidth;
    const char* offset;
    const char* width;
    const char* diagnostic;
  };
  for(const auto& input : {RangeCase{"32", "64", "1", "M309"},
                           RangeCase{"32", "18446744073709551614", "1", "M309"},
                           RangeCase{"32", "31", "2", "M324"}}) {
    SCOPED_TRACE(input.offset);
    SCOPED_TRACE(input.width);
    m_messages.clear();
    XMLTreeElement element(nullptr, "register");
    const auto fields = ConfigureRegister(element, input.registerWidth);
    AddEnum(AddField(fields, "BAD", input.offset, input.width), "1");
    AddField(fields, "VALID", "0", "1");
    SvdRegister reg(nullptr);

    ASSERT_TRUE(reg.Construct(&element));
    ASSERT_NE(nullptr, reg.GetFieldContainer());
    ASSERT_EQ(2U, reg.GetFieldContainer()->GetChildCount());
    EXPECT_FALSE(reg.GetFieldContainer()->GetChildren().front()->IsValid());
    EXPECT_TRUE(reg.GetFieldContainer()->GetChildren().back()->IsValid());
    EXPECT_TRUE(HasMessage(input.diagnostic));
    EXPECT_FALSE(HasMessage("M335"));
    EXPECT_EQ(1U, reg.GetAccessMaskWrite());
  }
}

TEST_F(SvdBitWidthTest, AccessMaskSkipsInvalidAndUncheckedFieldRanges) {
  SvdRegister reg(nullptr);
  reg.SetBitWidth(32);
  reg.SetAccess(SvdTypes::Access::READWRITE);
  AddUncheckedField(reg, 31, 1);
  AddUncheckedField(reg, 0, 0);
  AddUncheckedField(reg, 0, 65);
  AddUncheckedField(reg, 0, -2);
  AddUncheckedField(reg, 64, 1);
  AddUncheckedField(reg, UINT64_MAX - 1, 1);
  AddUncheckedField(reg, 31, 2);
  AddUncheckedField(reg, 0, 64)->Invalidate();

  EXPECT_TRUE(reg.CalcAccessMask());
  EXPECT_EQ(uint64_t{1} << 31, reg.GetAccessMaskWrite());
  EXPECT_EQ(uint64_t{UINT32_MAX}, reg.GetAccessMaskRead());
  EXPECT_EQ(SvdTypes::Access::READWRITE, reg.GetAccessCalculated());
}

TEST_F(SvdBitWidthTest, InvalidRegisterWidthsAreRejectedBeforeMaskCalculations) {
  for(const auto* width : {"0", "65", "4294967294"}) {
    SCOPED_TRACE(width);
    m_messages.clear();
    XMLTreeElement element(nullptr, "register");
    const auto fields = ConfigureRegister(element, width);
    AddField(fields, "BIT", "0", "1");
    SvdRegister reg(nullptr);

    ASSERT_TRUE(reg.Construct(&element));
    EXPECT_FALSE(reg.IsValid());
    EXPECT_EQ(1, count(m_messages.begin(), m_messages.end(), "M302"));
    EXPECT_EQ(0U, reg.GetAccessMask());
    EXPECT_EQ(0U, reg.GetAccessMaskRead());
    EXPECT_EQ(0U, reg.GetAccessMaskWrite());
  }
}

TEST_F(SvdBitWidthTest, MaskQueriesHandleUncheckedInvalidRegisterWidths) {
  for(const auto width : {0, 65, -2}) {
    SCOPED_TRACE(width);
    SvdRegister reg(nullptr);
    reg.SetBitWidth(width);
    reg.SetAccess(SvdTypes::Access::READWRITE);
    AddUncheckedField(reg, 0, 1);

    EXPECT_EQ(0U, reg.GetAccessMask());
    EXPECT_FALSE(reg.CalcAccessMask());
    EXPECT_EQ(0U, reg.GetAccessMaskRead());
    EXPECT_EQ(0U, reg.GetAccessMaskWrite());
    EXPECT_EQ(SvdTypes::Access::READWRITE, reg.GetAccessCalculated());
  }
}

TEST_F(SvdBitWidthTest, EnumValuesAboveTheFieldWidthRemainInvalid) {
  XMLTreeElement element(nullptr, "register");
  const auto fields = ConfigureRegister(element, "32");
  AddEnum(AddField(fields, "VALUE", "0", "31"), "2147483648");
  SvdRegister reg(nullptr);

  ASSERT_TRUE(reg.Construct(&element));
  ASSERT_NE(nullptr, reg.GetFieldContainer());
  const auto field = dynamic_cast<SvdField*>(reg.GetFieldContainer()->GetChildren().front());
  ASSERT_NE(nullptr, field);
  ASSERT_EQ(1U, field->GetEnumContainer().size());
  const auto enumContainer = field->GetEnumContainer().front();
  ASSERT_EQ(1U, enumContainer->GetChildCount());
  EXPECT_FALSE(enumContainer->GetChildren().front()->IsValid());
  EXPECT_TRUE(HasMessage("M335"));
}

TEST_F(SvdBitWidthTest, FullWidth32BitRegisterWithoutFieldsPreservesAllAccessBits) {
  SvdRegister reg(nullptr);
  reg.SetBitWidth(32);
  EXPECT_EQ(uint64_t{UINT32_MAX}, reg.GetAccessMaskRead());
  EXPECT_EQ(uint64_t{UINT32_MAX}, reg.GetAccessMaskWrite());
}

TEST_F(SvdBitWidthTest, FieldValidationHandlesMissingRegisterAncestors) {
  SvdFieldContainer fields(nullptr);
  SvdDimension dimension(nullptr);
  SvdItem* parents[] = {&fields, &dimension};
  for(const auto parent : parents) {
    SvdField field(parent);
    field.SetName("BIT");
    field.SetOffset(0);
    field.SetBitWidth(1);
    EXPECT_TRUE(field.CheckItem());
    EXPECT_TRUE(field.IsValid());
  }
}

TEST_F(SvdBitWidthTest, HeaderFieldGroupsDetectFullWidthAndUpperBitOverlaps) {
  SvdOptions options;
  FileHeaderInfo info;
  BitWidthHeaderData header(info, options);
  SvdRegister reg(nullptr);
  reg.SetBitWidth(32);
  const auto full = AddUncheckedField(reg, 0, 32);
  const auto upper = AddUncheckedField(reg, 31, 1);
  const auto lower = AddUncheckedField(reg, 0, 1);
  FIELDMAPLIST groups;
  ASSERT_TRUE(header.AddFields(reg.GetFieldContainer(), groups));
  ASSERT_EQ(2U, groups.size());
  EXPECT_EQ(UINT32_MAX, groups.front().mask);
  EXPECT_EQ(full, groups.front().fields.at(0));
  EXPECT_EQ(UINT32_C(0x80000001), groups.back().mask);
  ASSERT_EQ(2U, groups.back().fields.size());
  EXPECT_EQ(lower, groups.back().fields.begin()->second);
  EXPECT_EQ(upper, groups.back().fields.rbegin()->second);
  EXPECT_EQ(lower, groups.back().fields.at(0));
  EXPECT_EQ(upper, groups.back().fields.at(31));
}

TEST_F(SvdBitWidthTest, HeaderFieldPaddingRejectsOverlapsAndOutOfRangePositions) {
  SvdOptions options;
  FileHeaderInfo info;
  BitWidthHeaderData header(info, options);
  SvdRegister reg(nullptr);
  reg.SetBitWidth(32);

  FIELDMAPLIST groups(1);
  auto& fields = groups.front().fields;
  fields.emplace(UINT64_C(0x100000000), AddUncheckedField(reg, UINT64_C(0x100000000), 1));
  EXPECT_FALSE(header.CreateSortedFields(groups, 4, "bits"));

  fields.clear();
  fields.emplace(0, AddUncheckedField(reg, 0, 2));
  fields.emplace(1, AddUncheckedField(reg, 1, 1));
  EXPECT_FALSE(header.CreateSortedFields(groups, 4, "bits"));

  fields.clear();
  fields.emplace(0, AddUncheckedField(reg, 0, 9));
  EXPECT_FALSE(header.CreateSortedFields(groups, 1, "bits"));
}

TEST_F(SvdBitWidthTest, HeaderMasksPreserve32BitBoundaryTextAndRejectWiderRanges) {
  CreateOutputDirectory();
  FileIo output;
  output.SetFileName((m_outputDirectory / "masks.h").string());
  BitWidthHeaderGenerator generator(&output);
  generator.MakeFieldPosMask3("FULL32", "Full 32-bit field", 0, UINT32_MAX);
  generator.MakeFieldPosMask3("TOP32", "Top 32-bit field", 31, 1);
  ASSERT_TRUE(output.Flush());
  ifstream input(m_outputDirectory / "masks.h");
  ASSERT_TRUE(input.is_open());
  const string text{istreambuf_iterator<char>(input), istreambuf_iterator<char>()};
  EXPECT_EQ("(0xffffffffUL)", DefineValue(text, "FULL32_Msk"));
  EXPECT_EQ("(0x80000000UL)", DefineValue(text, "TOP32_Msk"));
  EXPECT_EQ("(0UL)", DefineValue(text, "FULL32_Pos"));
  EXPECT_EQ("(31UL)", DefineValue(text, "TOP32_Pos"));

  SvdOptions options;
  FileHeaderInfo info;
  BitWidthHeaderData header(info, options);
  SvdRegister reg(nullptr);
  reg.SetBitWidth(32);
  const auto full = AddUncheckedField(reg, 0, 32);
  PosMaskNames names;
  EXPECT_TRUE(header.CreateFieldPosMask(full, &names));
  EXPECT_FALSE(HasMessage("M220"));
  EXPECT_FALSE(header.CreateFieldPosMask(AddUncheckedField(reg, 31, 2), &names));
  EXPECT_FALSE(header.CreateFieldPosMask(AddUncheckedField(reg, 0, 33), &names));
  EXPECT_FALSE(header.CreateFieldPosMask(AddUncheckedField(reg, 0, 64), &names));
  EXPECT_FALSE(header.CreateFieldPosMask(AddUncheckedField(reg, UINT64_C(0x100000000), 1), &names));
}

TEST_F(SvdBitWidthTest, FieldExpressionsPreserve32BitBoundaryTextAndRejectWiderRanges) {
  SvdRegister reg(nullptr);
  reg.SetName("CTRL");
  reg.SetBitWidth(32);
  SvdCExpression expression;
  EXPECT_EQ("(CTRL & (0xffffffff << 0))", expression.CreateFieldExpression(AddUncheckedField(reg, 0, 32)));
  EXPECT_EQ("(CTRL & (0x1 << 31))", expression.CreateFieldExpression(AddUncheckedField(reg, 31, 1)));
  EXPECT_TRUE(expression.CreateFieldExpression(AddUncheckedField(reg, 31, 2)).empty());
  EXPECT_TRUE(expression.CreateFieldExpression(AddUncheckedField(reg, 32, 1)).empty());
  EXPECT_TRUE(expression.CreateFieldExpression(AddUncheckedField(reg, 0, 33)).empty());
  EXPECT_TRUE(expression.CreateFieldExpression(AddUncheckedField(reg, 0, 64)).empty());
  EXPECT_TRUE(expression.CreateFieldExpression(AddUncheckedField(reg, 64, 1)).empty());
  EXPECT_TRUE(expression.CreateFieldExpression(AddUncheckedField(reg, 0, 65)).empty());
  EXPECT_TRUE(expression.CreateFieldExpression(AddUncheckedField(reg, 0, 0)).empty());
  EXPECT_TRUE(expression.CreateFieldExpression(AddUncheckedField(reg, UINT64_C(0x100000000), 1)).empty());
}

TEST_F(SvdBitWidthTest, InvalidFieldOperandsRejectTheWholeExpression) {
  for(const auto text : {"TEST.CTRL.FIELD == 0", "0 == TEST.CTRL.FIELD",
                         "TEST.CTRL == 0 || TEST.CTRL.FIELD == 0"}) {
    SCOPED_TRACE(text);
    SvdDevice device(nullptr);
    const auto [peripheral, reg] = AddExpressionRegister(device);
    const auto field = AddUncheckedField(*reg, 0, 1);
    SvdCExpression expression;
    expression.SetText(text);
    ASSERT_TRUE(expression.CalcExpression(peripheral));
    ASSERT_FALSE(expression.GetExpressionString().empty());

    field->SetBitWidth(0);
    ASSERT_TRUE(field->IsValid());
    EXPECT_TRUE(expression.GetExpressionString().empty());

    field->SetBitWidth(1);
    field->Invalidate();
    EXPECT_TRUE(expression.GetExpressionString().empty());
  }
}

TEST_F(SvdBitWidthTest, InvalidRegisterOperandsRejectTheWholeExpression) {
  SvdDevice device(nullptr);
  const auto [peripheral, reg] = AddExpressionRegister(device);
  SvdCExpression expression;
  expression.SetText("0 == TEST.CTRL");
  ASSERT_TRUE(expression.CalcExpression(peripheral));
  ASSERT_EQ("0 == TEST_CTRL ", expression.GetExpressionString());
  reg->Invalidate();
  EXPECT_TRUE(expression.GetExpressionString().empty());
}

TEST_F(SvdBitWidthTest, ExpressionLookupRejectsRegistersWithoutFieldContainers) {
  SvdDevice device(nullptr);
  const auto [peripheral, reg] = AddExpressionRegister(device);
  ASSERT_TRUE(reg->GetChildren().empty());

  SvdCExpression expression;
  expression.SetText("TEST.CTRL.FIELD == 0");
  EXPECT_FALSE(expression.CalcExpression(peripheral));
  EXPECT_FALSE(expression.IsValid());
  EXPECT_TRUE(HasMessage("M244"));
}

TEST_F(SvdBitWidthTest, ExpressionLookupRejectsPeripheralsWithoutRegisterContainers) {
  SvdDevice device(nullptr);
  const auto peripheral = AddExpressionRegister(device).first;
  const auto peripherals = peripheral->GetParent();
  ASSERT_NE(peripherals, nullptr);
  const auto empty = new SvdPeripheral(peripherals);
  peripherals->AddItem(empty);
  empty->SetName("EMPTY");
  ASSERT_TRUE(empty->GetChildren().empty());

  SvdCExpression expression;
  expression.SetText("EMPTY.CTRL == 0");
  EXPECT_FALSE(expression.CalcExpression(peripheral));
  EXPECT_FALSE(expression.IsValid());
  EXPECT_TRUE(HasMessage("M244"));
}

TEST_F(SvdBitWidthTest, SfdChecksActualFieldReferencesBeforeCreatingOutput) {
  CreateOutputDirectory();
  for(const bool invalid : {false, true}) {
    SCOPED_TRACE(invalid);
    m_messages.clear();
    SvdDevice device(nullptr);
    const auto [peripheral, reg] = AddExpressionRegister(device);
    const auto field = AddUncheckedField(*reg, invalid ? 0 : 32, 1);
    if(invalid) {
      field->Invalidate();
    }
    XMLTreeElement condition(nullptr, "disableCondition");
    condition.SetText("TEST.CTRL.FIELD == 0");
    ASSERT_TRUE(peripheral->ProcessXmlElement(&condition));
    ASSERT_TRUE(peripheral->CalcDisableCondition());
    ASSERT_TRUE(peripheral->GetDisableCondition()->IsValid());
    ASSERT_TRUE(reg->IsValid());

    SvdOptions options;
    FileHeaderInfo info;
    SfdData sfd(info, options);
    const auto output = m_outputDirectory / "device.sfd";
    EXPECT_FALSE(sfd.Create(&device, output.string()));
    EXPECT_TRUE(HasMessage("M226"));
    EXPECT_FALSE(filesystem::exists(output));
  }
}

TEST_F(SvdBitWidthTest, SfdPreservesValidConditionsWithUnreferencedInvalidFields) {
  CreateOutputDirectory();
  const auto inputFile = m_outputDirectory / "input.svd";
  {
    ofstream input(inputFile);
    input << "<device/>\n";
    ASSERT_TRUE(input.good());
  }
  SvdDevice device(nullptr);
  const auto [peripheral, reg] = AddExpressionRegister(device);
  AddUncheckedField(*reg, 0, 1);
  const auto unused = AddUncheckedField(*reg, 0, 0);
  unused->SetName("UNUSED");
  unused->Invalidate();
  XMLTreeElement condition(nullptr, "disableCondition");
  condition.SetText("TEST.CTRL.FIELD == 0");
  ASSERT_TRUE(peripheral->ProcessXmlElement(&condition));
  ASSERT_TRUE(peripheral->CalcDisableCondition());
  ASSERT_TRUE(peripheral->GetDisableCondition()->IsValid());
  EXPECT_EQ("(TEST_CTRL & (0x1 << 0)) == 0 ", peripheral->GetDisableCondition()->GetExpressionString());

  SvdOptions options;
  FileHeaderInfo info;
  info.svdFileName = inputFile.string();
  const auto output = m_outputDirectory / "device.sfd";
  {
    SfdData sfd(info, options);
    EXPECT_TRUE(sfd.Create(&device, output.string()));
  }
  EXPECT_FALSE(HasMessage("M226"));
  ifstream input(output);
  ASSERT_TRUE(input.is_open());
  const string text{istreambuf_iterator<char>(input), istreambuf_iterator<char>()};
  EXPECT_NE(string::npos, text.find("<disableCond> (TEST_CTRL & (0x1 << 0)) == 0"));
}

TEST_F(SvdBitWidthTest, SfdRejectsUnsupported64BitRegisterBeforeCreatingOutput) {
  CreateOutputDirectory();
  SvdDevice device(nullptr);
  const auto reg = new SvdRegister(&device);
  device.AddItem(reg);
  reg->SetName("CTRL");
  reg->SetBitWidth(64);
  SvdOptions options;
  FileHeaderInfo info;
  SfdData sfd(info, options);
  const auto output = m_outputDirectory / "device.sfd";
  EXPECT_FALSE(sfd.Create(&device, output.string()));
  EXPECT_TRUE(HasMessage("M226"));
  EXPECT_FALSE(filesystem::exists(output));
}

TEST_F(SvdBitWidthTest, SfdFieldMasksAccept32BitsAndRejectWiderRanges) {
  SvdOptions options;
  FileHeaderInfo info;
  BitWidthSfdData sfd(info, options);
  SvdRegister reg(nullptr);
  reg.SetBitWidth(32);
  EXPECT_TRUE(sfd.CreateFieldItem(AddUncheckedField(reg, 0, 32)));
  EXPECT_TRUE(sfd.CreateFieldItem(AddUncheckedField(reg, 31, 1)));
  EXPECT_FALSE(HasMessage("M226"));
  EXPECT_FALSE(sfd.CreateFieldItem(AddUncheckedField(reg, 0, 64)));
  EXPECT_FALSE(sfd.CreateFieldItem(AddUncheckedField(reg, 32, 1)));
  EXPECT_TRUE(HasMessage("M226"));
}

TEST_F(SvdBitWidthTest, SfdRejectsInvalidExpressionReferencesBeforeCreatingOutput) {
  CreateOutputDirectory();
  SvdDevice device(nullptr);
  const auto reg = new SvdRegister(&device);
  device.AddItem(reg);
  reg->SetName("CTRL");
  reg->SetBitWidth(1024);
  reg->Invalidate();
  device.GetExpressionRegistersList()["CTRL"] = reg;
  SvdOptions options;
  FileHeaderInfo info;
  SfdData sfd(info, options);
  const auto output = m_outputDirectory / "device.sfd";
  EXPECT_FALSE(sfd.Create(&device, output.string()));
  EXPECT_TRUE(HasMessage("M226"));
  EXPECT_FALSE(filesystem::exists(output));
}

TEST_F(SvdBitWidthTest, HeaderFieldGroupsSkipUncheckedInvalidRanges) {
  SvdOptions options;
  FileHeaderInfo info;
  BitWidthHeaderData header(info, options);
  SvdRegister reg(nullptr);
  reg.SetBitWidth(32);
  AddUncheckedField(reg, 0, 0);
  AddUncheckedField(reg, 0, -1);
  AddUncheckedField(reg, 0, INT32_MIN);
  AddUncheckedField(reg, 0, 33);
  AddUncheckedField(reg, 0, 64);
  AddUncheckedField(reg, 0, 65);
  AddUncheckedField(reg, 32, 1);
  AddUncheckedField(reg, 64, 1);
  AddUncheckedField(reg, 31, 2);
  AddUncheckedField(reg, UINT64_C(0x100000000), 1);
  AddUncheckedField(reg, UINT64_C(0x10000001f), 1);
  AddUncheckedField(reg, UINT64_MAX, 1);
  const auto upper = AddUncheckedField(reg, 31, 1);

  FIELDMAPLIST groups;
  ASSERT_TRUE(header.AddFields(reg.GetFieldContainer(), groups));
  ASSERT_EQ(1U, groups.size());
  EXPECT_EQ(UINT32_C(0x80000000), groups.front().mask);
  ASSERT_EQ(1U, groups.front().fields.size());
  EXPECT_EQ(upper, groups.front().fields.at(31));
}

TEST_F(SvdBitWidthTest, HeaderRejectsInvalidShiftRangesWithoutPartialMacros) {
  CreateOutputDirectory();
  FileIo output;
  output.SetFileName((m_outputDirectory / "masks.h").string());
  BitWidthHeaderGenerator generator(&output);
  generator.MakeFieldPosMask3("INVALID_POSITION", "Position outside register", 32, 1);
  generator.MakeFieldPosMask3("INVALID_MASK", "Mask crosses register boundary", 31, 3);
  generator.MakeFieldPosMask3("INVALID_WIDE_POSITION", "Position must not wrap", UINT64_C(0x10000001f), 1);
  generator.MakeFieldPosMask3("INVALID_WIDE_MASK", "Mask must not wrap", 0, UINT64_C(0x100000000));
  EXPECT_EQ(4, count(m_messages.begin(), m_messages.end(), "M220"));
  generator.MakeFieldPosMask3("VALID", "Top 32-bit field", 31, 1);
  ASSERT_TRUE(output.Flush());

  ifstream input(m_outputDirectory / "masks.h");
  ASSERT_TRUE(input.is_open());
  const string text{istreambuf_iterator<char>(input), istreambuf_iterator<char>()};
  EXPECT_EQ(string::npos, text.find("INVALID_POSITION"));
  EXPECT_EQ(string::npos, text.find("INVALID_MASK"));
  EXPECT_EQ(string::npos, text.find("INVALID_WIDE_POSITION"));
  EXPECT_EQ(string::npos, text.find("INVALID_WIDE_MASK"));
  EXPECT_EQ("(31UL)", DefineValue(text, "VALID_Pos"));
  EXPECT_EQ("(0x80000000UL)", DefineValue(text, "VALID_Msk"));
}

TEST_F(SvdBitWidthTest, SfdRejectsUnsupportedExpressionReferenceOutsideDeviceChildren) {
  CreateOutputDirectory();
  // The separate owner outlives the device's non-owning expression-reference map.
  const auto reg = make_unique<SvdRegister>(nullptr);
  SvdDevice device(nullptr);
  reg->SetName("CTRL");
  reg->SetBitWidth(64);
  device.GetExpressionRegistersList()["CTRL"] = reg.get();
  SvdOptions options;
  FileHeaderInfo info;
  SfdData sfd(info, options);
  const auto output = m_outputDirectory / "device.sfd";

  ASSERT_TRUE(reg->IsValid());
  ASSERT_TRUE(device.GetChildren().empty());
  EXPECT_FALSE(sfd.Create(&device, output.string()));
  EXPECT_TRUE(HasMessage("M226"));
  EXPECT_FALSE(filesystem::exists(output));
}

TEST_F(SvdBitWidthTest, FullWidthFieldsStillReportIncompatibleRegisterAccess) {
  constexpr int32_t width = 32;
  SvdRegister reg(nullptr);
  reg.SetName("CTRL");
  reg.SetBitWidth(width);
  reg.SetAccess(SvdTypes::Access::READONLY);
  const auto field = AddUncheckedField(reg, 0, width);

  EXPECT_TRUE(field->CheckItem());
  EXPECT_TRUE(field->IsValid());
  EXPECT_TRUE(HasMessage("M367"));
  EXPECT_EQ(SvdTypes::Access::READWRITE, field->GetAccess());
}

TEST_F(SvdBitWidthTest, RegisterSkipsRejectedRangesBeforeCheckingSiblingConflicts) {
  SvdRegister reg(nullptr);
  reg.SetName("CTRL");
  reg.SetBitWidth(32);
  const auto invalid = AddUncheckedField(reg, 31, 2);
  const auto valid = AddUncheckedField(reg, 31, 1);

  EXPECT_TRUE(reg.CheckFields(reg.GetFieldContainer(), 32, reg.GetName()));
  EXPECT_FALSE(invalid->IsValid());
  EXPECT_TRUE(valid->IsValid());
  EXPECT_EQ(1, count(m_messages.begin(), m_messages.end(), "M345"));
  EXPECT_FALSE(HasMessage("M336"));
  EXPECT_FALSE(HasMessage("M338"));
  EXPECT_EQ(uint64_t{1} << 31, reg.GetAccessMaskWrite());
}

} // namespace
