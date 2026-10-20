/*
 * Copyright (c) 2010-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef HeaderData_H
#define HeaderData_H

#include "SvdItem.h"
#include "HeaderGenerator.h"
#include "SvdGenerator.h"
#include "SvdOptions.h"
#include "FileIo.h"
#include "SvdField.h"

#include <stdint.h>
#include <string>
#include <map>
#include <list>

struct ONESTRUCT {
  uint32_t mask;
  std::map<uint32_t, SvdField*> fields;

  ONESTRUCT() : mask(0) {}
};

using REGMAP = std::map<uint64_t, std::list<SvdItem*> >;
using FIELDMAPLIST = std::list<ONESTRUCT>;

#define MAX_REGS      32

struct RegNode {
  SvdItem        *reg[MAX_REGS];
  uint32_t        regItems;
};

struct RegSorter {
  uint32_t     address;
  uint32_t     unaligned;
  RegNode      accessINT;
  RegNode      accessSHORT[2];
  RegNode      accessBYTE[4];
  RegNode      accessINT64;
};

enum class REGTYPE {
  REG_NULL    = 0,
  REG_BYTE    = 1,
  REG_SHORT   = 2,
  REG_INT     = 4
};

struct RegTreeNode {
  REGTYPE          type;
  uint32_t         regItems;
  SvdItem         *reg[MAX_REGS];
  RegTreeNode     *pos[4];
};

struct StructUnion {
  bool isUnion;
  uint32_t num;
};

struct PosMaskNames {
  std::string name;
  std::string reg;
  std::string alternate;
};

struct EnumValuesNames {
  std::string name;
  std::string reg;
  std::string alternate;
  std::string field;
  std::string headerEnumName;
};


class SvdDevice;
class SvdPeripheral;
class SvdRegister;
class SvdCluster;
class SvdField;
class SvdEnum;
class SvdEnumContainer;
class SvdEnum;

class HeaderData {
public:
  HeaderData(const FileHeaderInfo &fileHeaderInfo, const SvdOptions& options);
  ~HeaderData();

  bool      Create(SvdItem *item, const std::string &fileName);

protected:
  bool      CreateInterruptList                     (SvdDevice *device);
  bool      CreateInterrupt                         (const std::string &name, const std::string &descr, int32_t num, bool lastEnum);
  bool      CreateHeaderStart                       (SvdDevice *device);
  bool      CreateHeaderEnd                         (SvdDevice *device);
  bool      CreateCmsisConfig                       (SvdDevice *device);
  bool      CreateAnnonUnionStart                   (SvdDevice *device);
  bool      CreateAnnonUnionEnd                     (SvdDevice *device);

  // HeaderData_Cluster
  bool      CreateClusters                          (SvdDevice* device);
  bool      CreateCluster                           (SvdCluster* cluster);
  bool      OpenCluster                             (SvdCluster* cluster);
  bool      CloseCluster                            (SvdCluster* cluster);

  // HeaderData_Peripheral
  bool      CreatePeripherals                       (SvdDevice* device);

  bool      CreatePeripheralsInstance               (SvdDevice* device);
  bool      CreatePeripheralInstance                (SvdPeripheral* peripheral);

  bool      CreatePeripheralsAddressMap             (SvdDevice* device);
  bool      CreatePeripheralAddressMap              (SvdPeripheral* peripheral);

  bool      CreatePeripheralsType                   (SvdDevice* device);
  bool      CreatePeripheralType                    (SvdPeripheral* peripheral);
  bool      OpenPeripheral                          (SvdPeripheral* peripheral);
  bool      ClosePeripheral                         (SvdPeripheral* peripheral);

  // HeaderData_Registers
  bool      CreateRegisters                         (SvdItem*     container);
  bool      AddRegisters                            (SvdItem*     container, REGMAP &m_sortedRegs);
  uint32_t  CreateRegister                          (SvdRegister* reg);
  uint32_t  CreateRegCluster                        (SvdCluster*  cluster);
  bool      CreateSortedRegisters                   (const REGMAP &regs);
  bool      CreateSortedRegisterGroup               (const std::list<SvdItem*>& regGroup, uint64_t baseAddr);
  bool      CheckAlignment                          (SvdItem* item);
  uint32_t  CreateSvdItem                           (SvdItem *item, uint64_t address);

  // HeaderData_RegStructure
  bool            AddNode_Register                  (SvdItem *reg, RegSorter* pRegSorter);
  void            GeneratePart                      (RegSorter *pRegSorter);
  uint32_t        GenerateNode                      (RegTreeNode *node, uint32_t address, uint32_t level, uint32_t &sizeMax, uint32_t localOffset = 0);
  uint32_t        GenerateChildNodes                (RegTreeNode *node, uint32_t address, uint32_t level);
  uint32_t        GenerateRegItems                  (RegTreeNode *node, uint32_t address, uint32_t level);
  void            GenerateReserved                  (int32_t resBytes,  uint32_t address, bool bGenerate = true);
  void            GenerateReserved                  ();
  void            GenerateReservedWidth             (int32_t nMany, uint32_t width);
  void            GenerateReservedField             (int32_t resBits, uint32_t regSize);
  void            AddReservedBytesLater             (int32_t reservedBytes);
  uint32_t        NodeHasChilds                     (RegTreeNode *node, uint32_t pos);
  bool            NodeHasRegs                       (RegTreeNode *node);
  bool            NodeValid                         (RegTreeNode *node);
  RegTreeNode    *GetNextRegNode                    (bool first = 0);
  void            CreateStructUnion                 (bool isUnion, bool open, uint32_t num);
  uint32_t        GetUnionStructNum                 ();
  bool            IsUnion                           ();
  bool            InStuctUnion                      (uint32_t num);
  bool            InStruct                          ();
  bool            InUnion                           ();
  void            Pop_structUnionStack              (bool &isUnion, uint32_t &num);
  void            Push_structUnionStack             (bool isUnion, uint32_t num);
  void            Init_OpenCloseStructUnion         ();

  // HeaderData_Fields
  uint32_t        CreateField                       (SvdField* field, uint32_t regSize, uint32_t offsCnt);
  bool            CreateSortedFields                (const FIELDMAPLIST& sortedFields, uint32_t regSize, std::string structName);
  bool            AddFields                         (SvdItem* container, FIELDMAPLIST& sortedFields);
  bool            CreateFields                      (SvdRegister* reg, std::string structName);

  // HeaderData_PosMask
  bool            CreatePosMask                     (SvdDevice* device);
  bool            CreateClustersPosMask             (SvdDevice* device);
  bool            CreateClusterPosMask              (SvdCluster* cluster);
  bool            CreateRegistersPosMask            (SvdItem* container,  PosMaskNames *posMaskNames);
  bool            CreateRegisterPosMask             (SvdRegister* reg,    PosMaskNames *posMaskNames);
  bool            CreateFieldPosMask                (SvdField* field,     PosMaskNames *posMaskNames);
  bool            CreatePeripheralsPosMask          (SvdDevice* device);
  bool            CreatePeripheralPosMask           (SvdPeripheral* peri);

  // HeaderData_EnumValues
  bool            CreateEnumValue                   (SvdDevice* device);
  bool            CreateClustersEnumValue           (SvdDevice* device);
  bool            CreatePeripheralsEnumValue        (SvdDevice* device);
  bool            CreatePeripheralEnumValue         (SvdPeripheral* peri);
  bool            CreateClusterEnumValue            (SvdCluster* cluster);
  bool            CreateClusterRegistersEnumValue   (SvdItem* container,          EnumValuesNames *enumValuesNames);
  bool            CreateRegistersEnumValue          (SvdItem* container,          EnumValuesNames *enumValuesNames);
  bool            CreateRegisterEnumValue           (SvdRegister* reg,            EnumValuesNames *enumValuesNames);
  bool            CreateFieldEnumValue              (SvdField* field,             EnumValuesNames *enumValuesNames);
  bool            CreateEnumValuesContainer         (SvdEnumContainer* enumCont,  EnumValuesNames *enumValuesNames);
  bool            CreateEnumValue                   (SvdEnum* enu,                EnumValuesNames *enumValuesNames);
  bool            CreateRegisterEnumArrayValue      (SvdRegister* reg,            EnumValuesNames *enumValuesNames);
  bool            CreateClusterEnumArrayValue       (SvdCluster* clust,           EnumValuesNames *enumValuesNames);
  bool            CreatePeripheralEnumArrayValue    (SvdPeripheral* peri,         EnumValuesNames *enumValuesNames);

  void            SetMaxBitWidth                    (uint32_t width) { m_maxBitWidth = width; }
  uint32_t        GetMaxBitWidth                    () { return m_maxBitWidth; }


  struct ReservedPad {
    int32_t nMany;
    uint32_t width;
  };

  void AddReservedPad(int32_t bytes);

private:
  const SvdOptions&   m_options;
  FileIo*             m_fileIo;
  HeaderGenerator*    m_gen;
  bool                m_bDebugHeaderfile;
  bool                m_bDebugStruct;
  uint32_t            m_addressCnt;
  uint32_t            m_reservedCnt;         // counts the "RESERVEDn number
  uint32_t            m_reservedFieldCnt;
  uint32_t            m_inUnion;
  int32_t             m_addReservedBytesLater;
  char                m_prevWasStruct;
  char                m_prevWasUnion;
  uint32_t            m_structUnionPos;
  uint32_t            m_maxBitWidth;
  RegTreeNode*        m_rootNode;
  RegTreeNode         m_regTreeNodes[32];     // 32 placeholder
  StructUnion         m_structUnionStack[32];

  std::map<std::string, SvdEnum*> m_usedEnumValues;   // check enum names globally
  std::list<ReservedPad>          m_reservedPad;

  static const std::string m_anonUnionStart;
  static const std::string m_anonUnionEnd;
};

#endif // HeaderData_H
