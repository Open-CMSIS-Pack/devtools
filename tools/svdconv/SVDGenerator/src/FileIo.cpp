/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "FileIo.h"
#include "ErrLog.h"
#include "SvdUtils.h"

#include <stdio.h>
#include <stdarg.h>
#include <fstream>

using namespace std;


const string FileIo::genericLicenseText = \
  "Copyright (c) 2009-2019 ARM Limited. All rights reserved.\\n"\
  "\\n"\
  "SPDX-License-Identifier: Apache-2.0\\n"\
  "\\n"\
  "Licensed under the Apache License, Version 2.0 (the License); you may\\n"\
  "not use this file except in compliance with the License.\\n"\
  "You may obtain a copy of the License at\\n"\
  "\\n"\
  "http://www.apache.org/licenses/LICENSE-2.0\\n"\
  "\\n"\
  "Unless required by applicable law or agreed to in writing, software\\n"\
  "distributed under the License is distributed on an AS IS BASIS, WITHOUT\\n"\
  "WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.\\n"\
  "See the License for the specific language governing permissions and\\n"\
  "limitations under the License.";


FileIo::FileIo() :
  m_tabSpaceCnt(0)
{
}

FileIo::~FileIo()
{
}

bool FileIo::Create(const string &fileName)
{
  SetFileName("");

  if(fileName.empty()) {
    return false;
  }

  ofstream fileStream(fileName);
  if(!fileStream.is_open()) {
    LogMsg("M130", NAME(fileName));
    return false;
  }

  fileStream.close();
  SetFileName(fileName);
  CreateFileDescription();

  return true;
}

bool FileIo::Write(const string& text)
{
  if(m_outFileStr.length() > FILE_BUF_SIZE) {
    Flush();
  }

  ConvertTab(m_outFileStr, text);

  return true;
}

bool FileIo::WriteLine(const char *text, ...)
{
  va_list   marker;
  char outBuf[1024];

  va_start (marker, text);
  vsnprintf(outBuf, 1024-2, text, marker);
  va_end(marker);

  Write(outBuf);
  Write("\n");

  return true;
}

bool FileIo::Flush()
{
  if(m_outFileStr.empty()) {
    return false;
  }

  const auto& fileName = GetFileName();
  ofstream fileStream(fileName, ofstream::out | ofstream::binary | ofstream::app);   // append
  if (!fileStream.is_open()) {
    return false;
  }

  fileStream << m_outFileStr;
  fileStream.flush();
  fileStream.close();
  m_outFileStr.clear();

  return true;
}

bool FileIo::Close()
{
  Write("\n");
  Flush();

  return true;
}

bool FileIo::WriteText(const string& text)
{
  Write(text);

  return true;
}

bool FileIo::WriteChar(const char c)
{
  string text;
  text += c;
  Write(text);

  return true;
}

uint32_t FileIo::ConvertTab(string& dest, const string& src)
{
  static uint32_t s_charCnt = 0;
  uint32_t j;
  uint32_t lenToNextTab = 0;
  uint32_t charCnt = 0;

  for(auto c : src) {
    if(c == '\n') {
      s_charCnt = 0;
      m_tabSpaceCnt = 0;
      dest += c;
      charCnt++;
    }
    else if(c == '\r') {
      m_tabSpaceCnt = 0;
    }
    else if(c == '\t') {
      if(m_tabSpaceCnt <=  s_charCnt) {  // if((m_tabSpaceCnt + SPACES_PER_TAB_FIO) <=  s_charCnt) {
        m_tabSpaceCnt += SPACES_PER_TAB_FIO;
      }
      else {
        lenToNextTab = SPACES_PER_TAB_FIO - (s_charCnt % SPACES_PER_TAB_FIO);      // calculate len to next tab
        if(!lenToNextTab) {
          lenToNextTab = SPACES_PER_TAB_FIO;
        }
        m_tabSpaceCnt += lenToNextTab;

        for(j=0; j<lenToNextTab; j++) {
          dest += ' ';
          charCnt++;
          s_charCnt++;
        }
      }
    }
    else {
      dest += c;
      charCnt++;
      s_charCnt++;
      m_tabSpaceCnt++;
    }
  }

  return charCnt;
}

bool FileIo::CreateFileDescription()
{
  const string& fileName = GetSvdFileName();

  time_t result = time(nullptr);
  const char* timeAsc = asctime(std::localtime(&result));
  string timeText { "<unknown>" };
  if(timeAsc) {
    timeText = timeAsc;

    if(*timeText.rbegin() == '\n') {
      timeText.pop_back();    // erase '\n'
    }
  }

  error_code ec;
  auto ftime = filesystem::last_write_time(fileName, ec);
  time_t cftime = ToTime(ftime);
  const char* fTimeAsc = asctime(std::localtime(&cftime));
  string fTimeText { "<unknown>" };
  if(fTimeAsc) {
    fTimeText = fTimeAsc;
    if(*fTimeText.rbegin() == '\n') {
      fTimeText.pop_back();    // erase '\n'
    }
  }

  string::size_type pos;
  string outFileName = GetFileName();
  pos = outFileName.find_last_of('\\');
  if(pos != string::npos) {
    outFileName.erase(0, pos+1);
  }

  string svdFileName = GetSvdFileName();
  pos = svdFileName.find_last_of('\\');
  if(pos != string::npos) {
    svdFileName.erase(0, pos+1);
  }

  string licenseText = GetLicenseText();
  if(licenseText.empty()) {
    licenseText = genericLicenseText;
  }

  string deviceVersion = GetDeviceVersion();
  if(deviceVersion.empty()) {
    deviceVersion = "1.0";
  }

  map<uint32_t, string> formatedText;
  SvdUtils::FormatText(formatedText, licenseText, 100);

  string textBuf = "/*";
  for(const auto& [key, text] : formatedText) {
    textBuf += "\n * ";
    textBuf += text;
  }

  textBuf += "\n *";
  textBuf += "\n * @file     ";
  textBuf += outFileName;
  textBuf += "\n * @brief    ";
  textBuf += GetBriefDescription();
  textBuf += "\n * @version  ";
  textBuf += deviceVersion;
  textBuf += "\n * @date     ";
  textBuf += timeText;
  textBuf += "\n * @note     Generated by SVDConv V";
  textBuf += GetVersionString();
  textBuf += "\n *           from File '";
  textBuf += svdFileName;
  textBuf += "',";
  textBuf += "\n *           last modified on ";
  textBuf += fTimeText;
  textBuf += "\n */\n";

  WriteText(textBuf);

  return true;
}
