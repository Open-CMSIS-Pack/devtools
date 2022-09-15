/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdarg.h>
#include "SfdData.h"
#include "SfdGenerator.h"
#include "SvdItem.h"
#include "FileIo.h"
#include "SvdDevice.h"

using namespace std;


SfdData::SfdData(const FileHeaderInfo &fileHeaderInfo, const SvdOptions& options):
  m_options(options),
  m_fileIo(nullptr),
  m_gen(nullptr)
{
  m_fileIo = new FileIo();
  m_gen    = new SfdGenerator(m_fileIo);
  
  m_fileIo->SetSvdFileName        (fileHeaderInfo.svdFileName);
  m_fileIo->SetProgramDescription (fileHeaderInfo.descr);
  m_fileIo->SetCopyrightString    (fileHeaderInfo.copyright);
  m_fileIo->SetVersionString      (fileHeaderInfo.version);
  m_fileIo->SetLicenseText        (fileHeaderInfo.licenseText);
  m_fileIo->SetDeviceVersion      (fileHeaderInfo.deviceVersion);
  m_fileIo->SetBriefDescription   ("CMSIS-SVD SFD File");
}

SfdData::~SfdData()
{
  m_fileIo->Close();

  delete m_gen;
  delete m_fileIo;
}

bool SfdData::Create(SvdItem *item, const string &fileName)
{
  if(!item) {
    return false;
  }

  m_fileIo->Create(fileName);

  const auto device = dynamic_cast<SvdDevice*>(item);
  if(!device) {
    return false;
  }

  CreateDevice(device);

  return true;
}

bool SfdData::IsValid(SvdItem* item)
{
  if(!item) {
    return false;
  }

  if(!item->IsValid()) {
    return false;
  }

  return true;
}
