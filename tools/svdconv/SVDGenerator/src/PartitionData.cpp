/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdarg.h>
#include "PartitionData.h"
#include "HeaderGenerator.h"
#include "SfdGenerator.h"
#include "SvdItem.h"
#include "FileIo.h"
#include "ErrLog.h"
#include "SvdDevice.h"
#include "SvdCpu.h"
#include "SvdInterrupt.h"
#include "SvdSauRegion.h"
#include "SvdTypes.h"
#include "SvdUtils.h"
#include "HeaderGenAPI.h"
#include "SfdGenAPI.h"

using namespace std;


const string PartitionData::CFG_BEGIN = "*** <<< Use Configuration Wizard in Context Menu >>> ***";
const string PartitionData::CFG_END   = "*** <<< end of configuration section >>> ***";
const string PartitionData::sauInitRegionMacro = \
  "#define SAU_INIT_REGION(n) \\\n"\
  "    SAU->RNR  =  (n                                     & SAU_RNR_REGION_Msk);  \\\n"\
  "    SAU->RBAR =  (SAU_INIT_START##n                     & SAU_RBAR_BADDR_Msk);  \\\n"\
  "    SAU->RLAR =  (SAU_INIT_END##n                       & SAU_RLAR_LADDR_Msk) | \\\n"\
  "                ((SAU_INIT_NSC##n << SAU_RLAR_NSC_Pos)  & SAU_RLAR_NSC_Msk)   | 1U\n";


PartitionData::PartitionData(const FileHeaderInfo& fileHeaderInfo, const SvdOptions& options):
  m_options(options),
  m_fileIo(nullptr),
  m_genH(nullptr),
  m_genSfd(nullptr),
  m_numOfItns(0)
{
  m_fileIo  = new FileIo();
  m_genH    = new HeaderGenerator(m_fileIo);
  m_genSfd  = new SfdGenerator(m_fileIo);

  m_fileIo->SetSvdFileName        (fileHeaderInfo.svdFileName);
  m_fileIo->SetProgramDescription (fileHeaderInfo.descr);
  m_fileIo->SetCopyrightString    (fileHeaderInfo.copyright);
  m_fileIo->SetVersionString      (fileHeaderInfo.version);
  m_fileIo->SetLicenseText        (fileHeaderInfo.licenseText);
  m_fileIo->SetDeviceVersion      (fileHeaderInfo.deviceVersion);
  m_fileIo->SetBriefDescription   ("CMSIS-Core: Security Attribution Unit (SAU) configuration");
}

PartitionData::~PartitionData()
{
  m_fileIo->Close();

  delete m_genH;
  delete m_genSfd;
  delete m_fileIo;
}

bool PartitionData::Create(SvdItem *item, const string& fileName)
{
  m_fileIo->Create(fileName);

  const auto device = dynamic_cast<SvdDevice*>(item);
  if(!device) {
    return false;
  }

  const auto cpu = device->GetCpu();
  if(!cpu) {
    //LogMsg("M209");
    return false;
  }

  CreatePartitionStart            (device);

  CreateSauGlobalConfig           (cpu);
  CreateSauRegionsConfig          (cpu);
  CreateSleepAndExceptionHandling (cpu);

  const auto cpuType = cpu->GetType();
  if(cpuType == SvdTypes::CpuType::CM23 || cpuType == SvdTypes::CpuType::V8MBL) {    // BaseLine has configurable SysTick
    CreateSingleSysTick           (cpu);
  }

  if(cpu->GetFpuPresent()) {
    CreateFloatingPointUnit       (cpu);
  }

  CreateSetupInterruptTarget      (device);
  CreateConfWizEnd                ();

  CreateSauRegions                (cpu);

  CreatePartitionEnd              (device);

  return true;
}

bool PartitionData::CreateHeadingBegin(const string& text)
{
  CreateCCommentBegin();
  m_genSfd->Generate<sfd::HEADING|sfd::BEGIN>(text.c_str());
  CreateCCommentEnd();

  return true;
}

bool PartitionData::CreateHeadingEnd()
{
  CreateCCommentBegin();
  m_genSfd->Generate<sfd::HEADING|sfd::END>("");
  CreateCCommentEnd();
  m_genH->Generate<c_header::DIRECT>("");

  return true;
}

bool PartitionData::CreateHeadingEnableBegin(const string& text)
{
  CreateCCommentBegin();
  m_genSfd->Generate<sfd::HEADINGENABLE|sfd::BEGIN>(text.c_str());
  CreateCCommentEnd();

  return true;
}

bool PartitionData::CreateHeadingEnableEnd()
{
  CreateCCommentBegin();
  m_genSfd->Generate<sfd::HEADINGENABLE|sfd::END>("");
  CreateCCommentEnd();
  m_genH->Generate<c_header::DIRECT>("");

  return true;
}

bool PartitionData::CreateConfWizStart()
{
  CreateCCommentBegin();
  m_genH->Generate<c_header::C_COMMENT>(CFG_BEGIN);
  CreateCCommentEnd();

  return true;
}

bool PartitionData::CreateConfWizEnd()
{
  CreateCCommentBegin();
  m_genH->Generate<c_header::C_COMMENT>(CFG_END);
  CreateCCommentEnd();

  return true;
}

void PartitionData::CreateCCommentBegin()
{
  m_genH->Generate<c_header::C_COMMENT|c_header::BEGIN>("");
}

void PartitionData::CreateCCommentEnd()
{
  m_genH->Generate<c_header::C_COMMENT|c_header::END>("");
}

bool PartitionData::CreatePartitionStart(SvdDevice *device)
{
  if(!device) {
    return false;
  }

  const auto& name    = device->GetName();
  const auto& vendor  = device->GetVendor();
  string headerDef    = name;

  SvdUtils::ToUpper(headerDef);
  CreateConfWizStart();

  m_genH->Generate<c_header::MAKE|c_header::MK_DOXYADDGROUP         >("%s Partition", vendor.c_str());
  m_genH->Generate<c_header::MAKE|c_header::MK_DOXYADDGROUP         >("%s Partition", name.c_str());
  m_genH->Generate<c_header::MAKE|c_header::MK_HEADERIFDEF          >("%s_PARTITION", true, headerDef.c_str());
  m_genH->Generate<c_header::MAKE|c_header::MK_HEADEREXTERNC_BEGIN  >("");

  return true;
}

bool PartitionData::CreatePartitionEnd(SvdDevice *device)
{
  if(!device) {
    return false;
  }

  const auto& name    = device->GetName();
  const auto& vendor  = device->GetVendor();
  string headerDef    = name;
  SvdUtils::ToUpper(headerDef);

  m_genH->Generate<c_header::MAKE|c_header::MK_HEADEREXTERNC_END    >("");
  m_genH->Generate<c_header::MAKE|c_header::MK_HEADERIFDEF          >("%s_PARTITION", false, headerDef.c_str());
  m_genH->Generate<c_header::MAKE|c_header::MK_DOXYENDGROUP         >("%s",           name.c_str());
  m_genH->Generate<c_header::MAKE|c_header::MK_DOXYENDGROUP         >("%s",           vendor.c_str());
  m_genH->Generate<c_header::DIRECT                                 >("");

  return true;
}

bool PartitionData::CreateSauRegionsConfig(SvdCpu *cpu)
{
  if(!cpu) {
    return false;
  }

  m_genH->Generate<c_header::DESCR|c_header::PART>("SAU Regions Config");

  CreateHeadingBegin      ("Initialize Secure Attribute Unit (SAU) Address Regions");
  CreateMaxNumSauRegions  (cpu);
  CreateInitSauRegions    (cpu);
  CreateHeadingEnd        ();

  return true;
}

bool PartitionData::CreateMaxNumSauRegions(SvdCpu *cpu)
{
  if(!cpu) {
    return false;
  }

  uint32_t value = cpu->GetSauNumRegions();
  const string name  = "SAU_REGIONS_MAX";
  const string descr = "Max. number of SAU regions";

  if(value == SvdItem::VALUE32_NOT_INIT) {
    m_genH->Generate<c_header::C_ERROR>("SAU Regions Config: Number of SAU regions not set", cpu->GetLineNumber());
    value = 0;
  }

  m_genH->Generate<c_header::MAKE|c_header::MK_DEFINE>("%s", value, 10, -1, descr.c_str(), name.c_str());

  return true;
}

bool PartitionData::CreateSauGlobalConfig(SvdCpu *cpu)
{
  if(!cpu) {
    return false;
  }

  m_genH->Generate<c_header::DESCR|c_header::PART>("SAU Config");

  CreateSauInitControl        (cpu);
  CreateSauInitControlEnable  (cpu);
  CreateSauAllNonSecure       (cpu);
  CreateHeadingEnableEnd      ();

  return true;
}

bool PartitionData::CreateSauInitControl(SvdCpu *cpu)
{
  if(!cpu) {
    return false;
  }

  uint32_t value = 1;
  const string name  = "SAU_INIT_CTRL";
  const string descr = ""; //SAU CTRL register enable";

  CreateHeadingEnableBegin("Initialize Secure Attribute Unit (SAU) CTRL register");
  m_genH->Generate<c_header::MAKE|c_header::MK_DEFINE>("%s", value, 10, -1, descr.c_str(), name.c_str());

  return true;
}

bool PartitionData::CreateSauInitControlEnable(SvdCpu *cpu)
{
  if(!cpu) {
    return false;
  }

  const auto sauRegionsCfg = cpu->GetSauRegionsConfig();
  if(!sauRegionsCfg) {
    return true;
  }

  bool value = sauRegionsCfg->GetEnabled();
  const string name = "SAU_INIT_CTRL_ENABLE";
  const string descr = "SAU->CTRL register bit ENABLE";

  CreateCCommentBegin();
  m_genSfd->Generate<sfd::QOPTION|sfd::SINGLE             >("Enable SAU");
  m_genSfd->Generate<sfd::INFO|sfd::SINGLE                >("%s", descr.c_str());
  CreateCCommentEnd();

  m_genH->Generate<c_header::MAKE|c_header::MK_DEFINE     >("%s", value, 10, -1, descr.c_str(), name.c_str());

  return true;
}

bool PartitionData::CreateSauAllNonSecure(SvdCpu *cpu)
{
  if(!cpu) {
    return false;
  }

  const auto sauRegionsCfg = cpu->GetSauRegionsConfig();
  if(!sauRegionsCfg) {
    return true;
  }

  const auto sauProtWDis = sauRegionsCfg->GetProtectionWhenDisabled();
  uint32_t value = 0;
  if(sauProtWDis == SvdTypes::ProtectionType::NONSECURE) {
    value = 1;
  }

  const string name = "SAU_INIT_CTRL_ALLNS";
  const string descr = "Value for SAU->CTRL register bit ALLNS";

  CreateCCommentBegin();
  m_genSfd->Generate<sfd::OPTION|sfd::SINGLE          >("When SAU is disabled");
  m_genSfd->Generate<sfd::MAKE|sfd::MK_ENABLEDISABLE  >("", "All Memory is Secure", "All Memory is Non-Secure");
  m_genSfd->Generate<sfd::INFO|sfd::SINGLE            >("%s", descr.c_str());
  m_genSfd->Generate<sfd::INFO|sfd::SINGLE            >("When all Memory is Non-Secure (ALLNS is 1), IDAU can override memory map configuration.");
  CreateCCommentEnd();

  m_genH->Generate<c_header::MAKE|c_header::MK_DEFINE >("%s", value, 10, -1, descr.c_str(), name.c_str());

  return true;
}

bool PartitionData::CreateInitSauRegions(SvdCpu *cpu)
{
  if(!cpu) {
    return false;
  }

  const auto sauRegionsCfg = cpu->GetSauRegionsConfig();
  if(!sauRegionsCfg) {
    return true;
  }

  const auto& childs = sauRegionsCfg->GetChildren();
  if(childs.empty()) {
    return true;
  }

  //m_genH->Generate<c_header::DESCR|c_header::PART>("SAU Regions");
  //CreateHeadingBegin("Initialize Secure Attribute Unit (SAU) Address Regions");

  int i=0;
  for(const auto child : childs) {
    const auto region = dynamic_cast<SvdSauRegion*>(child);
    if(!region) {
      continue;
    }

    CreateInitSauRegionNumber(region, i++);
  }

  return true;
}

bool PartitionData::CreateInitSauRegionNumber(SvdSauRegion* region, uint32_t regionNumber)
{
  if(!region) {
    return false;
  }

  string name, descr;
  m_genH->Generate<c_header::DESCR|c_header::SUBPART      >("SAU Region %d", regionNumber);

  // SAU_INIT_REGION
  bool regionEnable = region->GetEnabled();
  name = "SAU_INIT_REGION";
  descr = "Setup SAU Region ";
  descr += SvdUtils::CreateDecNum(regionNumber);
  descr += " memory attributes";

  CreateCCommentBegin();
  m_genSfd->Generate<sfd::HEADINGENABLE|sfd::BEGIN        >("Initialize SAU Region %i",               regionNumber);
  m_genSfd->Generate<sfd::INFO|sfd::SINGLE                >("Setup SAU Region %i memory attributes",  regionNumber);
  CreateCCommentEnd();
  m_genH->Generate<c_header::MAKE|c_header::MK_DEFINE     >("%s%d",                                   regionEnable, 10, -1, descr.c_str(), name.c_str(), regionNumber);

  // SAU_INIT_START
  uint32_t startAddr = region->GetBase();
  name = "SAU_INIT_START";
  descr = "Start Address of SAU region ";
  descr += SvdUtils::CreateDecNum(regionNumber);

  CreateCCommentBegin();
  m_genSfd->Generate<sfd::OPTION|sfd::SINGLE              >("Start Address <0-0xFFFFFFE0>");
  m_genSfd->Generate<sfd::INFO|sfd::SINGLE                >("Start address of SAU region %i",         regionNumber);
  CreateCCommentEnd();
  m_genH->Generate<c_header::MAKE|c_header::MK_DEFINE     >("%s%d",                                   startAddr, 16, -1, descr.c_str(), name.c_str(), regionNumber);

  // SAU_INIT_END
  uint32_t endAddr = region->GetLimit();
  name = "SAU_INIT_END";
  descr = "End Address of SAU region ";
  descr += SvdUtils::CreateDecNum(regionNumber);
  CreateCCommentBegin();
  m_genSfd->Generate<sfd::OPTION|sfd::SINGLE              >("End Address <0x1F-0xFFFFFFFF>");
  m_genSfd->Generate<sfd::INFO|sfd::SINGLE                >("End Address of SAU region %i",           regionNumber);
  CreateCCommentEnd();
  m_genH->Generate<c_header::MAKE|c_header::MK_DEFINE     >("%s%d",                                   endAddr, 16, -1, descr.c_str(), name.c_str(), regionNumber);

  // Region secure / non secure
  const auto sauAccessType = region->GetAccessType();
  uint32_t regionIsSecure = 0;
  if(sauAccessType == SvdTypes::SauAccessType::NONSECURE) {
    regionIsSecure = 0;
  }
  else if(sauAccessType == SvdTypes::SauAccessType::SECURE) {
    regionIsSecure = 1;
  }

  name = "SAU_INIT_NSC";
  descr = "Region State (secure, non secure)";
  CreateCCommentBegin();
  m_genSfd->Generate<sfd::OPTION|sfd::SINGLE              >("Region %d is",                           regionNumber);
  m_genSfd->Generate<sfd::INFO|sfd::SINGLE                >("Region State (secure, non secure)");
  m_genSfd->Generate<sfd::MAKE|sfd::MK_ENABLEDISABLE      >("", "Non-Secure", "Secure, Non-Secure Callable");
  CreateCCommentEnd();
  m_genH->Generate<c_header::MAKE|c_header::MK_DEFINE     >("%s%d",                                   regionIsSecure, 10, -1, descr.c_str(), name.c_str(), regionNumber);
  CreateHeadingEnableEnd();

  m_genH->Generate<c_header::DIRECT                       >("");

  return true;
}

bool PartitionData::CreateSleepAndExceptionHandling(SvdCpu *cpu)
{
  if(!cpu) {
    return false;
  }

  m_genH->Generate<c_header::DESCR|c_header::PART>("SAU Sleep and Exception Handling");

  CreateSleepAndExceptionBegin    (cpu);
  CreateDeepSleep                 (cpu);
  CreateSystemReset               (cpu);
  CreatePriorityExceptions        (cpu);
  CreateFault                     (cpu);
  CreateHeadingEnableEnd          ();

  return true;
}

bool PartitionData::CreateSleepAndExceptionBegin(SvdCpu *cpu)
{
  if(!cpu) {
    return false;
  }

  uint32_t value = 1;
  string name = "SCB_CSR_AIRCR_INIT";
  string descr = "";

  CreateCCommentBegin();
  m_genSfd->Generate<sfd::HEADINGENABLE|sfd::BEGIN        >("Setup behaviour of Sleep and Exception Handling");
  //m_genSfd->Generate<sfd::INFO|sfd::SINGLE              >("%s", descr.c_str());
  CreateCCommentEnd();
  m_genH->Generate<c_header::MAKE|c_header::MK_DEFINE     >("%s", value, 10, -1, descr.c_str(), name.c_str());

  return true;
}

bool PartitionData::CreateDeepSleep(SvdCpu *cpu)
{
  if(!cpu) {
    return false;
  }

  uint32_t value = 1;
  string name = "SCB_CSR_DEEPSLEEPS_VAL";
  string descr = "Value for SCB->CSR register bit DEEPSLEEPS";

  CreateCCommentBegin();
  m_genSfd->Generate<sfd::OPTION|sfd::SINGLE              >("Deep Sleep can be enabled by");
  m_genSfd->Generate<sfd::MAKE|sfd::MK_ENABLEDISABLE      >("", "Secure and Non-Secure state", "Secure state only");
  m_genSfd->Generate<sfd::INFO|sfd::SINGLE                >("%s",                                       descr.c_str());
  CreateCCommentEnd();
  m_genH->Generate<c_header::MAKE|c_header::MK_DEFINE     >("%s",                                       value, 10, -1, descr.c_str(), name.c_str());

  return true;
}

bool PartitionData::CreateSystemReset(SvdCpu *cpu)
{
  if(!cpu) {
    return false;
  }

  uint32_t value = 1;
  string name = "SCB_AIRCR_SYSRESETREQS_VAL";
  string descr = "Value for SCB->AIRCR register bit SYSRESETREQS";

  CreateCCommentBegin();
  m_genSfd->Generate<sfd::OPTION|sfd::SINGLE              >("System reset request accessible from");
  m_genSfd->Generate<sfd::MAKE|sfd::MK_ENABLEDISABLE      >("", "Secure and Non-Secure state", "Secure state only");
  m_genSfd->Generate<sfd::INFO|sfd::SINGLE                >("%s",                                       descr.c_str());
  CreateCCommentEnd();
  m_genH->Generate<c_header::MAKE|c_header::MK_DEFINE     >("%s",                                       value, 10, -1, descr.c_str(), name.c_str());

  return true;
}

bool PartitionData::CreatePriorityExceptions(SvdCpu *cpu)
{
  if(!cpu) {
    return false;
  }

  uint32_t value = 1;
  string name = "SCB_AIRCR_PRIS_VAL";
  string descr = "Value for SCB->AIRCR register bit PRIS";

  CreateCCommentBegin();
  m_genSfd->Generate<sfd::OPTION|sfd::SINGLE              >("Priority of Non-Secure exceptions is");
  m_genSfd->Generate<sfd::MAKE|sfd::MK_ENABLEDISABLE      >("", "Not altered", "Lowered to 0x80-0xFF");
  m_genSfd->Generate<sfd::INFO|sfd::SINGLE                >("%s",                                       descr.c_str());
  CreateCCommentEnd();
  m_genH->Generate<c_header::MAKE|c_header::MK_DEFINE     >("%s",                                       value, 10, -1, descr.c_str(), name.c_str());

  return true;
}

bool PartitionData::CreateFault(SvdCpu *cpu)
{
  if(!cpu) {
    return false;
  }

  uint32_t value = 0;
  string name = "SCB_AIRCR_BFHFNMINS_VAL";
  string descr = "Value for SCB->AIRCR register bit BFHFNMINS";

  CreateCCommentBegin();
  m_genSfd->Generate<sfd::OPTION|sfd::SINGLE              >("BusFault, HardFault, and NMI target");
  m_genSfd->Generate<sfd::MAKE|sfd::MK_ENABLEDISABLE      >("", "Secure state", "Non-Secure state");
  m_genSfd->Generate<sfd::INFO|sfd::SINGLE                >("%s",                                       descr.c_str());
  CreateCCommentEnd();
  m_genH->Generate<c_header::MAKE|c_header::MK_DEFINE     >("%s",                                       value, 10, -1, descr.c_str(), name.c_str());

  return true;
}

bool PartitionData::CreateSingleSysTick(SvdCpu *cpu)
{
  if(!cpu) {
    return false;
  }

  m_genH->Generate<c_header::DESCR|c_header::PART >("Setup behavior of single SysTick");

  CreateSingleSysTickBegin(cpu);
  CreateSingleSysTickIcsr(cpu);
  CreateHeadingEnableEnd();

  return true;
}

bool PartitionData::CreateSingleSysTickBegin(SvdCpu *cpu)
{
  if(!cpu) {
    return false;
  }

  uint32_t value = 0;
  string name  = "SCB_ICSR_INIT";
  string descr = "";

  CreateCCommentBegin();
  m_genSfd->Generate<sfd::HEADINGENABLE|sfd::BEGIN        >("Setup behaviour of single SysTick");
  m_genSfd->Generate<sfd::INFO|sfd::SINGLE                >("%s", descr.c_str());
  CreateCCommentEnd();
  m_genH->Generate<c_header::MAKE|c_header::MK_DEFINE     >("%s", value, 10, -1, descr.c_str(), name.c_str());

  return true;
}

bool PartitionData::CreateSingleSysTickIcsr(SvdCpu *cpu)
{
  if(!cpu) {
    return false;
  }

  uint32_t value = 0;
  string name = "SCB_ICSR_STTNS_VAL";
  string descr = "";

  CreateCCommentBegin();
  m_genSfd->Generate<sfd::OPTION|sfd::SINGLE              >("In a single SysTick implementation, SysTick is");
  m_genSfd->Generate<sfd::MAKE|sfd::MK_ENABLEDISABLE      >("", "Secure state", "Non-Secure");
  m_genSfd->Generate<sfd::INFO|sfd::SINGLE                >("Value for SCB->ICSR register bit STTNS");
  m_genSfd->Generate<sfd::INFO|sfd::SINGLE                >("only for single SysTick implementation");
  CreateCCommentEnd();
  m_genH->Generate<c_header::MAKE|c_header::MK_DEFINE     >("%s",                                       value, 10, -1, descr.c_str(), name.c_str());

  return true;
}

bool PartitionData::CreateFloatingPointUnit(SvdCpu *cpu)
{
  if(!cpu) {
    return false;
  }

  m_genH->Generate<c_header::DESCR|c_header::PART >("Setup behavior of Floating Point Unit");

  CreateFloatingPointUnitBegin(cpu);
  CreateFloatingPointUnitNsacrCp10Cp11(cpu);
  CreateFloatingPointUnitFpccrTs(cpu);
  CreateFloatingPointUnitFpccrClrOnRetS(cpu);
  CreateFloatingPointUnitFpccrClrOnRet(cpu);
  CreateHeadingEnableEnd();

  return true;
}

bool PartitionData::CreateFloatingPointUnitBegin(SvdCpu *cpu)
{
  if(!cpu) {
    return false;
  }

  uint32_t value = 0;
  string name  = "TZ_FPU_NS_USAGE";
  string descr = "";

  CreateCCommentBegin();
  m_genSfd->Generate<sfd::HEADINGENABLE|sfd::BEGIN        >("Setup behavior of Floating Point Unit");
  //m_genSfd->Generate<sfd::INFO|sfd::SINGLE              >("%s", descr.c_str());
  CreateCCommentEnd();
  m_genH->Generate<c_header::MAKE|c_header::MK_DEFINE     >("%s", value, 10, -1, descr.c_str(), name.c_str());

  return true;
}

bool PartitionData::CreateFloatingPointUnitNsacrCp10Cp11(SvdCpu *cpu)
{
  if(!cpu) {
    return false;
  }

  uint32_t value = 0;
  string name = "SCB_NSACR_CP10_11_VAL";
  string descr = "";

  CreateCCommentBegin();
  m_genSfd->Generate<sfd::OPTION|sfd::SINGLE              >("Floating Point Unit usage");
  m_genSfd->Generate<sfd::MAKE|sfd::MK_OPTSEL             >("Secure state only", 0);
  m_genSfd->Generate<sfd::MAKE|sfd::MK_OPTSEL             >("Secure and Non-Secure state", 3);
  m_genSfd->Generate<sfd::INFO|sfd::SINGLE                >("Value for SCB->NSACR register bits CP10, CP11");
  CreateCCommentEnd();
  m_genH->Generate<c_header::MAKE|c_header::MK_DEFINE     >("%s",                                       value, 10, -1, descr.c_str(), name.c_str());

  return true;
}

bool PartitionData::CreateFloatingPointUnitFpccrTs(SvdCpu *cpu)
{
  if(!cpu) {
    return false;
  }

  uint32_t value = 0;
  string name = "FPU_FPCCR_TS_VAL";
  string descr = "";

  CreateCCommentBegin();
  m_genSfd->Generate<sfd::OPTION|sfd::SINGLE              >("Treat floating-point registers as Secure"     );
  m_genSfd->Generate<sfd::MAKE|sfd::MK_ENABLEDISABLE      >("", "Disabled", "Enabled");
  m_genSfd->Generate<sfd::INFO|sfd::SINGLE                >("Value for FPU->FPCCR register bit TS"         );
  CreateCCommentEnd();
  m_genH->Generate<c_header::MAKE|c_header::MK_DEFINE     >("%s",                                       value, 10, -1, descr.c_str(), name.c_str());

  return true;
}

bool PartitionData::CreateFloatingPointUnitFpccrClrOnRetS(SvdCpu *cpu)
{
  if(!cpu) {
    return false;
  }

  uint32_t value = 0;
  string name = "FPU_FPCCR_CLRONRETS_VAL";
  string descr = "";

  CreateCCommentBegin();
  m_genSfd->Generate<sfd::OPTION|sfd::SINGLE              >("Clear on return (CLRONRET) accessibility"     );
  m_genSfd->Generate<sfd::MAKE|sfd::MK_ENABLEDISABLE      >("", "Secure and Non-Secure state", "Secure state only");
  m_genSfd->Generate<sfd::INFO|sfd::SINGLE                >("Value for FPU->FPCCR register bit CLRONRETS"   );
  CreateCCommentEnd();
  m_genH->Generate<c_header::MAKE|c_header::MK_DEFINE     >("%s",                                       value, 10, -1, descr.c_str(), name.c_str());

  return true;
}

bool PartitionData::CreateFloatingPointUnitFpccrClrOnRet(SvdCpu *cpu)
{
  if(!cpu) {
    return false;
  }

  uint32_t value = 0;
  string name = "FPU_FPCCR_CLRONRET_VAL";
  string descr = "";

  CreateCCommentBegin();
  m_genSfd->Generate<sfd::OPTION|sfd::SINGLE              >("Clear floating-point caller saved registers on exception return"     );
  m_genSfd->Generate<sfd::MAKE|sfd::MK_ENABLEDISABLE      >("", "Disabled", "Enabled");
  m_genSfd->Generate<sfd::INFO|sfd::SINGLE                >("Value for FPU->FPCCR register bit CLRONRET"   );
  CreateCCommentEnd();
  m_genH->Generate<c_header::MAKE|c_header::MK_DEFINE     >("%s",                                       value, 10, -1, descr.c_str(), name.c_str());

  return true;
}

bool PartitionData::CreateSetupInterruptTarget(SvdDevice *device)
{
  if(!device) {
    return false;
  }

  const auto& name = device->GetName();

  m_genH->Generate<c_header::DESCR|c_header::PART>("%s Specific Interrupt Numbers",            name.c_str());

  CreateHeadingBegin("Setup Interrupt Target");

  const auto& interrupts = device->GetInterruptList();
  map<int32_t, map<int32_t, SvdInterrupt*>> irqBlocks;

  for(const auto& [key, interrupt] : interrupts) {
    if(!interrupt) {
      continue;
    }

    // Sort Interrupts: 0..31, 32..63, ...
    uint32_t irqNum = interrupt->GetValue();
    uint32_t irqBlockNum = irqNum / 32;
    irqBlocks[irqBlockNum][irqNum] = interrupt;
  }

  for(const auto& [num, interrupt] : irqBlocks) {
    CreateInterruptBlock(interrupt, num);
    m_numOfItns++;
  }

  CreateHeadingEnd();

  return true;
}

bool PartitionData::CreateInterruptBlockBegin(int32_t num)
{
  uint32_t value = 1;
  string name  = "NVIC_INIT_ITNS";
  string descr = "";

  m_genH->Generate<c_header::DESCR|c_header::SUBPART      >("Interrupts %d..%d", num*32, ((num+1)*32)-1);

  CreateCCommentBegin();
  m_genSfd->Generate<sfd::HEADINGENABLE|sfd::BEGIN        >("Initialize ITNS %i (Interrupts %d..%d)", num, num*32, ((num+1)*32)-1);
  //m_genSfd->Generate<sfd::INFO|sfd::SINGLE              >("%s", descr.c_str());
  CreateCCommentEnd();
  m_genH->Generate<c_header::MAKE|c_header::MK_DEFINE     >("%s%i", value, 10, -1, descr.c_str(), name.c_str(), num);

  return true;
}

bool PartitionData::CreateInterruptBlock(const map<int32_t, SvdInterrupt*>& interrupts, int32_t num)
{
  if(interrupts.empty()) {
    return false;
  }

  CreateInterruptBlockBegin(num);
  CreateCCommentBegin();

  for(const auto& [key, interrupt] : interrupts) {
    if(!interrupt) {
      continue;
    }

    const auto name  = interrupt->GetNameCalculated();
    const auto descr = interrupt->GetDescriptionCalculated();

    CreateSetupInterruptTargetItem(interrupt);
  }

  uint32_t defaultVal = 0x000000;

  CreateCCommentEnd();
  m_genH->Generate<c_header::MAKE|c_header::MK_DEFINE>("NVIC_INIT_ITNS%i_VAL", defaultVal, 16, -1, "", num);

  CreateHeadingEnableEnd();

  return true;
}


bool PartitionData::CreateSetupInterruptTargetItem(SvdInterrupt *interrupt)
{
  if(!interrupt) {
    return false;
  }

  const auto name  = interrupt->GetNameCalculated();
  const auto descr = interrupt->GetDescriptionCalculated();
  uint32_t   num   = interrupt->GetValue();

  num &= 0x1f;
  m_genSfd->Generate<sfd::OBIT_NORANGE                    >("%s", num, name.c_str());
  m_genSfd->Generate<sfd::INFO|sfd::SINGLE                >("%s", descr.c_str());
  m_genSfd->Generate<sfd::MAKE|sfd::MK_ENABLEDISABLE      >("", "Secure state", "Non-Secure state");

  return true;
}

bool PartitionData::CreateSauRegions(SvdCpu *cpu)
{
  if(!cpu) {
    return false;
  }

  m_genH->Generate<c_header::DESCR|c_header::PART>("SAU Setup");

  CreateSauRegionMacro  (cpu);
  CreateSauSetup        (cpu);

  return true;
}

bool PartitionData::CreateSauRegionMacro(SvdCpu *cpu)
{
  if(!cpu) {
    return false;
  }

  CreateCCommentBegin();
  m_genH->Generate<c_header::DIRECT                      >("  max 128 SAU regions.");
  m_genH->Generate<c_header::DIRECT                      >("  SAU regions are defined in partition.h");
  CreateCCommentEnd();
  m_genH->Generate<c_header::DIRECT                      >("");
  m_genH->Generate<c_header::DIRECT                      >("%s", sauInitRegionMacro.c_str());
  m_genH->Generate<c_header::DIRECT                      >("");

  return true;
}

bool PartitionData::CreateSauSetup(SvdCpu *cpu)
{
  if(!cpu) {
    return false;
  }

  const auto cpuType = cpu->GetType();

  m_genH->Generate<c_header::DOXY_COMMENTSTAR|c_header::BEGIN>("");
  m_genH->Generate<c_header::DIRECT                      >("\\brief   Setup a SAU Region");
  m_genH->Generate<c_header::DIRECT                      >("\\details Writes the region information contained in SAU_Region to the");
  m_genH->Generate<c_header::DIRECT                      >("         registers SAU_RNR, SAU_RBAR, and SAU_RLAR");
  m_genH->Generate<c_header::DOXY_COMMENTSTAR|c_header::END  >("");

  m_genH->Generate<c_header::DIRECT                      >("__STATIC_INLINE void TZ_SAU_Setup (void)");
  m_genH->Generate<c_header::DIRECT                      >("{");
  m_genH->Generate<c_header::DIRECT                      >("");
  m_genH->Generate<c_header::DIRECT                      >("  #if defined (__SAUREGION_PRESENT) && (__SAUREGION_PRESENT == 1U)");
  m_genH->Generate<c_header::DIRECT                      >("");

  uint32_t numOfSauRegions = cpu->GetSauNumRegions();
  if(numOfSauRegions == SvdItem::VALUE32_NOT_INIT) {
    m_genH->Generate<c_header::C_ERROR>("SAU Setup: Number of SAU regions not set", cpu->GetLineNumber());
  }
  else {
    for(uint32_t i=0; i<numOfSauRegions; i++) {
      m_genH->Generate<c_header::DIRECT                    >("    #if defined (SAU_INIT_REGION%i) && (SAU_INIT_REGION%i == 1U)", i, i);
      m_genH->Generate<c_header::DIRECT                    >("      SAU_INIT_REGION(%i);", i);
      m_genH->Generate<c_header::DIRECT                    >("    #endif");
    }
  }

  m_genH->Generate<c_header::DIRECT                      >("");
  m_genH->Generate<c_header::DIRECT                      >("  #endif /* defined (__SAUREGION_PRESENT) && (__SAUREGION_PRESENT == 1U) */");

  m_genH->Generate<c_header::DIRECT                      >("");
  m_genH->Generate<c_header::DIRECT                      >("  #if defined (SAU_INIT_CTRL) && (SAU_INIT_CTRL == 1U)");
  m_genH->Generate<c_header::DIRECT                      >("    SAU->CTRL = ((SAU_INIT_CTRL_ENABLE << SAU_CTRL_ENABLE_Pos) & SAU_CTRL_ENABLE_Msk) |");
  m_genH->Generate<c_header::DIRECT                      >("                ((SAU_INIT_CTRL_ALLNS  << SAU_CTRL_ALLNS_Pos ) & SAU_CTRL_ALLNS_Msk)   ;");
  m_genH->Generate<c_header::DIRECT                      >("  #endif");
  m_genH->Generate<c_header::DIRECT                      >("  ");

  m_genH->Generate<c_header::DIRECT                      >("  #if defined (SCB_CSR_AIRCR_INIT) && (SCB_CSR_AIRCR_INIT == 1U)");
  m_genH->Generate<c_header::DIRECT                      >("    SCB->SCR   = (SCB->SCR   & ~(SCB_SCR_SLEEPDEEPS_Msk    )) |");
  m_genH->Generate<c_header::DIRECT                      >("                ((SCB_CSR_DEEPSLEEPS_VAL     << SCB_SCR_SLEEPDEEPS_Pos)     & SCB_SCR_SLEEPDEEPS_Msk);");
  m_genH->Generate<c_header::DIRECT                      >("");
  m_genH->Generate<c_header::DIRECT                      >("    SCB->AIRCR = (SCB->AIRCR & ~(SCB_AIRCR_VECTKEY_Msk   | SCB_AIRCR_SYSRESETREQS_Msk |");
  m_genH->Generate<c_header::DIRECT                      >("                                 SCB_AIRCR_BFHFNMINS_Msk | SCB_AIRCR_PRIS_Msk)        )                    |");
  m_genH->Generate<c_header::DIRECT                      >("                 ((0x05FAU                    << SCB_AIRCR_VECTKEY_Pos)      & SCB_AIRCR_VECTKEY_Msk)      |");
  m_genH->Generate<c_header::DIRECT                      >("                 ((SCB_AIRCR_SYSRESETREQS_VAL << SCB_AIRCR_SYSRESETREQS_Pos) & SCB_AIRCR_SYSRESETREQS_Msk) |");
  m_genH->Generate<c_header::DIRECT                      >("                 ((SCB_AIRCR_PRIS_VAL         << SCB_AIRCR_PRIS_Pos)         & SCB_AIRCR_PRIS_Msk)         |");
  m_genH->Generate<c_header::DIRECT                      >("                 ((SCB_AIRCR_BFHFNMINS_VAL    << SCB_AIRCR_BFHFNMINS_Pos)    & SCB_AIRCR_BFHFNMINS_Msk);");
  m_genH->Generate<c_header::DIRECT                      >("  #endif /* defined (SCB_CSR_AIRCR_INIT) && (SCB_CSR_AIRCR_INIT == 1U) */");
  m_genH->Generate<c_header::DIRECT                      >("  ");

  if(cpuType == SvdTypes::CpuType::CM23 || cpuType == SvdTypes::CpuType::V8MBL) {
    m_genH->Generate<c_header::DIRECT                    >("  #if defined (SCB_ICSR_INIT) && (SCB_ICSR_INIT == 1U)");
    m_genH->Generate<c_header::DIRECT                    >("    SCB->ICSR  = (SCB->ICSR  & ~(SCB_ICSR_STTNS_Msk        )) |");
    m_genH->Generate<c_header::DIRECT                    >("                   ((SCB_ICSR_STTNS_VAL         << SCB_ICSR_STTNS_Pos)         & SCB_ICSR_STTNS_Msk);");
    m_genH->Generate<c_header::DIRECT                    >("  #endif /* defined (SCB_ICSR_INIT) && (SCB_ICSR_INIT == 1U) */");
    m_genH->Generate<c_header::DIRECT                    >("");
  }

  if(cpu->GetFpuPresent()) {
    m_genH->Generate<c_header::DIRECT                    >("  #if defined (__FPU_USED) && (__FPU_USED == 1U) && defined (TZ_FPU_NS_USAGE) && (TZ_FPU_NS_USAGE == 1U)");
    m_genH->Generate<c_header::DIRECT                    >("    SCB->NSACR = (SCB->NSACR & ~(SCB_NSACR_CP10_Msk | SCB_NSACR_CP10_Msk)) |");
    m_genH->Generate<c_header::DIRECT                    >("                 ((SCB_NSACR_CP10_11_VAL << SCB_NSACR_CP10_Pos) & (SCB_NSACR_CP10_Msk | SCB_NSACR_CP11_Msk));");
    m_genH->Generate<c_header::DIRECT                    >("");
    m_genH->Generate<c_header::DIRECT                    >("    FPU->FPCCR = (FPU->FPCCR & ~(FPU_FPCCR_TS_Msk | FPU_FPCCR_CLRONRETS_Msk | FPU_FPCCR_CLRONRET_Msk)) |");
    m_genH->Generate<c_header::DIRECT                    >("                 ((FPU_FPCCR_TS_VAL        << FPU_FPCCR_TS_Pos       ) & FPU_FPCCR_TS_Msk       ) |");
    m_genH->Generate<c_header::DIRECT                    >("                 ((FPU_FPCCR_CLRONRETS_VAL << FPU_FPCCR_CLRONRETS_Pos) & FPU_FPCCR_CLRONRETS_Msk) |");
    m_genH->Generate<c_header::DIRECT                    >("                 ((FPU_FPCCR_CLRONRET_VAL  << FPU_FPCCR_CLRONRET_Pos ) & FPU_FPCCR_CLRONRET_Msk );");
    m_genH->Generate<c_header::DIRECT                    >("  #endif /* defined (__FPU_USED) && (__FPU_USED == 1U) && defined (TZ_FPU_NS_USAGE) && (TZ_FPU_NS_USAGE == 1U) */");
    m_genH->Generate<c_header::DIRECT                    >("");
  }

  for(uint32_t i=0; i<m_numOfItns; i++) {
    m_genH->Generate<c_header::DIRECT                    >("  #if defined (NVIC_INIT_ITNS%i) && (NVIC_INIT_ITNS%i == 1U)", i, i);
    m_genH->Generate<c_header::DIRECT                    >("    NVIC->ITNS[%i] = NVIC_INIT_ITNS%i_VAL;", i, i);
    m_genH->Generate<c_header::DIRECT                    >("  #endif");
    m_genH->Generate<c_header::DIRECT                    >("");
  }

  m_genH->Generate<c_header::DIRECT                      >("}");

  return true;
}
