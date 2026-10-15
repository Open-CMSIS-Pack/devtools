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
#include "SvdDimension.h"
#include "SvdField.h"
#include "SvdRegister.h"
#include "SvdUtils.h"
#include "ErrLog.h"

using namespace std;

namespace {

bool CheckSfdBitRanges(SvdItem* item)
{
  if(!item->IsValid()) {
    return true;
  }

  const auto reg = dynamic_cast<SvdRegister*>(item);
  const auto field = dynamic_cast<SvdField*>(item);
  if(reg || field) {
    const auto width = item->GetEffectiveBitWidth();
    const auto mask = SvdUtils::CreateBitMask(width, field ? field->GetOffset() : 0);
    if(!mask || mask > UINT32_MAX) {
      LogMsg("M226", MSG("SFD supports only bit ranges within 32 bits: '" + item->GetNameCalculated() + "'"), item->GetLineNumber());
      return false;
    }
  }

  for(const auto child : item->GetChildren()) {
    if(!CheckSfdBitRanges(child)) {
      return false;
    }
  }
  const auto dim = item->GetDimension();
  return !dim || CheckSfdBitRanges(dim);
}

} // namespace


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

  const auto device = dynamic_cast<SvdDevice*>(item);
  if(!device || !CheckSfdBitRanges(device)) {
    return false;
  }
  for(const auto& [name, reg] : device->GetExpressionRegistersList()) {
    if(!reg || !reg->IsValid()) {
      LogMsg("M226", MSG("Invalid register referenced by an SFD expression: '" + name + "'"));
      return false;
    }
    if(!CheckSfdBitRanges(reg)) {
      return false;
    }
  }

  m_fileIo->Create(fileName);

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
