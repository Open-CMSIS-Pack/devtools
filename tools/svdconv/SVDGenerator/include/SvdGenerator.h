/*
 * Copyright (c) 2010-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SvdGenerator_H
#define SvdGenerator_H

#include "SvdOptions.h"

#include <string>


enum MapLevel {
  MAPLEVEL_PERIPHERAL = 0,
  MAPLEVEL_REGISTER   = 1,
  MAPLEVEL_FIELD      = 2,
};

struct FileHeaderInfo {
  std::string version;
  std::string descr;
  std::string copyright;
  std::string svdFileName;
  std::string licenseText;
  std::string deviceVersion;
};

class Options;
class SvdItem;
class SvdDevice;
class FileIo;

class SvdGenerator {
public:
  SvdGenerator(const SvdOptions& options);
  ~SvdGenerator();

  bool            SetProgramInfo      (const std::string &version, const std::string &descr, const std::string &copyright);
  bool            GetProgramInfo      (      std::string &version,       std::string &descr,       std::string &copyright);
  bool            SetFileHeader       (FileHeaderInfo &fileHeaderInfo, SvdDevice *device);

  bool            CmsisHeaderFile     (SvdDevice *device, const std::string &path);
  bool            CmsisPartitionFile  (SvdDevice *device, const std::string &path);
  bool            SfdFile             (SvdDevice *device, const std::string &path);
  bool            SfrFile             (SvdDevice *device, const std::string &path);
  bool            PeripheralListing   (SvdDevice *device, const std::string &path);
  bool            RegisterListing     (SvdDevice *device, const std::string &path);
  bool            FieldListing        (SvdDevice *device, const std::string &path);

  bool                  SetOutPath          (const std::string &path)     { m_outPath = path; return true; }
  const std::string&    GetOutPath          ()                            { return m_outPath; }

  bool                  SetDeviceName       (const std::string &name)     { m_deviceName = name; return true; }
  const std::string     GetDeviceName       ();


  bool                  SetSvdFileName       (const std::string &name)    { m_svdFileName = name; return true; }
  const std::string&    GetSvdFileName       ()                           { return m_svdFileName; }

  std::string           GetCmsisHeaderFileName    ();
  std::string           GetSfdFileName            ();
  std::string           GetSfrFileName            ();
  std::string           GetCmsisPartitionFileName ();
  std::string           GetPeripheralListFileName ();
  std::string           GetRegisterListFileName   ();
  std::string           GetFieldListFileName      ();

protected:

private:
  const SvdOptions&   m_options;

  std::string         m_outPath;
  std::string         m_deviceName;
  std::string         m_svdFileName;
  std::string         m_version;
  std::string         m_descr;
  std::string         m_copyright;

  static const std::string NAME_PERIPHERAL_LIST;
  static const std::string NAME_REGISTER_LIST;
  static const std::string NAME_FIELD_LIST;
};

#endif // SvdGenerator_H
