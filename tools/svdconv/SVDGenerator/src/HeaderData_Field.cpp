/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdarg.h>
#include "HeaderData.h"
#include "HeaderGenerator.h"
#include "SvdItem.h"
#include "SvdRegister.h"
#include "SvdField.h"
#include "SvdDimension.h"
#include "SvdUtils.h"
#include "HeaderGenAPI.h"

using namespace std;
using namespace c_header;


bool HeaderData::CreateFields(SvdRegister* reg, string structName)
{
  if(!reg) {
    return false;
  }

  const auto& childs = reg->GetChildren();
  if(childs.empty()) {
    return true;
  }

  uint32_t regSize = reg->GetSize();

  for(const auto child : childs) {
    const auto fieldCont = dynamic_cast<SvdFieldContainer*>(child);
    if(!fieldCont || !fieldCont->IsValid()) {
      continue;
    }

    FIELDMAPLIST sortedFields;
    AddFields(fieldCont, sortedFields);

    if(!CreateSortedFields(sortedFields, regSize, structName)) {
      return false;
    }
  }

  return true;
}


struct FIELDSTRUCT {
  uint32_t usedBits;
  std::map<uint32_t, SvdItem*> fields;

  FIELDSTRUCT() { usedBits = 0; }
};

std::list<FIELDSTRUCT> allFields;


bool HeaderData::AddFields(SvdItem* container, FIELDMAPLIST& sortedFields)
{
  const auto& childs = container->GetChildren();
  for(const auto child : childs) {
    const auto field = dynamic_cast<SvdField*>(child);
    if(!field || !field->IsValid()) {
      continue;
    }

    const auto dim = field->GetDimension();
    if(dim) {
      if(dim->GetExpression()->GetType() == SvdTypes::Expression::EXTEND) {
        AddFields(dim, sortedFields);
        continue;
      }
    }

    const auto offset = field->GetOffset();
    const auto bitWidth = field->GetBitWidth();
    if(offset >= 32 || bitWidth <= 0 || bitWidth > 32 || offset + bitWidth > 32) {
      continue;
    }
    const auto mask = SvdUtils::CreateBitMask(bitWidth, offset);

    ONESTRUCT* oneStruct = nullptr;
    for(auto& oneStr : sortedFields) {
      if(!(oneStr.mask & mask)) {
        oneStruct = &oneStr;
        break;
      }
    }

    if(!oneStruct) {
      ONESTRUCT oneStr;
      sortedFields.push_back(oneStr);
      oneStruct = &*sortedFields.rbegin();
    }

    oneStruct->fields.emplace(offset, field);
    oneStruct->mask |= mask;
  }

  return true;
}


bool HeaderData::CreateSortedFields(const FIELDMAPLIST& sortedFields, uint32_t regSize, string structName)
{
  m_reservedFieldCnt = 0;
  const auto registerBits = uint64_t{regSize} * 8;

  for(const auto& oneStruct : sortedFields) {
    uint64_t offsCnt = 0;
    m_gen->Generate<STRUCT|BEGIN>("");

    for(const auto& [newOffs, field] : oneStruct.fields) {
      if(newOffs < offsCnt || newOffs > registerBits) {
        m_gen->Generate<C_ERROR>("Reserved bits calculation error", -1);
        return false;
      }
      GenerateReservedField(newOffs - offsCnt, regSize);
      offsCnt = newOffs;
      offsCnt += CreateField(field, regSize, offsCnt);
    }

    if(offsCnt > registerBits) {
      m_gen->Generate<C_ERROR>("Reserved bits calculation error", -1);
      return false;
    }
    GenerateReservedField(registerBits - offsCnt, regSize);  // fill up (pad) bitfields

    m_gen->Generate<STRUCT|END>("%s", structName.c_str());
  }

  return true;
}

uint64_t HeaderData::CreateField(SvdField* field, uint32_t regSize, uint64_t offsCnt)
{
  if(!field) {
    return false;
  }

  const auto& dataType  = SvdUtils::GetDataTypeString(regSize);
  const auto name       = field->GetNameCalculated();
  const auto descr      = field->GetDescriptionCalculated();
  const auto accType    = field->GetEffectiveAccess();
  const auto fieldWidth = field->GetBitWidth();
  const auto fieldPos   = field->GetOffset();
  if(fieldPos >= 32 || fieldWidth <= 0 || fieldWidth > 32 || fieldPos + fieldWidth > 32) {
    return 0;
  }

  m_gen->Generate<MAKE|MK_FIELD_STRUCT>("%s", accType, dataType.c_str(), regSize, fieldWidth, name.c_str());
  const auto lastBit = fieldPos + fieldWidth -1;
  m_gen->Generate<MAKE|MK_DOXY_COMMENT_BITFIELD>("%s", fieldPos, lastBit, !descr.empty()? descr.c_str() : name.c_str());

  if(offsCnt != fieldPos) {
    m_gen->Generate<C_ERROR>("Reserved bits calculation error", -1);
  }

  return fieldWidth;
}

