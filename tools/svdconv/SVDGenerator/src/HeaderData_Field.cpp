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


bool HeaderData::CreateFields(SvdRegister* reg)
{
  uint32_t regSize = reg->GetSize();  //GetEffectiveBitWidth() / 8;
  
  const auto& childs = reg->GetChildren();
  if(childs.empty()) {
    return true;
  }

  for(const auto child : childs) {
    const auto fieldCont = dynamic_cast<SvdFieldContainer*>(child);
    if(!fieldCont || !fieldCont->IsValid()) {
      continue;
    }

    FIELDMAP sortedFields;
    AddFields(fieldCont, sortedFields);
    CreateSortedFields(sortedFields, regSize);
    break;      // only create first one now
  }

  return true;
}

bool HeaderData::AddFields(SvdItem* container, FIELDMAP& sortedFields)
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
    sortedFields[offs].push_back(field);
  }

  return true;
}



bool HeaderData::CreateSortedFields(const FIELDMAP& sortedFields, uint32_t regSize)
{
  uint32_t offsCnt = 0;
  m_reservedFieldCnt = 0;

  for(const auto& [newOffs, fieldList] : sortedFields) {
    int32_t res = newOffs - offsCnt;

    GenerateReservedField(res, regSize);
    offsCnt += res;
    offsCnt += CreateField(fieldList, regSize, offsCnt);
  }

  GenerateReservedField((regSize * 8) - offsCnt, regSize);  // fill up (pad) bitfields

  return true;
}

uint32_t HeaderData::CreateField(const list<SvdItem*>& fieldList, uint32_t regSize, uint32_t offsCnt) 
{
  if(fieldList.empty()) {
    return false;
  }

  uint32_t sizeNeeded = 0;
  //SvdField* field = dynamic_cast<SvdField*>(*fieldList.begin());
  //uint32_t newOffs = (uint32_t)field->GetOffset();

  if(fieldList.size() > 1) {
    m_gen->Generate<UNION|BEGIN>("");
  }

  for(const auto item : fieldList) {
    const auto field = dynamic_cast<SvdField*>(item);
    if(!field) {
      continue;
    }

    const auto& dataType  = SvdUtils::GetDataTypeString(regSize);
    const auto name       = field->GetNameCalculated();
    const auto descr      = field->GetDescriptionCalculated();
    const auto accType    = field->GetEffectiveAccess();
    uint32_t   fieldWidth = field->GetBitWidth();
    uint32_t   fieldPos   = (uint32_t)field->GetOffset();

    m_gen->Generate<MAKE|MK_FIELD_STRUCT>("%s", accType, dataType.c_str(), regSize, fieldWidth, name.c_str());
    //m_gen->Generate<MAKE|MK_DOXY_COMMENT_ADDR>("%s", fieldPos, !descr.empty()? descr.c_str() : name.c_str());
    //if(fieldWidth == 1)
    //  m_gen->Generate<MAKE|MK_DOXY_COMMENT_BITPOS>("%s", fieldPos, !descr.empty()? descr.c_str() : name.c_str());
    //else {
      uint32_t lastBit = fieldPos + fieldWidth -1;
      m_gen->Generate<MAKE|MK_DOXY_COMMENT_BITFIELD>("%s", fieldPos, lastBit, !descr.empty()? descr.c_str() : name.c_str());
    //}

    if(sizeNeeded < fieldWidth) {
      sizeNeeded = fieldWidth;
    }

    if(offsCnt != fieldPos) {
      m_gen->Generate<C_ERROR>("Reserved bits calculation error", -1);
    }
  }

  if(fieldList.size() > 1) {
    m_gen->Generate<UNION|END|ANON>("");
  }
  
  return sizeNeeded;
}
