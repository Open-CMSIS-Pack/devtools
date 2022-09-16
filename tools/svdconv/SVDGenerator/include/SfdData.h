/*
 * Copyright (c) 2010-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SfdData_H
#define SfdData_H

#include "SvdItem.h"
#include "SfdGenerator.h"
#include "SvdGenerator.h"
#include "SvdOptions.h"
#include "FileIo.h"

#include <string>
#include <map>
#include <list>


using SFD_REGMAP = std::list<SvdItem*>;
using SFD_ITEMMAP = std::list<SvdItem*>;

class SvdDevice;
class SvdPeripheralContainer;
class SvdPeripheral;
class SvdRegisterContainer;
class SvdRegister;
class SvdCluster;
class SvdFieldContainer;
class SvdField;
class SvdEnumContainer;
class SvdEnum;

class SfdData {
public:
  SfdData(const FileHeaderInfo &fileHeaderInfo, const SvdOptions& options);
  ~SfdData();

  bool Create                       (SvdItem *item, const std::string &fileName);

protected:
  bool CreateDevice                 (SvdDevice*      device);

  bool CreateInterruptItems         (SvdDevice*      device);
  bool CreateExpressionRefs         (SvdDevice*      device);
  bool CreatePeripherals            (SvdItem*        cont,    std::list<SvdItem*> &peripheralList);
  bool CreatePeripheralMenu         (SvdDevice*      device,  std::list<SvdItem*> &list);
  bool CreatePeripheral             (SvdPeripheral*  peri,    std::list<SvdItem*> &peripheralList);
  bool CreatePeripheralArray        (SvdPeripheral*  peri,    std::list<SvdItem*> &peripheralList);
  bool CreatePeripheralArrayPeri    (SvdPeripheral*  peri,    std::list<SvdItem*> &peripheralList);
  bool CreatePeripheralView         (SvdPeripheral*  peri,    std::list<SvdItem*> registerList);
  bool CreateDisableCondition       (SvdPeripheral*  peri);
  bool CreateItemList               (const std::list<SvdItem*> &list);
  bool CreateItemDescription_       (SvdItem* item, const std::string& text, const std::string srcFile, uint32_t srcLine);
  bool CreateRegisters              (SvdItem*                 regs,   std::list<SvdItem*> &registerList);
  bool CreateRegClust               (SvdItem*                 item,   std::list<SvdItem*> &registerList);
  bool CreateFields                 (SvdItem*                 item,   std::list<SvdItem*> &list);
  bool CreateClusterArray           (SvdCluster*              clust,  std::list<SvdItem*> &list);
  bool CreateCluster                (SvdCluster*              clust,  std::list<SvdItem*> &lis);
  bool CreateRegisterArray          (SvdRegister*             reg  ,  std::list<SvdItem*> &list);
  bool CreateRegister               (SvdRegister*             reg  ,  std::list<SvdItem*> &list);
  bool CreateField                  (SvdField*                field,  std::list<SvdItem*> &list);
  bool CreatePeripheralArrayItree   (const std::string& prefix, SvdPeripheral* peri, std::list<SvdItem*> &periArrayList);
  bool CreatePeripheralArrayView    (const std::string& prefix, SvdPeripheral* peri);
  bool CreateFieldItem              (SvdField*      field);
  bool CreateEnumValues             (const std::list<SvdEnumContainer*>& conts, SvdField* field);
  bool CreateEnumValueSet           (SvdEnumContainer *cont, SvdField* field);
  bool CreateRegisterArrayItree     (SvdRegister*   reg   , std::list<SvdItem*> &regArrayList);
  bool CreateClusterArrayItree      (SvdCluster*    clust , std::list<SvdItem*> &clustArrayList);
  bool CreateRegisterItem           (SvdRegister*   reg);
  bool CreateRegisterArrayItem      (SvdRegister*   reg);
  bool CreateClusterItree           (SvdCluster* reg, std::list<SvdItem*> &clustArrayList);
  bool IsValid                      (SvdItem* item);

private:
  const SvdOptions&   m_options;
  FileIo*             m_fileIo;
  SfdGenerator*       m_gen;

  static const std::string m_addrWidthStr[];
};

#define CreateItemDescription(item, text)     CreateItemDescription_(item, text, __FILE__, __LINE__)

#endif // SfdData_H
