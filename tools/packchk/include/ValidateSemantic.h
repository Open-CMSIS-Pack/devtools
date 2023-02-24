/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef VALIDATESEMANTIC_H
#define VALIDATESEMANTIC_H

#include "Validate.h"
#include "PackChk.h"
#include "GatherCompilers.h"

#include <list>
#include <set>

#define REGEX_NOTFOUND   0
#define REGEX_FOUND      1
#define REGEX_WRONGEXP   2

class ValidateSemantic : public Validate
{
public:
  ValidateSemantic(RteGlobalModel& rteModel, CPackOptions& packOptions);
  ~ValidateSemantic();

  bool BeginTest(const std::string& packName);
  bool EndTest();

  bool Check();
  bool TestMcuDependencies(RtePackage* pKg);
  bool TestDepsResult(const std::map<const RteItem*, RteDependencyResult>& results, RteComponent* component);
  bool CheckSelfResolvedCondition(RteComponent* component, RteTarget* target);
  bool TestComponentDependencies();
  bool TestApis();
  bool CheckForUnsupportedChars(const std::string& name, const std::string& tag, int lineNo);
  bool CheckMemory(RteDeviceItem* device);
  bool GatherCompilers(RtePackage* pKg);
  bool CheckDeviceDescription(RteDeviceItem* device, RteDeviceProperty* processorProperty);
  bool FindName(const std::string& name, const std::string& searchName, const std::string& searchExt);
  bool UpdateRte(RteTarget* target, RteProject* rteProject, RteComponent* component);
  bool CheckDependencyResult(RteTarget* target, RteComponent* component, std::string mcuVendor, std::string mcuDispName, compiler_s compiler);
  bool ExcludeSysHeaderDirectories(const std::string& systemHeader, const std::string& rteFolder);
  bool FindFileFromList(const std::string& systemHeader, const std::set<RteFile*>& targFiles);
  bool CheckDeviceDependencies(RteDeviceItem* device, RteProject* rteProject);
  bool HasExternalGenerator(RteComponentAggregate* aggregate);

  const std::map<std::string, compiler_s>& GetCompilers() {
    return m_compilers;
  }

private:
  bool OutputDepResults(const RteDependencyResult& dependencyResult, bool inRecursion = 0);

  std::map<std::string, compiler_s> m_compilers;
};

#endif // VALIDATESEMANTIC_H
