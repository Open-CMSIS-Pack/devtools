/*
 * Copyright (c) 2010-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef MemoryMap_H
#define MemoryMap_H

#include "SvdGenerator.h"
#include "SvdItem.h"

#include <string>


class FileIo;
class SvdDevice;
class SvdPeripheral;
class SvdRegister;
class SvdCluster;
class SvdField;
class SvdEnumContainer;
class SvdEnum;
class SfdGenerator;

class MemoryMap {
public:
  MemoryMap(FileHeaderInfo &fileHeaderInfo);
  virtual ~MemoryMap();

  bool          CreateMap       (SvdItem *item, const std::string &fileName, MapLevel mapLevel);
  void          SetFileIo       (FileIo *fileIo)  { m_fileIo = fileIo; }
  FileIo*       GetFileIo       ()                { return m_fileIo; }

protected:
  bool          IteratePeripherals      (SvdDevice     *device, MapLevel mapLevel);
  bool          IterateRegisters        (SvdPeripheral *peri  , MapLevel mapLevel);
  bool          IterateClusterRegisters (SvdCluster    *clust , MapLevel mapLevel);
  bool          IterateFields           (SvdRegister   *reg   , MapLevel mapLevel);
  bool          IterateEnums            (SvdField      *field , MapLevel mapLevel);

  bool          EnumContainer   (SvdEnumContainer *enu);
  bool          EnumValue       (SvdEnum       *enu);
  bool          Field           (SvdField      *field);
  bool          Register        (SvdRegister   *reg);
  bool          Cluster         (SvdCluster *cluster);
  bool          Peripheral      (SvdPeripheral *peri);

  bool          AddressBlock    (SvdPeripheral *peripheral);
  bool          Interrupt       (SvdDevice *device);

  bool          EnumInfo        (SvdEnum     *item);
  bool          FieldInfo       (SvdField    *item);
  bool          RegisterInfo    (SvdRegister *item);
  bool          ClusterInfo     (SvdCluster  *item);
  bool          PeripheralInfo  (SvdItem     *item);
  bool          DimInfo         (SvdItem     *item);
  bool          DeriveInfo      (SvdItem     *item);

private:
  FileIo*                   m_fileIo;
  SfdGenerator*             m_gen;

  static const std::string  m_access_str[];
};

#endif // MemoryMap_H
