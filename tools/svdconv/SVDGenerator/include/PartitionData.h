/*
 * Copyright (c) 2010-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef PartitionData_H
#define PartitionData_H

#include "FileIo.h"
#include "SvdOptions.h"
#include "SvdGenerator.h"


#include <string>
#include <map>
#include <list>


class SvdDevice;
class SvdCpu;
class SvdInterrupt;
class HeaderGenerator;
class ConfWizGenerator;
class SfdGenerator;
class SvdSauRegion;

class PartitionData {
public:
  PartitionData(const FileHeaderInfo &fileHeaderInfo, const SvdOptions& options);
  ~PartitionData();

  bool      Create                                  (SvdItem *item, const std::string &fileName);

protected:
  bool      CreatePartitionStart                    (SvdDevice *device);
  bool      CreatePartitionEnd                      (SvdDevice *device);

  // PartitionData
  bool      CreateMaxNumSauRegions                  (SvdCpu *cpu);
  bool      CreateSauRegionsConfig                  (SvdCpu *cpu);
  bool      CreateSauGlobalConfig                   (SvdCpu *cpu);
  bool      CreateSauInitControl                    (SvdCpu *cpu);
  bool      CreateSauInitControlEnable              (SvdCpu *cpu);
  bool      CreateSauAllNonSecure                   (SvdCpu *cpu);
  bool      CreateInitSauRegions                    (SvdCpu *cpu);
  bool      CreateInitSauRegionNumber               (SvdSauRegion* region, uint32_t regionNumber);
  bool      CreateSleepAndExceptionHandling         (SvdCpu *cpu);

  bool      CreateSleepAndExceptionBegin            (SvdCpu *cpu);
  bool      CreateFault                             (SvdCpu *cpu);
  bool      CreatePriorityExceptions                (SvdCpu *cpu);
  bool      CreateSystemReset                       (SvdCpu *cpu);
  bool      CreateDeepSleep                         (SvdCpu *cpu);

  bool      CreateSingleSysTick                     (SvdCpu *cpu);
  bool      CreateSingleSysTickBegin                (SvdCpu *cpu);
  bool      CreateSingleSysTickIcsr                 (SvdCpu *cpu);

  bool      CreateFloatingPointUnit                 (SvdCpu *cpu);
  bool      CreateFloatingPointUnitBegin            (SvdCpu *cpu);
  bool      CreateFloatingPointUnitNsacrCp10Cp11    (SvdCpu *cpu);
  bool      CreateFloatingPointUnitFpccrTs          (SvdCpu *cpu);
  bool      CreateFloatingPointUnitFpccrClrOnRetS   (SvdCpu *cpu);
  bool      CreateFloatingPointUnitFpccrClrOnRet    (SvdCpu *cpu);
  bool      CreateSetupInterruptTarget              (SvdDevice *device);
  bool      CreateSetupInterruptTargetItem          (SvdInterrupt *interrupt);
  bool      CreateInterruptBlock                    (const std::map<int32_t, SvdInterrupt*>& interrupts, int32_t num);
  bool      CreateInterruptBlockBegin               (int32_t num);
  bool      CreateSauRegions                        (SvdCpu *cpu);
  bool      CreateSauRegionMacro                    (SvdCpu *cpu);
  bool      CreateSauSetup                          (SvdCpu *cpu);
  bool      CreateHeadingBegin                      (const std::string& text);
  bool      CreateHeadingEnd                        ();
  bool      CreateHeadingEnableBegin                (const std::string& text);
  bool      CreateHeadingEnableEnd                  ();
  bool      CreateConfWizStart                      ();
  bool      CreateConfWizEnd                        ();
  void      CreateCCommentBegin();
  void      CreateCCommentEnd();

  const SvdOptions& GetOptions() { return m_options; }

private:
  const SvdOptions&   m_options;
  FileIo*             m_fileIo;
  HeaderGenerator*    m_genH;
  SfdGenerator*       m_genSfd;

  uint32_t            m_numOfItns;
  
  static const std::string CFG_BEGIN;
  static const std::string CFG_END;
  static const std::string sauInitRegionMacro;
};

#endif // PartitionData_H
