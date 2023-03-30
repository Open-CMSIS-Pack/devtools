/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// RteChk.h : RTE Model tester

#include "RtePackage.h"
#include "RteComponent.h"

#include <string>
#include <set>
#include <iostream>

class RteModel;
class RteDeviceElement;
class RteDeviceProperty;
class RteDeviceItem;
class RteDeviceItemAggregate;
class RteItemBuilder;
class XMLTree;

enum PACK_TYPE { PACK_GENERIC, PACK_DFP, PACK_BSP };

class RteChk
{
public:
  RteChk(std::ostream& os = std::cout);
  ~RteChk();

  static int CheckRte(int argc, char* argv[]);

  void SetFlag(unsigned flag, char op = '+');
  unsigned GetFlags() const { return m_flags; }
  bool IsFlagSet(unsigned flag) const;

  int RunCheckRte();
  void AddFileDir(const std::string& path);

  int ProcessArguments(int argc, char* argv[]);

  void DumpModel();

  void PrintText(const std::string& str);

  void DumpConditions(RtePackage* pack);
  void DumpComponents(RtePackage* pack);
  void DumpExamples  (RtePackage* pack);

  void DumpDeviceAggregate(RteDeviceItemAggregate* da);
  void DumpDeviceItem(RteDeviceItem* d, const std::string& dName, bool endLeaf);
  void DumpDeviceElement(RteDeviceElement* e);
  void DumpEffectiveContent(RteDeviceProperty* d);
  void DumpEffectiveProperties(RteDeviceItem* d, const std::string& processorName, bool endLeaf);

  void ListPacks();
  void CollectPacks(RtePackageMap& packs, PACK_TYPE packType);
  void PrintPacks(const RtePackageMap& packs);

  void ListComponents();
  void CollectComponents(RteComponentMap& components, PACK_TYPE packType);
  void PrintComponents(RteComponentMap& components, bool bFiles);
  void PrintComponent(RteComponent* pComp, bool bFiles);

  unsigned long GetPackCount() const;
  int GetComponentCount() const;
  int GetDeviceCount() const;
  int GetBoardCount() const;

  const RteModel* GetRteModel() const { return m_rteModel; }

  unsigned long CollectFiles();
  static unsigned CharToFlagValue(char ch);
  static PACK_TYPE GetPackageType(RtePackage* pack);

  // flags
  static const unsigned DUMP = 0x0001;
  static const unsigned TIME = 0x0002;
  static const unsigned VALIDATE = 0x0004;
  static const unsigned COMPONENTS = 0x0008;
  static const unsigned PACKS = 0x0010;
  static const unsigned DFP = 0x0020;
  static const unsigned BSP = 0x0040;
  static const unsigned ALL = 0xFFFF;
  static const unsigned NONE = 0x0000;

private:
  RteModel* m_rteModel;
  XMLTree* m_xmlTree;
  RteItemBuilder* m_rteItemBuilder;
  std::set<std::string> m_files;
  std::set<std::string> m_dirs;

  unsigned m_flags;
  unsigned long m_npdsc;
  int m_indent;
  std::ostream& m_os;
};
