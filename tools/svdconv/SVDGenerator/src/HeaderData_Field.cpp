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

    CreateSortedFields(sortedFields, regSize, structName);
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

    const auto name = field->GetNameCalculated();
    uint32_t offs = (uint32_t) field->GetOffset();
    uint32_t width = (uint32_t) field->GetBitWidth();
    uint32_t mask = ((1<<width)-1) << offs;

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

    oneStruct->mask |= mask;
    oneStruct->fields[offs] = field;
  }

  return true;
}


bool HeaderData::CreateSortedFields(const FIELDMAPLIST& sortedFields, uint32_t regSize, string structName)
{
  m_reservedFieldCnt = 0;

  for(const auto& oneStruct : sortedFields) {
    uint32_t offsCnt = 0;
    m_gen->Generate<STRUCT|BEGIN>("");

    for(const auto& [newOffs, field] : oneStruct.fields) {
      int32_t res = newOffs - offsCnt;

      GenerateReservedField(res, regSize);
      offsCnt += res;
      offsCnt += CreateField(field, regSize, offsCnt);
    }

    GenerateReservedField((regSize * 8) - offsCnt, regSize);  // fill up (pad) bitfields

    m_gen->Generate<STRUCT|END>("%s", structName.c_str());
  }

  return true;
}

uint32_t HeaderData::CreateField(SvdField* field, uint32_t regSize, uint32_t offsCnt)
{
  if(!field) {
    return false;
  }

  uint32_t sizeNeeded = 0;

  const auto& dataType  = SvdUtils::GetDataTypeString(regSize);
  const auto name       = field->GetNameCalculated();
  const auto descr      = field->GetDescriptionCalculated();
  const auto accType    = field->GetEffectiveAccess();
  uint32_t   fieldWidth = field->GetBitWidth();
  uint32_t   fieldPos   = (uint32_t)field->GetOffset();

  m_gen->Generate<MAKE|MK_FIELD_STRUCT>("%s", accType, dataType.c_str(), regSize, fieldWidth, name.c_str());
  uint32_t lastBit = fieldPos + fieldWidth -1;
  m_gen->Generate<MAKE|MK_DOXY_COMMENT_BITFIELD>("%s", fieldPos, lastBit, !descr.empty()? descr.c_str() : name.c_str());

  if(sizeNeeded < fieldWidth) {
    sizeNeeded = fieldWidth;
  }

  if(offsCnt != fieldPos) {
    m_gen->Generate<C_ERROR>("Reserved bits calculation error", -1);
  }

  return sizeNeeded;
}

