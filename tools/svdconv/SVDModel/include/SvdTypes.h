/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SvdTypes_H
#define SvdTypes_H

#include <cstdint>
#include <string>
#include <vector>
#include <set>
#include <list>
#include <map>




struct CpuFeature {
  const bool VTOR;
  const bool MPU;
  const bool FPU;
  const bool FPUDP;
  const bool ICACHE;
  const bool DCACHE;
  const bool ITCM;
  const bool DTCM;
  const bool SAU;
  const bool DSP;
  const bool PMU;
  const bool MVE;
  const bool MVEFP;
  const uint16_t NUMEXTIRQ;
public:
  CpuFeature(const bool vtor, const bool mpu, const bool fpu, const bool fpudp,
               const bool icache, const bool dcache, const bool itcm, const bool dtcm,
               const bool sau, const bool dsp, const bool pmu, const bool mve, const bool mvefp,
               const uint16_t numExtIrq)
  : VTOR(vtor), MPU(mpu), FPU(fpu), FPUDP(fpudp), ICACHE(icache), DCACHE(dcache),
    ITCM(itcm), DTCM(dtcm), SAU(sau), DSP(dsp), PMU(pmu), MVE(mve), MVEFP(mvefp),
    NUMEXTIRQ(numExtIrq)
    {}
};

struct CpuIrq {
  const std::string name;
  const std::string descr;
};

struct CpuTypeFeature {
  const std::string type;
  const std::string name;
  bool              irq[16];
  const CpuFeature  cpuFeature;

public:
  CpuTypeFeature(const char *t, const char *n,
        const bool irqVals0,  const bool irqVals1,  const bool irqVals2,  const bool irqVals3,
        const bool irqVals4,  const bool irqVals5,  const bool irqVals6,  const bool irqVals7,
        const bool irqVals8,  const bool irqVals9,  const bool irqVals10, const bool irqVals11,
        const bool irqVals12, const bool irqVals13, const bool irqVals14, const bool irqVals15,
        const bool cpuF0,     const bool cpuF1,     const bool cpuF2,     const bool cpuF3,
        const bool cpuF4,     const bool cpuF5,     const bool cpuF6,     const bool cpuF7,
        const bool cpuF8,     const bool cpuF9,     const bool cpuF10,    const bool cpuF11,
        const bool cpuF12,
        const uint16_t numExtIrq
  )
    : type(t), name(n),
      cpuFeature(cpuF0,cpuF1,cpuF2,cpuF3,cpuF4,cpuF5,cpuF6,cpuF7,cpuF8,cpuF9,cpuF10,cpuF11,cpuF12, numExtIrq)
  {
    uint32_t i = 0;
    irq[i++] = irqVals0;
    irq[i++] = irqVals1;
    irq[i++] = irqVals2;
    irq[i++] = irqVals3;
    irq[i++] = irqVals4;
    irq[i++] = irqVals5;
    irq[i++] = irqVals6;
    irq[i++] = irqVals7;
    irq[i++] = irqVals8;
    irq[i++] = irqVals9;
    irq[i++] = irqVals10;
    irq[i++] = irqVals11;
    irq[i++] = irqVals12;
    irq[i++] = irqVals13;
    irq[i++] = irqVals14;
    irq[i++] = irqVals15;
  }
};



class SvdTypes {
public:
  virtual ~SvdTypes() {}

  enum class ProtectionType   { UNDEF = 0, NONSECURE/*n*/, SECURE/*s*/, PRIVILEGED/*p*/             };
  enum class SauAccessType    { UNDEF = 0, NONSECURE/*n*/, SECURE/*c*/                              };
  enum class Expression       { UNDEF = 0, NONE, EXTEND, ARRAY, INVALID, ARRAYINVALID               };
  enum class Access           { UNDEF = 0, READONLY, WRITEONLY, READWRITE, WRITEONCE, READWRITEONCE, END };
  enum class AddrBlockUsage   { UNDEF = 0, REGISTERS, BUFFER, RESERVED                              };
  enum class Endian           { UNDEF = 0, LITTLE, BIG, SELECTABLE, OTHER                           };
  enum class ReadAction       { UNDEF = 0, CLEAR, SET, MODIFY, MODIFEXT                             };
  enum class EnumUsage        { UNDEF = 0, READ, WRITE, READWRITE                                   };
  enum class SvdConvV2accType { EMPTY = 0, READ, READONLY, WRITE, WRITEONLY, READWRITE, UNDEF       };
  enum class CpuIrqNum        { IRQ0  = 0, IRQ1, IRQ2, IRQ3, IRQ4, IRQ5, IRQ6, IRQ7, IRQ8, IRQ9, IRQ10, IRQ11, IRQ12, IRQ13, IRQ14, IRQ15, IRQ_END, IRQ_RESERVED, IRQ_UNDEF };

  #define CPUTYPE(enumType, name) enumType,
  enum class CpuType {
    #include "EnumStringTables.h"
  };
  #undef CPUTYPE

  #define MODIFWRV(enumType, name) enumType,
  enum class ModifiedWriteValue {
    #include "EnumStringTables.h"
  };
  #undef MODIFWRV

  struct CmsisCfgForce {
    uint32_t  bMpuPresent     : 1;
    uint32_t  bFpuPresent     : 1;
    uint32_t  bVtorPresent    : 1;
    uint32_t  bDspPresent     : 1;
    uint32_t  bFpuDP          : 1;
    uint32_t  bIcachePresent  : 1;
    uint32_t  bDcachePresent  : 1;
    uint32_t  bItcmPresent    : 1;
    uint32_t  bDtcmPresent    : 1;
    uint32_t  bSauPresent     : 1;
    uint32_t  bPmuPresent     : 1;
    uint32_t  bMvePresent     : 1;
    uint32_t  bMveFP          : 1;
  };

  static const std::string&   GetExpressionType                (Expression exprType);
  static const std::string&   GetAccessType                    (Access accType);
  static const std::string&   GetAccessTypeSfd                 (Access accType);
  static const std::string&   GetAccessTypeIo                  (Access accType);
  static const std::string&   GetUsage                         (AddrBlockUsage  usageType);
  static const std::string&   GetCpuType                       (CpuType  cpuType);
  static const std::string&   GetCpuName                       (CpuType  cpuType);
  static const std::string&   GetCpuEndian                     (Endian endian);
  static const std::string&   GetModifiedWriteValue            (ModifiedWriteValue val);
  static const std::string&   GetReadAction                    (ReadAction act);
  static const std::string&   GetCortexMInterruptName          (SvdTypes::CpuIrqNum num);
  static const std::string&   GetCortexMInterruptDescription   (SvdTypes::CpuIrqNum num);
  static const std::string&   GetEnumUsage                     (EnumUsage enumUsage);
  static       bool           GetCortexMInterruptAvailable     (CpuType cpuType, SvdTypes::CpuIrqNum num);
  static       bool           GetCortexMInterrupt              (CpuType cpuType, SvdTypes::CpuIrqNum num, std::string &name, std::string &descr);
  static const CpuFeature&    GetCpuFeatures                   (CpuType cpuType);

private:
  static const std::map<SvdTypes::CpuType, CpuTypeFeature> cpuTypeName;
  static const std::map<SvdTypes::CpuIrqNum, CpuIrq> cpuIrqName;
  static const std::string readAction_str[];
  static const std::string modifiedWriteValues_str[];
  static const std::string endian_str[];
  static const std::string enumUsage_str[];
  static const std::string addrBlockUsage_str[];
  static const std::string access_ioTypes_str[];
  static const std::string accessSfd_str[];
  static const std::string access_str[];
  static const std::string expression_str[];
};

#endif // SvdTypes_H
