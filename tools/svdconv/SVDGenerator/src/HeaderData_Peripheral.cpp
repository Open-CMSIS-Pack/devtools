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
#include "SvdDevice.h"
#include "SvdPeripheral.h"
#include "SvdRegister.h"
#include "SvdDimension.h"
#include "SvdUtils.h"
#include "HeaderGenAPI.h"

using namespace std;
using namespace c_header;


bool HeaderData::CreatePeripherals(SvdDevice* device)
{
  CreatePeripheralsType         (device);
  CreatePeripheralsAddressMap   (device);
  CreatePeripheralsInstance     (device);

  return true;
}

bool HeaderData::CreatePeripheralsType(SvdDevice* device)
{
  m_gen->Generate<DESCR|HEADER         >("Device Specific Peripheral Section");
  m_gen->Generate<MAKE|MK_DOXYADDGROUP >("Device_Peripheral_peripherals");

  const auto& peris = device->GetPeripheralList();
  for(const auto peri : peris) {
    if(!peri || !peri->IsValid()) {
      continue;
    }

    CreatePeripheralType(peri);
  }

  m_gen->Generate<MAKE|MK_DOXYENDGROUP >("Device_Peripheral_peripherals");

  return true;
}

bool HeaderData::CreatePeripheralType(SvdPeripheral* peripheral)
{
  m_addressCnt  = 0;
  m_reservedCnt = 0;

  OpenPeripheral(peripheral);

  const auto regCont = peripheral->GetRegisterContainer();
  if(regCont) {
    //CalculateMaxPaddingWidth(regCont);
    SetMaxBitWidth(peripheral->GetBitWidth());
    CreateRegisters(regCont);
  }

  peripheral->SetSize(m_addressCnt);
  ClosePeripheral(peripheral);

  return true;
}

bool HeaderData::OpenPeripheral(SvdPeripheral* peripheral)
{
//const auto& headerTypeName = peripheral->GetHeaderTypeName();
  const auto peripheralName  = peripheral->GetNameCalculated();
  const auto descr           = peripheral->GetDescriptionCalculated();

  m_reservedPad.clear();

  string text = descr;
  SvdUtils::TrimWhitespace(text);
  text += " (";
  text += peripheralName;
  text += ")";

  m_gen->Generate<DESCR|PART >("%s", peripheralName.c_str());
  m_gen->Generate<MAKE|MK_DOXYADDPERI>("%s", text.c_str());
  m_gen->Generate<DIRECT >("");
  m_gen->Generate<STRUCT|BEGIN|TYPEDEF>("");

  uint32_t addrInPeri = (uint32_t) peripheral->GetAbsoluteAddress();
  m_gen->Generate<MAKE|MK_DOXY_COMMENT_ADDR>("%s Structure", addrInPeri, peripheralName.c_str());

  return true;
}

bool HeaderData::ClosePeripheral(SvdPeripheral* peripheral)
{
  const auto periName  = peripheral->GetHeaderTypeName();
  uint32_t maxWidth = peripheral->GetBitWidth();
  uint32_t remain = m_addressCnt % (maxWidth / 8);

  if(remain) {
    int32_t genRes = (int32_t)4 - remain;
    GenerateReserved(genRes, m_addressCnt, false);
    m_addressCnt += genRes;
  }

  remain = m_addressCnt % (maxWidth / 8);
  if(remain) {
    m_gen->Generate<C_ERROR>("Struct end-padding calculation error!", -1);
  }

  string name = periName;
  const auto dim = peripheral->GetDimension();
  if(dim) {
    uint32_t periSize  = peripheral->GetSize();
    uint32_t periInc   = dim->GetDimIncrement();

    if(periSize <= periInc) {
      int32_t reserved   = (int32_t)periInc - periSize;
      int32_t checkRes   = (int32_t)periInc - m_addressCnt;

      if(reserved != checkRes) {
        m_gen->Generate<C_ERROR>("Reserved bytes calculation error!", -1);
      }

      GenerateReserved(reserved, (uint32_t)peripheral->GetAbsoluteAddress() + periSize, false);
      peripheral->SetSize(reserved + periSize);
    }
    else {
      m_gen->Generate<C_ERROR>("Peripheral size (0x%02x) greater than <dimIncrement> (0x%02x) !", peripheral->GetLineNumber(), periSize, periInc);
    }
  }

  GenerateReserved();
  peripheral->SetSize(m_addressCnt);

  if(m_reservedPad.size()) {
    m_gen->Generate<C_ERROR>("Not generated remaining reserved bytes error!", -1);
  }

  if(dim) {
    const auto expr = dim->GetExpression();
    if(expr) {
      const auto exprType = expr->GetType();

      if(exprType == SvdTypes::Expression::ARRAY) {
        m_gen->Generate<STRUCT|END|TYPEDEF>("%s", name.c_str());
      }
    }
  }
  else {
    m_gen->Generate<STRUCT|END|TYPEDEF>("%s", name.c_str());
  }

  if(m_bDebugHeaderfile) {
    uint32_t size = peripheral->GetSize();
    m_gen->Generate<MAKE|MK_DOXY_COMMENT>("Size = %i (0x%x)", size, size);
  }

  if(dim) {
    const auto expr = dim->GetExpression();
    if(expr) {
      const auto exprType = expr->GetType();

      if(exprType == SvdTypes::Expression::ARRAY) {
        uint32_t num = dim->GetDim();
        m_gen->Generate<MAKE|MK_TYPEDEFTOARRAY>("%s", num, name.c_str());
      }
    }
  }

  m_gen->Generate<DIRECT>("");

  return true;
}

bool HeaderData::CreatePeripheralsAddressMap(SvdDevice* device)
{
  if(!device) {
    return true;
  }

  m_gen->Generate<DESCR|HEADER         >("Device Specific Peripheral Address Map");
  m_gen->Generate<MAKE|MK_DOXYADDGROUP >("Device_Peripheral_peripheralAddr");

  const auto periCont = device->GetPeripheralContainer();
  if(!periCont) {
    return false;
  }

  const auto& childs = periCont->GetChildren();
  for(const auto child : childs) {
    const auto peri = dynamic_cast<SvdPeripheral*>(child);
    if(!peri || !peri->IsValid()) {
      continue;
    }

    CreatePeripheralAddressMap(peri);
  }

  m_gen->Generate<MAKE|MK_DOXYENDGROUP >("Device_Peripheral_peripheralAddr");

  return true;
}

bool HeaderData::CreatePeripheralAddressMap(SvdPeripheral* peripheral)
{
  if(!peripheral) {
    return true;
  }

  const auto periName  = peripheral->GetNameCalculated();
  const auto& periPre  = peripheral->GetHeaderDefinitionsPrefix();
  uint32_t    periAddr = (uint32_t )peripheral->GetAbsoluteAddress();

  string name = periName;
  const auto dim = peripheral->GetDimension();
  if(dim) {
    const auto expr = dim->GetExpression();
    if(expr) {
      const auto exprType = expr->GetType();

      if(exprType == SvdTypes::Expression::ARRAY) {
        name = expr->GetName();
      }
      else if(exprType == SvdTypes::Expression::EXTEND) {
        const auto& childs = dim->GetChildren();
        for(const auto child : childs) {
          const auto peri = dynamic_cast<SvdPeripheral*>(child);
          if(!peri || !peri->IsValid()) {
            continue;
          }

          CreatePeripheralAddressMap(peri);
        }
        return true;
      }
    }
  }

  m_gen->Generate<MAKE|MK_PERIADDRDEF>("%s", periAddr, periPre.c_str(), name.c_str());

  return true;
}

bool HeaderData::CreatePeripheralsInstance(SvdDevice* device)
{
  if(!device) {
    return true;
  }

  m_gen->Generate<DESCR|HEADER         >("Peripheral declaration");
  m_gen->Generate<MAKE|MK_DOXYADDGROUP >("Device_Peripheral_declaration");

  const auto periCont = device->GetPeripheralContainer();
  if(!periCont) {
    return false;
  }

  const auto& childs = periCont->GetChildren();
  for(const auto child : childs) {
    const auto peri = dynamic_cast<SvdPeripheral*>(child);
    if(!peri|| !peri->IsValid()) {
      continue;
    }

    CreatePeripheralInstance(peri);
  }

  m_gen->Generate<MAKE|MK_DOXYENDGROUP >("Device_Peripheral_declaration");

  return true;
}

bool HeaderData::CreatePeripheralInstance(SvdPeripheral* peripheral)
{
  if(!peripheral) {
    return true;
  }

  const auto typeName = peripheral->GetHeaderTypeName();
  const auto periName = peripheral->GetNameCalculated();
  const auto& periPre = peripheral->GetHeaderDefinitionsPrefix();
  uint32_t   periAddr = (uint32_t )peripheral->GetAbsoluteAddress();

  string name = periName;
  auto exprType = SvdTypes::Expression::UNDEF;

  const auto dim = peripheral->GetDimension();
  if(dim) {
    SvdExpression* expr = dim->GetExpression();
    if(expr) {
      exprType = expr->GetType();
      if(exprType == SvdTypes::Expression::ARRAY) {
        ;
      }
      else if(exprType == SvdTypes::Expression::EXTEND) {
        const auto& childs = dim->GetChildren();
        for(const auto child : childs) {
          const auto peri = dynamic_cast<SvdPeripheral*>(child);
          if(!peri || !peri->IsValid()) {
            continue;
          }

          CreatePeripheralInstance(peri);
        }
        return true;
      }
    }
  }

  if(exprType == SvdTypes::Expression::ARRAY)
    m_gen->Generate<MAKE|MK_PERIARRADDRMAP    >("%s", periAddr, typeName.c_str(), periPre.c_str(), name.c_str());
  else
    m_gen->Generate<MAKE|MK_PERIADDRMAP       >("%s", periAddr, typeName.c_str(), periPre.c_str(), name.c_str());

  return true;
}
