/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef VALIDATESYNTAX_H
#define VALIDATESYNTAX_H

#include "Validate.h"
#include "CheckFiles.h"
#include "PackChk.h"

#define PKG_FEXT               ".pack"
#define COMMON_PROCESSORS_STR  "__COMMON__PROCESSORS__PROPMAP__"

typedef enum SchemaCheckResult {
  SCHEMA_SUPPORTED = 0,   // Schema version is supported
  SCHEMA_OLDER = 1,   // Schema version is older than min supported
  SCHEMA_NEWER = 2,   // Schema version is newer than max supported
  SCHEMA_MISSING = 3,   // Schema version is missing
} SCHEMA_CHECK_RESULT;

struct FeatureEntry {
  FeatureEntry() {};
  FeatureEntry(std::string n, std::string e, std::string a) : defaultName(n), example(e), shownAs(a) {};

  std::string defaultName;
  std::string example;
  std::string shownAs;
};

typedef std::map<std::string, FeatureEntry> FEATURE_TABLE;

class ValidateSyntax : public Validate
{
public:
  ValidateSyntax(RteGlobalModel& rteModel, CPackOptions& packOptions);
  ~ValidateSyntax();
  bool Check();

protected:
  static bool IsURL(const std::string& filename);

private:
  std::string CreateId(RteItem* prop, const std::string& cpuName);
  std::string CreateIdFeature(RteItem* prop);

  bool BeginTest(const std::string& packName);
  bool EndTest();

  bool CheckSchemaVersion(RtePackage* pKg);
  bool ClearSchemaVersion();
  bool CheckAllFiles(RtePackage* pKg);
  bool CheckInfo(RtePackage* pKg);
  bool CheckPackageUrl(RtePackage* pKg);
  bool CheckPackageReleaseDate(RtePackage* pKg);
  bool CheckDevices(RtePackage* pKg);
  bool CheckComponents(RtePackage* pKg);
  bool CheckExamples(RtePackage* pKg);
  bool CheckBoards(RtePackage* pKg);
  bool CheckTaxonomy(RtePackage* pKg);
  bool CheckRequirements(RtePackage* pKg);
  bool CheckRequirements_Packages(RteItem* requirement);
  bool CheckLicense(RtePackage* pKg, CheckFilesVisitor& fileVisitor);
  bool CheckFeatureDevice(RteDeviceProperty* prop, const std::string& devName);
  bool CheckFeatureBoard(RteItem* prop, const std::string& boardName);
  bool CheckDeviceProperties(RtePackage* pKg);
  bool CheckBoardProperties(RtePackage* pKg);
  bool CheckDeviceProperties(RteDeviceItem* item, std::map<std::string, std::map<std::string, RteDeviceProperty*>>& prevPropertiesMaps);
  bool CheckBoardProperties(RteItem* boardItem, std::map<std::string, RteItem*>& prevProperties);
  bool CheckAddProperty(RteDeviceProperty* prop, std::map<std::string, std::map<std::string, RteDeviceProperty*>>& propertiesMaps, const std::string& devN);
  bool CheckAddPropertyAll(RteDeviceProperty* prop, std::map<std::string, std::map<std::string, RteDeviceProperty*>>& propertiesMaps, const std::string& devN);
  bool CheckAddBoardProperty(RteItem* prop, std::map<std::string, RteItem*>& prevProperties, const std::string& boardName);
  bool CheckForBoard(RteExample* example);
  bool CheckAddBoard(RteBoard* board);
  bool BoardFindExamples(RteBoard* board);
  void AddToId(std::string& id, const std::string type, const std::string text);
  bool CheckDevicesMultiple(RteDeviceItem* deviceItem, const std::map<std::string, RteDeviceItem*>& newDevicesList);
  bool CheckAddDevice(RteDeviceItem* deviceItem, std::map<std::string, RteDeviceItem*>& devicesList, const std::string& devName);
  bool CheckHierarchy(RtePackage* pKg);
  bool CheckHierarchy(RteItem* parentItem, std::map<std::string, RteDeviceItem*>& treeItems);
  void InitFeatures();
  bool CheckAllItems();
  bool CheckAllDevices();
  const std::string& GetRteTypeString(const RteDeviceItem::TYPE type);

  std::map<std::string, std::list<RteDeviceItem*> > m_allItems;
  std::map<std::string, std::list<RteDeviceItem*> > m_allDevices;
  std::string m_pdscFullpath;
  std::map<std::string, RteBoard*> boardsFound;

  FEATURE_TABLE m_featureTableDevice;
  FEATURE_TABLE m_featureTableBoard;
  std::map<std::string, FEATURE_TABLE::iterator> m_featureTableDeviceLowerCase;
  std::map<std::string, FEATURE_TABLE::iterator> m_featureTableBoardLowerCase;
  std::string m_schemaVersion;

  std::map<std::string, RteDeviceItem*> m_allDevicesList;

  static const std::map<const RteDeviceItem::TYPE, const std::string> m_rteTypeStr;
  static const std::map<const RteDeviceItem::TYPE, const std::string> m_rteTypeStrHirachy;
};

#endif // VALIDATESYNTAX_H
