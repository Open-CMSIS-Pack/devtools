/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "SvdTypes.h"
#include "SvdUtils.h"

using namespace std;


const string SvdTypes::expression_str[] = {
  "Expression::NONE",
  "Expression::EXTEND",
  "Expression::ARRAY",
  "Expression::INVALID"
};

const string SvdTypes::access_str[] = {
  "Access: undefined",
  "read-only",
  "write-only",
  "read-write",
  "writeOnce",
  "read-writeOnce",
};

const string SvdTypes::accessSfd_str[] = {
  "UNDEF",
  "RO",
  "WO",
  "RW",
  "RW",
  "RW",
};

const string SvdTypes::access_ioTypes_str[] = {
  "     ",    // "undefined",
  "__IM ",    // "RO: read-only",
  "__OM ",    // "WO: write-only",
  "__IOM",    // "RW: read-write",
  "__OM ",    // "W: write once",
  "__IOM",    // "RW: read-write once",
};

const string SvdTypes::addrBlockUsage_str[] = {
  "undefined",
  "registers",
  "buffer",
  "reserved"
};

const string SvdTypes::enumUsage_str[] = {
  "undefined",
  "read",
  "write",
  "read-write"
};

const string SvdTypes::endian_str[] = {
  "<endian not set>",
  "little",
  "big",
  "selectable",
  "other"
};

#define MODIFWRV(enumType, name) { name, },
const string SvdTypes::modifiedWriteValues_str[] = {
  #include "EnumStringTables.h"
};
#undef MODIFWRV

const string SvdTypes::readAction_str[] = {
  "undefined",
  "clear",
  "set",
  "modify",
  "modifyExternal",
};

// Cortex M "internal"
const map<SvdTypes::CpuIrqNum, CpuIrq> SvdTypes::cpuIrqName = {
  { SvdTypes::CpuIrqNum::IRQ0         , { "Reserved0"         , "Stack Top is loaded from first entry of vector Table on Reset"                  } },
  { SvdTypes::CpuIrqNum::IRQ1         , { "Reset"             , "Reset Vector, invoked on Power up and warm reset"                               } },
  { SvdTypes::CpuIrqNum::IRQ2         , { "NonMaskableInt"    , "Non maskable Interrupt, cannot be stopped or preempted"                         } },
  { SvdTypes::CpuIrqNum::IRQ3         , { "HardFault"         , "Hard Fault, all classes of Fault"                                               } },
  { SvdTypes::CpuIrqNum::IRQ4         , { "MemoryManagement"  , "Memory Management, MPU mismatch, including Access Violation and No Match"       } },
  { SvdTypes::CpuIrqNum::IRQ5         , { "BusFault"          , "Bus Fault, Pre-Fetch-, Memory Access Fault, other address/memory related Fault" } },
  { SvdTypes::CpuIrqNum::IRQ6         , { "UsageFault"        , "Usage Fault, i.e. Undef Instruction, Illegal State Transition"                  } },
  { SvdTypes::CpuIrqNum::IRQ7         , { "SecureFault"       , "Secure Fault Handler"                                                           } },
  { SvdTypes::CpuIrqNum::IRQ8         , { "Reserved8"         , "Reserved - do not use"                                                          } },
  { SvdTypes::CpuIrqNum::IRQ9         , { "Reserved9"         , "Reserved - do not use"                                                          } },
  { SvdTypes::CpuIrqNum::IRQ10        , { "Reserved10"        , "Reserved - do not use"                                                          } },
  { SvdTypes::CpuIrqNum::IRQ11        , { "SVCall"            , "System Service Call via SVC instruction"                                        } },
  { SvdTypes::CpuIrqNum::IRQ12        , { "DebugMonitor"      , "Debug Monitor"                                                                  } },
  { SvdTypes::CpuIrqNum::IRQ13        , { "Reserved11"        , "Reserved - do not use"                                                          } },
  { SvdTypes::CpuIrqNum::IRQ14        , { "PendSV"            , "Pendable request for system service"                                            } },
  { SvdTypes::CpuIrqNum::IRQ15        , { "SysTick"           , "System Tick Timer"                                                              } },
  { SvdTypes::CpuIrqNum::IRQ_RESERVED , { "Reserved"          , "Reserved - do not use"                                                          } },
};

const map <SvdTypes::CpuType, CpuTypeFeature> SvdTypes::cpuTypeName = {
//                                                                                                                                              *I *D
//                                                                                                                                           *F *C *C                   *M   *I
//                                                                                                                                  *V       *P *A *A *I *D             *V   *R
//                                                                                                                                  *T *M *F *U *C *A *T *T *S *D *P *M *E   *Q
//                                                                                                                                  *O *P *P *D *H *C *C *C *A *S *M *V *F   *
//                                                                           IRQ 0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15     *R *U *U *P *E *E *M *M *U *P *U *E *P   *N
  { SvdTypes::CpuType::UNDEF    , {"undef"              , "undefined"         ,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   0 } },
  // SVDConv supported
  { SvdTypes::CpuType::CM0      , {"CM0"                , "ARM Cortex-M0"     ,  0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1,     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  32 } },
  { SvdTypes::CpuType::CM0PLUS  , {"CM0PLUS"            , "ARM Cortex-M0+"    ,  0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1,     1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  32 } },
  { SvdTypes::CpuType::CM0P     , {"CM0PLUS"            , "ARM Cortex-M0+"    ,  0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1,     1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  32 } },
  { SvdTypes::CpuType::CM1      , {"CM1"                , "ARM Cortex-M1"     ,  0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1,     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  32 } },
  { SvdTypes::CpuType::SC000    , {"SC000"              , "Secure Core SC000" ,  0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1,     1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  32 } },
  { SvdTypes::CpuType::CM3      , {"CM3"                , "ARM Cortex-M3"     ,  0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 1, 1,     0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 240 } },
  { SvdTypes::CpuType::SC300    , {"SC300"              , "Secure Core SC300" ,  0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 1, 1,     0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 240 } },
  { SvdTypes::CpuType::CM4      , {"CM4"                , "ARM Cortex-M4"     ,  0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 1, 1,     0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 240 } },
  { SvdTypes::CpuType::CM7      , {"CM7"                , "ARM Cortex-M7"     ,  0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 1, 1,     0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 240 } },
  { SvdTypes::CpuType::CM33     , {"CM33"               , "ARM Cortex-M33"    ,  0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 1, 1,     1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 480 } },
  { SvdTypes::CpuType::CM23     , {"CM23"               , "ARM Cortex-M23"    ,  0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1,     1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 240 } },
  { SvdTypes::CpuType::CM35     , {"CM35"               , "ARM Cortex-M35"    ,  0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 1, 1,     1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 480 } },
  { SvdTypes::CpuType::CM35P    , {"CM35P"              , "ARM Cortex-M35P"   ,  0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 1, 1,     1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 480 } },
  { SvdTypes::CpuType::V8MML    , {"ARMV8MML"           , "ARM ARMV8MML"      ,  0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 1, 1,     1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 480 } },
  { SvdTypes::CpuType::V8MBL    , {"ARMV8MBL"           , "ARM ARMV8MBL"      ,  0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1,     1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 240 } },
  { SvdTypes::CpuType::V81MML   , {"ARMV81MML"          , "ARM ARMV81MML"     ,  0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 1, 1,     1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 480 } },
  { SvdTypes::CpuType::CM55     , {"CM55"               , "ARM Cortex-M55"    ,  0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 1, 1,     1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 480 } }, // MVE: 0, not generated atm
  { SvdTypes::CpuType::CM85     , {"CM85"               , "ARM Cortex-M85"    ,  0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 1, 1,     1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 480 } }, // MVE: 0, not generated atm

  // Arm China
  { SvdTypes::CpuType::SMC1     , {"SMC1"               , "ARM China Star-MC1",  0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 1, 1,     1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 480 } },  // ~ M33
  { SvdTypes::CpuType::CM52     , {"CM52"               , "ARM Cortex-M52"    ,  0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 1, 1,     1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 480 } }, // MVE: 0, not generated atm

  // SVDConv not supported
  { SvdTypes::CpuType::CA5      , {"CA5"                , "ARM Cortex-A5"     ,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 240 } },
  { SvdTypes::CpuType::CA7      , {"CA7"                , "ARM Cortex-A7"     ,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 240 } },
  { SvdTypes::CpuType::CA8      , {"CA8"                , "ARM Cortex-A8"     ,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 240 } },
  { SvdTypes::CpuType::CA9      , {"CA9"                , "ARM Cortex-A9"     ,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 240 } },
  { SvdTypes::CpuType::CA15     , {"CA15"               , "ARM Cortex-A15"    ,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 240 } },
  { SvdTypes::CpuType::CA17     , {"CA17"               , "ARM Cortex-A17"    ,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 240 } },
  { SvdTypes::CpuType::CA53     , {"CA53"               , "ARM Cortex-A53"    ,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 240 } },
  { SvdTypes::CpuType::CA57     , {"CA57"               , "ARM Cortex-A57"    ,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 240 } },
  { SvdTypes::CpuType::CA72     , {"CA72"               , "ARM Cortex-A72"    ,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 240 } },
  { SvdTypes::CpuType::OTHER    , {"other"              , "other"             ,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 240 } },
};


const string& SvdTypes::GetExpressionType(Expression expr)
{
  return expression_str[(uint32_t)expr];
}

const string& SvdTypes::GetAccessType(Access acc)
{
  if(acc >= Access::END) {
    return SvdUtils::EMPTY_STRING;
  }

  return access_str[(uint32_t)acc];
}

const string& SvdTypes::GetAccessTypeSfd(Access acc)
{
  return accessSfd_str[(uint32_t)acc];
}

const string& SvdTypes::GetAccessTypeIo(Access acc)
{
  return access_ioTypes_str[(uint32_t)acc];
}

const string& SvdTypes::GetUsage(AddrBlockUsage usage)
{
  return addrBlockUsage_str[(uint32_t)usage];
}

const string& SvdTypes::GetCpuType(CpuType  cpuType)
{
  auto it = cpuTypeName.find(cpuType);
  if(it != cpuTypeName.end()) {
    return it->second.type;
  }

  return SvdUtils::EMPTY_STRING;
}

const string& SvdTypes::GetCpuName(CpuType  cpuType)
{
  auto it = cpuTypeName.find(cpuType);
  if(it != cpuTypeName.end()) {
    return it->second.name;
  }

  return SvdUtils::EMPTY_STRING;
}

const string& SvdTypes::GetCpuEndian(Endian endian)
{
  return endian_str[(uint32_t)endian];
}

const string& SvdTypes::GetModifiedWriteValue(ModifiedWriteValue val)
{
  return modifiedWriteValues_str[(uint32_t)val];
}

const string& SvdTypes::GetReadAction(ReadAction act)
{
  return readAction_str[(uint32_t)act];
}

const string& SvdTypes::GetCortexMInterruptName(SvdTypes::CpuIrqNum num)
{
  auto it = cpuIrqName.find(num);
  if(it != cpuIrqName.end()) {
    return it->second.name;
  }

  return SvdUtils::EMPTY_STRING;
}

const string& SvdTypes::GetCortexMInterruptDescription(SvdTypes::CpuIrqNum num)
{
  auto it = cpuIrqName.find(num);
  if(it != cpuIrqName.end()) {
    return it->second.descr;
  }

  return SvdUtils::EMPTY_STRING;
}

bool SvdTypes::GetCortexMInterruptAvailable(CpuType cpuType, SvdTypes::CpuIrqNum num)
{
  auto it = cpuTypeName.find(cpuType);
  if(it != cpuTypeName.end()) {
    return it->second.irq[(uint32_t)num];
  }

  return false;
}

bool SvdTypes::GetCortexMInterrupt(CpuType cpuType, SvdTypes::CpuIrqNum num, string &name, string &descr)
{
  if(!GetCortexMInterruptAvailable(cpuType, num)) {
    name  = GetCortexMInterruptName(SvdTypes::CpuIrqNum::IRQ_RESERVED);
    name += SvdUtils::CreateDecNum((uint32_t)num);
    descr = GetCortexMInterruptDescription(SvdTypes::CpuIrqNum::IRQ_RESERVED);
  }
  else {
    name  = GetCortexMInterruptName(num);
    descr = GetCortexMInterruptDescription(num);
  }

  return true;
}

const string& SvdTypes::GetEnumUsage(EnumUsage enumUsage)
{
  return enumUsage_str[(uint32_t)enumUsage];
}

const CpuFeature& SvdTypes::GetCpuFeatures(CpuType cpuType)
{
  auto it = cpuTypeName.find(cpuType);
  if(it != cpuTypeName.end()) {
    return it->second.cpuFeature;
  }

  return cpuTypeName.find(CpuType::OTHER)->second.cpuFeature;
}
