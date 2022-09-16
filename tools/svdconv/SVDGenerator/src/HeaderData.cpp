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
#include "ErrLog.h"
#include "SvdDevice.h"
#include "SvdCpu.h"
#include "SvdInterrupt.h"
#include "SvdUtils.h"

using namespace std;
using namespace c_header;


const string HeaderData::m_anonUnionStart = {
  "#if defined (__CC_ARM)"\
  "\n  #pragma push"\
  "\n  #pragma anon_unions"\
  "\n#elif defined (__ICCARM__)"\
  "\n  #pragma language=extended"\
  "\n#elif defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)"\
  "\n  #pragma clang diagnostic push"\
  "\n  #pragma clang diagnostic ignored \"-Wc11-extensions\""\
  "\n  #pragma clang diagnostic ignored \"-Wreserved-id-macro\""\
  "\n  #pragma clang diagnostic ignored \"-Wgnu-anonymous-struct\""\
  "\n  #pragma clang diagnostic ignored \"-Wnested-anon-types\""\
  "\n#elif defined (__GNUC__)"\
  "\n  /* anonymous unions are enabled by default */"\
  "\n#elif defined (__TMS470__)"\
  "\n  /* anonymous unions are enabled by default */"\
  "\n#elif defined (__TASKING__)"\
  "\n  #pragma warning 586"\
  "\n#elif defined (__CSMC__)"\
  "\n  /* anonymous unions are enabled by default */"\
  "\n#else"\
  "\n  #warning Not supported compiler type"\
  "\n#endif"\
};

const string HeaderData::m_anonUnionEnd = {
  "#if defined (__CC_ARM)"\
  "\n  #pragma pop"\
  "\n#elif defined (__ICCARM__)"\
  "\n  /* leave anonymous unions enabled */"\
  "\n#elif defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)"\
  "\n  #pragma clang diagnostic pop"\
  "\n#elif defined (__GNUC__)"\
  "\n  /* anonymous unions are enabled by default */"\
  "\n#elif defined (__TMS470__)"\
  "\n  /* anonymous unions are enabled by default */"\
  "\n#elif defined (__TASKING__)"\
  "\n  #pragma warning restore"\
  "\n#elif defined (__CSMC__)"\
  "\n  /* anonymous unions are enabled by default */"\
  "\n#endif"\
};


HeaderData::HeaderData(const FileHeaderInfo &fileHeaderInfo, const SvdOptions& options):
  m_options(options),
  m_fileIo(nullptr),
  m_gen(nullptr),
  m_bDebugHeaderfile(true),
  m_bDebugStruct(false),
  m_addressCnt(0),
  m_reservedCnt(0),         // counts "RESERVEDn number
  m_reservedFieldCnt(0),
  m_inUnion(0),
  m_addReservedBytesLater(0),
  m_prevWasStruct(0),
  m_prevWasUnion(0),
  m_structUnionPos(0),
  m_maxBitWidth(32),
  m_rootNode(nullptr)
{
  memset(&m_structUnionStack, 0, sizeof(StructUnion) * 32);
  memset(&m_regTreeNodes, 0, sizeof(RegTreeNode) * 32);     // 32 placeholder
  
  m_fileIo  = new FileIo();
  m_gen     = new HeaderGenerator(m_fileIo);

  m_gen->SetDebugHeaderfile(options.IsDebugHeaderfile());
  
  m_fileIo->SetSvdFileName        (fileHeaderInfo.svdFileName);
  m_fileIo->SetProgramDescription (fileHeaderInfo.descr);
  m_fileIo->SetCopyrightString    (fileHeaderInfo.copyright);
  m_fileIo->SetVersionString      (fileHeaderInfo.version);
  m_fileIo->SetLicenseText        (fileHeaderInfo.licenseText);
  m_fileIo->SetBriefDescription   ("CMSIS HeaderFile");
  m_fileIo->SetDeviceVersion      (fileHeaderInfo.deviceVersion);
}

HeaderData::~HeaderData()
{
  m_fileIo->Close();

  delete m_gen;
  delete m_fileIo;
}

bool HeaderData::Create(SvdItem *item, const string &fileName)
{
  m_fileIo->Create(fileName);

  const auto device = dynamic_cast<SvdDevice*>(item);
  if(!device) {
    return false;
  }

  const auto cpu = device->GetCpu();
  if(!cpu) {
    LogMsg("M209");
  }

  CreateHeaderStart           (device);
  CreateInterruptList         (device);
  CreateCmsisConfig           (device);

  if(device->GetHasAnnonUnions()) {
    CreateAnnonUnionStart     (device);
  }

  CreateClusters              (device);
  CreatePeripherals           (device);

  if(device->GetHasAnnonUnions()) {
    CreateAnnonUnionEnd       (device);
  }

  if(m_options.IsCreateMacros()) {
    CreatePosMask             (device);
  }

  if(m_options.IsCreateEnumValues()) {
    CreateEnumValue           (device);
  }
  
  CreateHeaderEnd             (device);
    
  return true;
}

bool HeaderData::CreateHeaderStart(SvdDevice *device)
{
  const auto& name    = device->GetName();
  const auto& vendor  = device->GetVendor();
  string headerDef    = name;
  SvdUtils::ToUpper(headerDef);

  m_gen->Generate<MAKE|MK_DOXYADDGROUP>("%s", vendor);
  m_gen->Generate<MAKE|MK_DOXYADDGROUP>("%s", name);
  m_gen->Generate<DIRECT              >("");
  m_gen->Generate<MAKE|MK_HEADERIFDEF >(headerDef, true);
  m_gen->Generate<MAKE|MK_CPLUSPLUS   >("", true);
  m_gen->Generate<MAKE|MK_DOXYADDGROUP>("Configuration_of_CMSIS");

  return true;
}

bool HeaderData::CreateHeaderEnd(SvdDevice *device)
{
  const auto& name    = device->GetName();
  const auto& vendor  = device->GetVendor();
  string headerDef      = name;
  SvdUtils::ToUpper(headerDef);

  m_gen->Generate<MAKE|MK_CPLUSPLUS    >("", false);
  m_gen->Generate<MAKE|MK_HEADERIFDEF  >(headerDef, false);
  m_gen->Generate<MAKE|MK_DOXYENDGROUP >("%s", name);
  m_gen->Generate<MAKE|MK_DOXYENDGROUP >("%s", vendor);

  return true;
}

bool HeaderData::CreateAnnonUnionStart(SvdDevice *device)
{
  m_gen->Generate<MAKE|MK_ANONUNION_COMPILER>(m_anonUnionStart, true);

  return true;
}

bool HeaderData::CreateAnnonUnionEnd(SvdDevice *device)
{
  m_gen->Generate<MAKE|MK_ANONUNION_COMPILER>(m_anonUnionEnd, false);

  return true;
}

bool HeaderData::CreateCmsisConfig(SvdDevice *device)
{
  const auto& name = device->GetName();
  CmsisCfg cmsisCfg;
  memset(&cmsisCfg, 0, sizeof(CmsisCfg));

  SvdCpu* cpu = device->GetCpu();
  if(cpu) {
    cmsisCfg.cpuType              = cpu->GetType();
    cmsisCfg.cpuRevision          = cpu->GetRevision();
    cmsisCfg.dcachePresent        = cpu->GetDcachePresent();
    cmsisCfg.dtcmPresent          = cpu->GetDtcmPresent();
    cmsisCfg.fpuDP                = cpu->GetFpuDP();
    cmsisCfg.fpuPresent           = cpu->GetFpuPresent();
    cmsisCfg.icachePresent        = cpu->GetIcachePresent();
    cmsisCfg.itcmPresent          = cpu->GetItcmPresent();
    cmsisCfg.mpuPresent           = cpu->GetMpuPresent();
    cmsisCfg.nvicPrioBits         = cpu->GetNvicPrioBits();
    cmsisCfg.vendorSystickConfig  = cpu->GetVendorSystickConfig();
    cmsisCfg.vtorPresent          = cpu->GetVtorPresent();
    cmsisCfg.dspPresent           = cpu->GetDspPresent();
    cmsisCfg.pmuPresent           = cpu->GetPmuPresent();
    cmsisCfg.pmuNumEventCnt       = cpu->GetPmuNumEventCounters();
    cmsisCfg.mvePresent           = cpu->GetMvePresent();
    cmsisCfg.mveFP                = cpu->GetMveFP();

    uint32_t sauTmp = cpu->GetSauNumRegions();
    if(sauTmp && sauTmp != SvdItem::VALUE32_NOT_INIT) {
      cmsisCfg.sauPresent = 1;
    }
    else {
      cmsisCfg.sauPresent = 0;
    }

    cmsisCfg.forceGeneration = cpu->GetCmsisCfgForce();
  }
  
  m_gen->Generate<MAKE|MK_CMSIS_CONFIG>("Configuration_of_CMSIS", name, cmsisCfg);

  return true;
}

bool HeaderData::CreateInterrupt(const string &name, const string &descr, int32_t num, bool lastEnum)
{
  m_gen->Generate<MAKE|MK_INTERRUPT_STRUCT>("%s_IRQn", num, lastEnum, name);
  m_gen->Generate<MAKE|MK_DOXY_COMMENT    >("%3i\t%s", num, descr);

  return true;
}

bool HeaderData::CreateInterruptList(SvdDevice *device)
{
  m_gen->Generate<DESCR|PART                  >("Interrupt Number Definition");
  m_gen->Generate<ENUM|BEGIN|TYPEDEF          >("");

  const auto cpu = device->GetCpu();
  
  // CPU specific 
  if(cpu) {
    SvdTypes::CpuType cpuType = cpu->GetType();  
    const auto& name = SvdTypes::GetCpuName(cpuType);
    m_gen->Generate<DESCR|SUBPART             >("%s Specific Interrupt Numbers", name);

    const auto& cpuInterrupts = cpu->GetInterruptList();
    for(const auto&[key, interrupt] : cpuInterrupts) {
      if(!interrupt) {
        continue;
      }

      const auto name  = interrupt->GetNameCalculated();
      const auto descr = interrupt->GetDescriptionCalculated();
      int32_t    num   = (int32_t)interrupt->GetValue() -16;  // Adjust CMSIS internal IRQ Table

      CreateInterrupt(name, descr, num, false);
    }
  }

  // Device specific 
  const auto& devName = device->GetName();
  m_gen->Generate<DESCR|SUBPART                  >("%s Specific Interrupt Numbers", devName);

  const auto& devInterrupts = device->GetInterruptList();
  uint32_t idx=0;
  auto lastIdx = devInterrupts.size() -1;

  for(const auto&[key, interrupt] : devInterrupts) {
    if(!interrupt) {
      continue;
    }

    const auto name     = interrupt->GetNameCalculated();
    const auto descr    = interrupt->GetDescriptionCalculated();
    uint32_t   num      = interrupt->GetValue();
    bool       lastEnum = idx < lastIdx? false : true;

    CreateInterrupt(name, descr, num, lastEnum);
    idx++;
  }

  m_gen->Generate<STRUCT|END|TYPEDEF             >("%s", "IRQn");

  return true;
}
