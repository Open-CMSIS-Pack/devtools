/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "SvdGenerator.h"
#include "SvdModel.h"
#include "ErrLog.h"
#include "MemoryMap.h"
#include "HeaderData.h"
#include "PartitionData.h"
#include "SfdData.h"
#include "SfrccInterface.h"
#include "SvdDevice.h"

#include <string>
#include <set>
#include <list>
#include <map>
using namespace std;

const string SvdGenerator::NAME_PERIPHERAL_LIST = string("MapPeripherals.txt");
const string SvdGenerator::NAME_REGISTER_LIST   = string("MapRegisters.txt");
const string SvdGenerator::NAME_FIELD_LIST      = string("MapFields.txt");


SvdGenerator::SvdGenerator(const SvdOptions& options) :
m_options(options),
m_deviceName("NoName")
{
}

SvdGenerator::~SvdGenerator()
{
}

bool SvdGenerator::SetProgramInfo(const string &version, const string &descr, const string &copyright)
{
  m_version   = version;
  m_descr     = descr;
  m_copyright = copyright;

  return true;
}

bool SvdGenerator::GetProgramInfo(string &version, string &descr, string &copyright)
{
  version   = m_version;
  descr     = m_descr;
  copyright = m_copyright;

  return true;
}

bool SvdGenerator::SetFileHeader(FileHeaderInfo &fileHeaderInfo, SvdDevice *device)
{
  string version, descr, copyright;
  GetProgramInfo(version, descr, copyright);

  fileHeaderInfo.svdFileName    = GetSvdFileName();
  fileHeaderInfo.descr          = descr;
  fileHeaderInfo.copyright      = copyright;
  fileHeaderInfo.version        = version;
  fileHeaderInfo.licenseText    = device->GetLicenseText();
  fileHeaderInfo.deviceVersion  = device->GetVersion();
  return true;
}

bool SvdGenerator::CmsisHeaderFile(SvdDevice *device, const string &path)
{
  SetOutPath(path);
  SetDeviceName(device->GetName());
  const auto fileName = GetCmsisHeaderFileName();

  FileHeaderInfo fileHeaderInfo;
  SetFileHeader(fileHeaderInfo, device);

  HeaderData *headerData = new HeaderData(fileHeaderInfo, m_options);
  headerData->Create(device, fileName);

  delete headerData;

  return true;
}

bool SvdGenerator::CmsisPartitionFile(SvdDevice *device, const string &path)
{
  SetOutPath(path);
  SetDeviceName(device->GetName());
  const auto fileName = GetCmsisPartitionFileName();

  FileHeaderInfo fileHeaderInfo;
  SetFileHeader(fileHeaderInfo, device);

  PartitionData *partitionData = new PartitionData(fileHeaderInfo, m_options);
  partitionData->Create(device, fileName);

  delete partitionData;

  return true;
}

bool SvdGenerator::SfdFile(SvdDevice *device, const string &path)
{
  SetOutPath(path);
  SetDeviceName(device->GetName());
  const auto fileName = GetSfdFileName();

  FileHeaderInfo fileHeaderInfo;
  SetFileHeader(fileHeaderInfo, device);

  const auto sfdData = new SfdData(fileHeaderInfo, m_options);
  sfdData->Create(device, fileName);

  delete sfdData;

  return true;
}

bool SvdGenerator::SfrFile(SvdDevice *device, const string &path)
{
  SetOutPath(path);
  SetDeviceName(device->GetName());
  const auto sfdFileName = GetSfdFileName();

  const auto sfrIf = new SfrccInterface();
  bool status = sfrIf->Compile(sfdFileName);

  delete sfrIf;

  return status;
}

bool SvdGenerator::PeripheralListing(SvdDevice *device, const string &path)
{
  SetOutPath(path);
  SetDeviceName(device->GetName());
  const auto fileName = GetPeripheralListFileName();

  FileHeaderInfo fileHeaderInfo;
  SetFileHeader(fileHeaderInfo, device);

  const auto memoryMap = new MemoryMap(fileHeaderInfo);
  memoryMap->CreateMap(device, fileName, MAPLEVEL_PERIPHERAL);
  delete memoryMap;

  return true;
}

bool SvdGenerator::RegisterListing(SvdDevice *device, const string &path)
{
  SetOutPath(path);
  SetDeviceName(device->GetName());
  const auto fileName = GetRegisterListFileName();

  FileHeaderInfo fileHeaderInfo;
  SetFileHeader(fileHeaderInfo, device);

  const auto memoryMap = new MemoryMap(fileHeaderInfo);
  memoryMap->CreateMap(device, fileName, MAPLEVEL_REGISTER);
  delete memoryMap;

  return true;
}

bool SvdGenerator::FieldListing(SvdDevice *device, const string &path)
{
  SetOutPath(path);
  SetDeviceName(device->GetName());
  const auto fileName = GetFieldListFileName();

  FileHeaderInfo fileHeaderInfo;
  SetFileHeader(fileHeaderInfo, device);

  const auto memoryMap = new MemoryMap(fileHeaderInfo);
  memoryMap->CreateMap(device, fileName, MAPLEVEL_FIELD);
  delete memoryMap;

  return true;
}

const string SvdGenerator::GetDeviceName()
{
  return m_deviceName;
}

string SvdGenerator::GetCmsisHeaderFileName()
{
  string name = GetOutPath();
  if(!name.empty()) {
    name += '/';
  }
  name += GetDeviceName();
  name += ".h";

  return name;
}

string SvdGenerator::GetSfdFileName()
{
  string name = GetOutPath();
  if(!name.empty()) {
    name += "/";
  }

  const string& overrideName = m_options.GetOutFilenameOverride();

  if(!overrideName.empty()) {
    name += overrideName;
  }
  else {
    name += GetDeviceName();
  }

  name += ".sfd";

  return name;
}

string SvdGenerator::GetSfrFileName()
{
  string name = GetOutPath();
  if(!name.empty()) {
    name += '/';
  }
  name += GetDeviceName();
  name += ".sfr";

  return name;
}

string SvdGenerator::GetCmsisPartitionFileName()
{
  string name = GetOutPath();
  if(!name.empty()) {
    name += '/';
  }
  name += "partition_";
  name += GetDeviceName();
  name += ".h";

  return name;
}

string SvdGenerator::GetPeripheralListFileName()
{
  string name = GetOutPath();
  if(!name.empty()) {
    name += '/';
  }
  name += GetDeviceName();
  name += "_";
  name += NAME_PERIPHERAL_LIST;

  return name;
}

string SvdGenerator::GetRegisterListFileName()
{
  string name = GetOutPath();
  if(!name.empty()) {
    name += '/';
  }
  name += GetDeviceName();
  name += "_";
  name += NAME_REGISTER_LIST;

  return name;
}

string SvdGenerator::GetFieldListFileName()
{
  string name = GetOutPath();
  if(!name.empty()) {
    name += '/';
  }
  name += GetDeviceName();
  name += "_";
  name += NAME_FIELD_LIST;

  return name;
}
