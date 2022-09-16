/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "SvdItem.h"
#include "SvdInterrupt.h"
#include "SvdUtils.h"
#include "XMLTree.h"
#include "ErrLog.h"
#include "SvdDimension.h"
#include "SvdDevice.h"
#include "SvdCpu.h"

using namespace std;


SvdInterrupt::SvdInterrupt(SvdItem* parent):
  SvdItem(parent),
  m_value(SvdItem::VALUE32_NOT_INIT)
{
  SetSvdLevel(L_Interrupt);
}

SvdInterrupt::~SvdInterrupt()
{
}

bool SvdInterrupt::Construct(XMLTreeElement* xmlElement)
{
  return SvdItem::Construct(xmlElement);
}

bool SvdInterrupt::ProcessXmlElement(XMLTreeElement* xmlElement)
{
	const auto& tag   = xmlElement->GetTag();
  const auto& value = xmlElement->GetText();

  if(tag == "value") {
    if(!SvdUtils::ConvertNumber(value, m_value))
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    return true;
  }

  return SvdItem::ProcessXmlElement(xmlElement);
}

bool SvdInterrupt::ProcessXmlAttributes(XMLTreeElement* xmlElement)
{
  return SvdItem::ProcessXmlAttributes(xmlElement);
}

bool SvdInterrupt::CalculateDim()
{
  const auto& name = GetName();

  string tmpText;
  uint32_t insertPos = 0;
  const auto exprType = SvdUtils::ParseExpression(name, tmpText, insertPos);

  if(exprType == SvdTypes::Expression::ARRAY || exprType == SvdTypes::Expression::EXTEND) {
    auto dim = GetDimension();
    if(!dim) {        // copy <dim> from parent
      const auto parent = GetParent();
      if(parent) {  
        const auto parentDim = parent->GetDimension();
        if(parentDim) {
          CopyDim(this, parent);
          dim = GetDimension();
          dim->SetDimIncrement(1);
        }
      }
    }
  }

  const auto dim = GetDimension();
  if(!dim) {
    return true;
  }

  const auto& childs = dim->GetChildren();
  if(!childs.empty()) {
    dim->ClearChildren();
  }
  
  dim->CalculateDim();

  const auto& dimIndexList = dim->GetDimIndexList();
  uint32_t value = GetValue();
  uint32_t dimElementIndex = 0;
  string dimIndexText;

  for(const auto& dimIndex : dimIndexList) {
    const auto newIrq = new SvdInterrupt(dim);
    dim->AddItem(newIrq);
    CopyChilds(this, newIrq);

    newIrq->CopyItem            (this);
    newIrq->SetName             (dim->CreateName(dimIndex));
    //newIrq->SetDisplayName    (dim->CreateDisplayName(dimIndex));
    newIrq->SetDescription      (dim->CreateDescription(dimIndex));
    newIrq->SetValue            (value);
    newIrq->SetDimElementIndex  (dimElementIndex++);
    value += dim->GetDimIncrement();
  }

  string description = dim->CreateDescription("");
  dim->SetDescription(description);

  return true;
}

bool SvdInterrupt::CheckItem()
{
  if(!IsValid()) {
    return true;
  }

  const auto& name = GetName();
  const auto lineNo = GetLineNumber();
  uint32_t deviceNumInterrupts = 0;
  uint32_t maxExtIrq = 0;
  string cpuName = "<unknown>";

  const auto device = GetDevice();
  if(device) {
    const auto cpu = device->GetCpu();
    if(cpu) {
      deviceNumInterrupts = cpu->GetDeviceNumInterrupts();
      cpuName = SvdTypes::GetCpuName(cpu->GetType());
      const auto& cpuFeatures = SvdTypes::GetCpuFeatures(cpu->GetType());
      maxExtIrq = cpuFeatures.NUMEXTIRQ;
    }
    else {
      LogMsg("M390", NAME(name), NUM(480), lineNo);
      maxExtIrq = 480;
    }
  }

  const auto val = GetValue();
  if(val == SvdItem::VALUE32_NOT_INIT) {
    LogMsg("M330", NAME(name), lineNo);
    Invalidate();
    return false;
  }

  if(deviceNumInterrupts) {                     // check against <deviceNumInterrupts>
    if(deviceNumInterrupts > maxExtIrq) {       // check <deviceNumInterrupts> against architecture
      LogMsg("M389", NUM(deviceNumInterrupts), NAME(cpuName), NUM2(maxExtIrq), lineNo);      // "Specified <deviceNumInterrupts>: '%NUM%' greater or equal '%NAME%': '%NUM2%'."
      Invalidate();
    }

    if(val >= deviceNumInterrupts) {
      LogMsg("M381", NAME(name), NUM(val), NUM2(deviceNumInterrupts), lineNo);
      Invalidate();
    }
  }

  if(val >= maxExtIrq) {      // check against architecture
    LogMsg("M331", NAME(name), NUM(val), NAME2(cpuName), NUM2(maxExtIrq-1), lineNo);
    Invalidate();
  }

  return SvdItem::CheckItem();
}
