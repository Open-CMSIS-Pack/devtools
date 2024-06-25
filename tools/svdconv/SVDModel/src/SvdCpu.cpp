/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>

#include "SvdItem.h"
#include "SvdCpu.h"
#include "SvdInterrupt.h"
#include "SvdSauRegion.h"
#include "SvdUtils.h"
#include "XMLTree.h"
#include "ErrLog.h"

using namespace std;


SvdCpu::SvdCpu(SvdItem* parent):
  SvdItem(parent),
  m_type(SvdTypes::CpuType::UNDEF),
  m_revision(0),
  m_endian(SvdTypes::Endian::UNDEF),
  m_bRevision(false),
  m_mpuPresent(false),
  m_fpuPresent(false),
  m_fpuDP(false),
  m_icachePresent(false),
  m_dcachePresent(false),
  m_itcmPresent(false),
  m_dtcmPresent(false),
  m_vtorPresent(false),
  m_dspPresent(false),
  m_pmuPresent(false),
  m_mvePresent(false),
  m_mveFP(0),
  m_vendorSystickConfig(false),
  m_nvicPrioBits(SvdItem::VALUE32_NOT_INIT),
  m_sauNumRegions(SvdItem::VALUE32_NOT_INIT),
  m_pmuNumEventCnt(0),
  m_deviceNumInterrupts(0),
  m_sauRegionsConfig(nullptr)
{
  SetSvdLevel(L_Cpu);
  m_interruptList.clear();
  memset(&m_cmsisCfgForce, 0, sizeof(SvdTypes::CmsisCfgForce));
}

SvdCpu::~SvdCpu()
{
  for(auto [key, value] : m_interruptList)
    delete value;

  delete m_sauRegionsConfig;
  //m_interruptList.clear();
}

bool SvdCpu::Calculate()
{
  uint32_t revision;
  const auto& revStr = GetRevisionStr();

  if(revStr != "") {
    if(SvdUtils::ConvertCpuRevision(revStr.c_str(), revision) == false) {
      LogMsg("M204", VALUE(revStr), GetLineNumber());
    }
    else {
      SetRevision(revision);
      m_bRevision = true;
    }
  }

  CreateInterruptList();

  return SvdItem::Calculate();
}

bool SvdCpu::Construct(XMLTreeElement* xmlElement)
{
  return SvdItem::Construct(xmlElement);
}

bool SvdCpu::ProcessXmlElement(XMLTreeElement* xmlElement)
{
	const auto& tag   = xmlElement->GetTag();
	const auto& value = xmlElement->GetText();
        auto& cmsisCfgForce = GetCmsisCfgForce();

  if(tag == "name") {
    if(!SvdUtils::ConvertCpuType(value, m_type)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    else {
      SetName(SvdTypes::GetCpuType(m_type));
    }
    return true;
  }
  else if(tag == "revision") {
    m_revisionStr = value;
    return true;
  }
  else if(tag == "endian") {
    if(!SvdUtils::ConvertCpuEndian(value, m_endian, xmlElement->GetLineNumber())) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    return true;
  }
  else if(tag == "mpuPresent") {
    if(!SvdUtils::ConvertNumber (value, m_mpuPresent)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    cmsisCfgForce.bMpuPresent = 1;
    return true;
  }
  else if(tag == "fpuPresent") {
    if(!SvdUtils::ConvertNumber (value, m_fpuPresent)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    cmsisCfgForce.bFpuPresent = 1;
    return true;
  }
  else if(tag == "fpuDP") {
    if(!SvdUtils::ConvertNumber (value, m_fpuDP)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    cmsisCfgForce.bFpuDP = 1;
    return true;
  }
  else if(tag == "nvicPrioBits") {
    if(!SvdUtils::ConvertNumber(value, m_nvicPrioBits)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    return true;
  }
  else if(tag == "vendorSystickConfig") {
    if(!SvdUtils::ConvertNumber (value, m_vendorSystickConfig)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    return true;
  }
  else if(tag == "icachePresent") {
    if(!SvdUtils::ConvertNumber (value, m_icachePresent)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    cmsisCfgForce.bIcachePresent = 1;
    return true;
  }
  else if(tag == "dcachePresent") {
    if(!SvdUtils::ConvertNumber (value, m_dcachePresent)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    cmsisCfgForce.bDcachePresent = 1;
    return true;
  }
  else if(tag == "itcmPresent") {
    if(!SvdUtils::ConvertNumber (value, m_itcmPresent)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    cmsisCfgForce.bItcmPresent = 1;
    return true;
  }
  else if(tag == "vtorPresent") {
    if(!SvdUtils::ConvertNumber (value, m_vtorPresent)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    cmsisCfgForce.bVtorPresent = 1;
    return true;
  }
  else if(tag == "dspPresent") {
    if(!SvdUtils::ConvertNumber (value, m_dspPresent)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    cmsisCfgForce.bDspPresent = 1;
    return true;
  }
  else if(tag == "dtcmPresent") {
    if(!SvdUtils::ConvertNumber (value, m_dtcmPresent)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    cmsisCfgForce.bDtcmPresent = 1;
    return true;
  }
  else if(tag == "deviceNumInterrupts") {
    if(!SvdUtils::ConvertNumber(value, m_deviceNumInterrupts)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    return true;
  }
  else if(tag == "sauNumRegions") {
    if(!SvdUtils::ConvertNumber(value, m_sauNumRegions)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    cmsisCfgForce.bSauPresent = 1;
    return true;
  }
  else if(tag == "sauRegionsConfig") {
    if(!m_sauRegionsConfig) {
      m_sauRegionsConfig = new SvdSauRegionsConfig(this);
    }
    cmsisCfgForce.bSauPresent = 1;
		return m_sauRegionsConfig->Construct(xmlElement);
	}
  else if(tag == "pmuPresent") {
    if(!SvdUtils::ConvertNumber (value, m_pmuPresent)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    cmsisCfgForce.bPmuPresent = 1;
    return true;
  }
  else if(tag == "pmuNumEventCnt") {
    if(!SvdUtils::ConvertNumber(value, m_pmuNumEventCnt)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    return true;
  }
  else if(tag == "mvePresent") {
    if(!SvdUtils::ConvertNumber (value, m_mvePresent)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    cmsisCfgForce.bMvePresent = 1;
    return true;
  }
  else if(tag == "mveFP") {
    if(!SvdUtils::ConvertNumber (value, m_mveFP)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    cmsisCfgForce.bMveFP = 1;
    return true;
  }

  return SvdItem::ProcessXmlElement(xmlElement);
}

bool SvdCpu::ProcessXmlAttributes(XMLTreeElement* xmlElement)
{
  return SvdItem::ProcessXmlAttributes(xmlElement);
}

bool SvdCpu::CopyItem(SvdItem *from)
{
  const auto pFrom = (SvdCpu*) from;

  const auto&  revisionStr         = GetRevisionStr          ();
  const auto   revision            = GetRevision             ();
  const auto   nvicPrioBits        = GetNvicPrioBits         ();
  const auto   mpuPresent          = GetMpuPresent           ();
  const auto   fpuPresent          = GetFpuPresent           ();
  const auto   vendorSystickConfig = GetVendorSystickConfig  ();
  const auto   fpuDP               = GetFpuDP                ();
  const auto   icachePresent       = GetIcachePresent        ();
  const auto   dcachePresent       = GetDcachePresent        ();
  const auto   itcmPresent         = GetItcmPresent          ();
  const auto   dtcmPresent         = GetDtcmPresent          ();
  const auto   vtorPresent         = GetVtorPresent          ();
  const auto   endian              = GetEndian               ();
  const auto   type                = GetType                 ();

  if(revisionStr         == "")                       { SetRevisionStr          (pFrom->GetRevisionStr          ()); }
  if(revision            == 0 )                       { SetRevision             (pFrom->GetRevision             ()); }
  if(nvicPrioBits        == 0 )                       { SetNvicPrioBits         (pFrom->GetNvicPrioBits         ()); }
  if(mpuPresent          == 0 )                       { SetMpuPresent           (pFrom->GetMpuPresent           ()); }
  if(fpuPresent          == 0 )                       { SetFpuPresent           (pFrom->GetFpuPresent           ()); }
  if(vendorSystickConfig == 0 )                       { SetVendorSystickConfig  (pFrom->GetVendorSystickConfig  ()); }
  if(fpuDP               == 0 )                       { SetFpuDP                (pFrom->GetFpuDP                ()); }
  if(icachePresent       == 0 )                       { SetIcachePresent        (pFrom->GetIcachePresent        ()); }
  if(dcachePresent       == 0 )                       { SetDcachePresent        (pFrom->GetDcachePresent        ()); }
  if(itcmPresent         == 0 )                       { SetItcmPresent          (pFrom->GetItcmPresent          ()); }
  if(dtcmPresent         == 0 )                       { SetDtcmPresent          (pFrom->GetDtcmPresent          ()); }
  if(vtorPresent         == 0 )                       { SetVtorPresent          (pFrom->GetVtorPresent          ()); }
  if(endian              == SvdTypes::Endian::UNDEF)  {  SetEndian              (pFrom->GetEndian               ()); }
  if(type                == SvdTypes::CpuType::UNDEF) { SetType                 (pFrom->GetType                 ()); }

  SvdItem::CopyItem(from);

  return false;
}

bool SvdCpu::CreateInterruptList()
{
  const auto cpuType = GetType();
  string name, descr;

  for(auto i=(uint32_t)SvdTypes::CpuIrqNum::IRQ0; i<(uint32_t)SvdTypes::CpuIrqNum::IRQ_END; i++) {
    if(SvdTypes::GetCortexMInterruptAvailable(cpuType, (SvdTypes::CpuIrqNum)i)) {    // filter available Interrupts
      if(i == 15 && GetVendorSystickConfig()) {    // 28.11.2016 SVDConv v3.2.56 fixed: when <vendorSystickConfig> is set, no SysTick IRQ is generated
        continue;
      }

      SvdTypes::GetCortexMInterrupt(cpuType, (SvdTypes::CpuIrqNum)i, name, descr);
      auto interrupt = new SvdInterrupt(this);
      interrupt->SetName(name);
      interrupt->SetValue(i);
      if(descr.empty()) {
        interrupt->SetDescription(name);
      }
      else {
        interrupt->SetDescription(descr);
      }

      m_interruptList[i] = interrupt;
    }
  }

  return true;
}

bool SvdCpu::CheckItem()
{
  const auto lineNo  = GetLineNumber();

  if(!IsValid()) {
    return true;
  }

  if(!m_bRevision) {
    LogMsg("M325", lineNo);
  }

  if(m_endian == SvdTypes::Endian::UNDEF) {
    LogMsg("M326", lineNo);
    m_endian = SvdTypes::Endian::LITTLE;
  }

  if(m_nvicPrioBits == SvdItem::VALUE32_NOT_INIT || m_nvicPrioBits < 2 || m_nvicPrioBits > 8) {
    LogMsg("M327", lineNo);
    m_nvicPrioBits = 4;
  }

  if(m_type == SvdTypes::CpuType::UNDEF) {
    LogMsg("M329", lineNo);
    m_type = SvdTypes::CpuType::CM3;
  }

  if(m_sauRegionsConfig) {
    if(m_sauNumRegions == SvdItem::VALUE32_NOT_INIT) {
      LogMsg("M363", lineNo);
      m_sauRegionsConfig->Invalidate();
    }
    else if(m_sauNumRegions == 0) {
      LogMsg("M387", lineNo);
      m_sauRegionsConfig->Invalidate();
    }
  }

  if(m_sauNumRegions != SvdItem::VALUE32_NOT_INIT) {
    if(m_sauNumRegions > MAXNUM_SAU_REGIONS) {
      LogMsg("M364", NUM(m_sauNumRegions), NUM2(MAXNUM_SAU_REGIONS), lineNo);
      m_sauNumRegions = SvdItem::VALUE32_NOT_INIT;
      if(m_sauRegionsConfig) {
        m_sauRegionsConfig->Invalidate();
      }
    }
    else if(m_sauRegionsConfig && m_sauNumRegions < m_sauRegionsConfig->GetChildCount()) {
      LogMsg("M391", NUM(m_sauRegionsConfig->GetChildCount()), NUM2(m_sauNumRegions),lineNo);
    }
  }

  if(m_pmuPresent) {
    const auto& cpuFeatures = SvdTypes::GetCpuFeatures(m_type);
    if(!cpuFeatures.PMU) {
      const auto& name = SvdTypes::GetCpuName (m_type);
      LogMsg("M385", NAME(name)); // Error: PMU: Not supported for CPU 'name'
      m_pmuPresent = 0;
      m_cmsisCfgForce.bPmuPresent = 0;
    }
    else if(m_pmuNumEventCnt < 2 || m_pmuNumEventCnt > 32) {
      LogMsg("M384", NUM(m_pmuNumEventCnt), lineNo); // Error: PMU: Number of Event Count not set or outside range [2..31]. Ignoring PMU entry.
      m_pmuPresent = 0;
    }
  }
  else if(m_pmuNumEventCnt) {
    LogMsg("M383"); // Error: PMU: Number of Event Count set but PMU present not set
  }

  if(m_mvePresent) {
    if(m_mveFP && !m_mvePresent) {
      LogMsg("M388");
    }
  }

  return SvdItem::CheckItem();
}
