/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "CodeGenerator.h"
#include "HeaderData.h"
#include "SvdDimension.h"
#include "SvdField.h"
#include "SvdRegister.h"

#include "gtest/gtest.h"
#include <memory>
#include <string>

using namespace std;


TEST(CodeGenerator, Check) {
#if 0
  CodeGenerator gen;
  const string text = "This is a %i printf %s";
  int num = 42;
  const char* textInsert = "insertion test";

  const string& res = gen.FormatStr(text, num, textInsert);

  ASSERT_EQ("This is a 42 printf insertion test", res);
#endif
}


namespace {

class RootItem : public SvdItem {
public:
  RootItem() : SvdItem(nullptr) {}

  const string& GetPrependToName() override { return m_empty; }
  const string& GetAppendToName() override { return m_empty; }

private:
  const string m_empty;
};

class TestHeaderData : public HeaderData {
public:
  using HeaderData::HeaderData;
  using HeaderData::CreateRegisterPosMask;
};

} // namespace


TEST(HeaderData, CreateRegisterPosMaskHandlesDimensionVariants) {
  const auto addField = [](SvdItem* parent, const string& name, uint32_t offset) {
    const auto field = new SvdField(parent);
    field->SetName(name);
    field->SetOffset(offset);
    field->SetBitWidth(1);
    parent->AddItem(field);
    return field;
  };

  SvdOptions options;
  FileHeaderInfo fileHeaderInfo;
  TestHeaderData headerData(fileHeaderInfo, options);

  auto root = make_unique<RootItem>();
  const auto reg = new SvdRegister(root.get());
  reg->SetName("CTRL");
  root->AddItem(reg);

  const auto fields = new SvdFieldContainer(reg);
  reg->AddItem(fields);
  addField(fields, "PLAIN", 0);

  const auto arrayField = addField(fields, "ARRAY", 1);
  const auto arrayDim = new SvdDimension(arrayField);
  arrayDim->GetExpression()->SetType(SvdTypes::Expression::ARRAY);
  arrayField->SetDimension(arrayDim);

  const auto extendField = addField(fields, "EXTEND%s", 2);
  const auto extendDim = new SvdDimension(extendField);
  extendDim->GetExpression()->SetType(SvdTypes::Expression::EXTEND);
  extendField->SetDimension(extendDim);

  addField(extendDim, "EXTEND0", 2);
  const auto invalidField = addField(extendDim, "EXTEND1", 3);
  invalidField->SetValid(false);
  extendDim->AddItem(new SvdItem(extendDim));

  PosMaskNames names;
  names.name = "TEST";
  EXPECT_TRUE(headerData.CreateRegisterPosMask(reg, &names));
}



