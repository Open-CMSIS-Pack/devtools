/*
 * Copyright (c) 2010-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef FileIo_H
#define FileIo_H

#include <string>
#include <map>
#include <chrono>
#include <filesystem>

#define FILE_BUF_SIZE           (1024 * 1024)
#define SPACES_PER_TAB_FIO      2


class FileIo {
public:
  FileIo();
  ~FileIo();

  bool                Create                    (const std::string &fileName);
  bool                Write                     (const std::string& text);
  bool                WriteText                 (const std::string& text);
  bool                WriteChar                 (const char c);
  bool                WriteLine                 (const char *text, ...);
  bool                Flush                     (void);
  bool                Close                     ();

  bool                SetFileName               (const std::string fileName)      { m_fileName = fileName; return true; }
  const std::string&  GetFileName               ()                                { return m_fileName; }
  bool                SetSvdFileName            (const std::string fileName)      { m_svdFileName = fileName; return true; }
  const std::string&  GetSvdFileName            ()                                { return m_svdFileName; }
  bool                SetVersionString          (const std::string text)          { m_versionString = text; return true; }
  const std::string&  GetVersionString          ()                                { return m_versionString; }
  bool                SetCopyrightString        (const std::string text)          { m_copyrightString = text; return true; }
  const std::string&  GetCopyrightString        ()                                { return m_copyrightString; }
  bool                SetProgramDescription     (const std::string text)          { m_programmDescription = text; return true; }
  const std::string&  GetProgramDescription     ()                                { return m_programmDescription; }
  bool                SetBriefDescription       (const std::string text)          { m_briefDescription = text; return true; }
  const std::string&  GetBriefDescription       ()                                { return m_briefDescription; }
  bool                SetLicenseText            (const std::string text)          { m_licenseText = text; return true; }
  const std::string&  GetLicenseText            ()                                { return m_licenseText; }
  bool                SetDeviceVersion          (const std::string text)          { m_deviceVersion = text; return true; }
  const std::string&  GetDeviceVersion          ()                                { return m_deviceVersion; }

protected:
  uint32_t            ConvertTab                (std::string& dest, const std::string& src);
  bool                CreateFileDescription     ();

  // see https://stackoverflow.com/questions/56788745/how-to-convert-stdfilesystemfile-time-type-to-a-string-using-gcc-9/58237530#58237530
  template <typename t>
  std::time_t ToTime(t tp) {
    auto time = std::chrono::time_point_cast<std::chrono::system_clock::duration>(tp - t::clock::now() + std::chrono::system_clock::now());
    return std::chrono::system_clock::to_time_t(time);
  }

private:
  uint32_t      m_tabSpaceCnt;
  std::string   m_fileName;
  std::string   m_svdFileName;
  std::string   m_versionString;
  std::string   m_copyrightString;
  std::string   m_programmDescription;
  std::string   m_briefDescription;
  std::string   m_licenseText;
  std::string   m_deviceVersion;
  std::string   m_outFileStr;

  static const std::string genericLicenseText;
};

#endif // FileIo_H
