/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "CbuildUtils.h"

#include "ErrLog.h"
#include "RteFile.h"
#include "RteUtils.h"
#include "XMLTreeSlim.h"

#include "CrossPlatform.h"
#include "CrossPlatformUtils.h"

#include <algorithm>
#include <array>
#include <functional>
#include <cstring>


using namespace std;

CbuildUtils::CbuildUtils() {
}

CbuildUtils::~CbuildUtils() {
}

RteFile::Category CbuildUtils::GetFileType(const RteFile::Category cat, const string& file) {
  switch (cat) {
    case RteFile::Category::SOURCE_C:
      return RteFile::Category::SOURCE_C;
    case RteFile::Category::SOURCE_CPP:
      return RteFile::Category::SOURCE_CPP;
    case RteFile::Category::SOURCE_ASM:
      return RteFile::Category::SOURCE_ASM;
    case RteFile::Category::SOURCE: {         // check if category == SOURCE
      fs::path ext((fs::path(file)).extension());
      if (ext.compare(".c") == 0)
        return RteFile::Category::SOURCE_C;
      if (ext.compare(".C") == 0)
        return RteFile::Category::SOURCE_C;
      if (ext.compare(".cpp") == 0)
        return RteFile::Category::SOURCE_CPP;
      if (ext.compare(".c++") == 0)
        return RteFile::Category::SOURCE_CPP;
      if (ext.compare(".C++") == 0)
        return RteFile::Category::SOURCE_CPP;
      if (ext.compare(".cc") == 0)
        return RteFile::Category::SOURCE_CPP;
      if (ext.compare(".CC") == 0)
        return RteFile::Category::SOURCE_CPP;
      if (ext.compare(".cxx") == 0)
        return RteFile::Category::SOURCE_CPP;
      if (ext.compare(".asm") == 0)
        return RteFile::Category::SOURCE_ASM;
      if (ext.compare(".s") == 0)
        return RteFile::Category::SOURCE_ASM;
      if (ext.compare(".S") == 0)
        return RteFile::Category::SOURCE_ASM;
      return RteFile::Category::OTHER;
    }
    default: return cat;
  }
}

const string CbuildUtils::RemoveSlash(const string& path) {
  size_t s = 0;
  string result = path;
  while ((s = result.find("/", s)) != string::npos) {
    result.replace(s, 1, "");
  }
  return result;
}

const string CbuildUtils::RemoveTrailingSlash(const string& path) {
  string result = path;
  if (result.rfind(SS) == result.length() - 1) result.pop_back();
  return result;
}

const string CbuildUtils::ReplaceColon(const string& path) {
  size_t s = 0;
  string result = path;
  while ((s = result.find(":", s)) != string::npos) {
    result.replace(s, 1, "_");
  }
  return result;
}

const string CbuildUtils::ReplaceSpacesByQuestionMarks(const string& path) {
  size_t s = 0;
  string result = path;
  while ((s = result.find(" ", s)) != string::npos) {
    result.replace(s, 1, "?");
  }
  return result;
}

const string CbuildUtils::EscapeQuotes(const string& path) {
  size_t s = 0;
  string result = path;
  // escape backslashes
  while ((s = result.find("\\", s)) != string::npos) {
    result.replace(s, 1, "\\\\");
    s += 2;
  }
  s = 0;
  // escape quotes
  while ((s = result.find("\"", s)) != string::npos) {
    result.replace(s, 1, "\\\"");
    s += 2;
  }
  return result;
}

const string CbuildUtils::EscapeSpaces(const string& path) {
  size_t s = 0;
  string result = path;
  while ((s = result.find(WS, s)) != string::npos) {
    if ((s == 0) || (result.compare(s - 1, 1, BS) != 0)) {
      result.insert(s, BS);
      s += 2;
    }
  }
  return result;
}

const RteItem* CbuildUtils::GetItemByTagAndAttribute(const Collection<RteItem*>& children, const string& tag, const string& attribute, const string& value)
{
  for(auto it = children.begin(); it != children.end(); it++){
    RteItem* item = *it;
    if ((item->GetTag() == tag) && (item->GetAttribute(attribute) == value))
      return item;
  }
  return NULL;
}

const string CbuildUtils::GetLocalTimestamp()
{
  // Timestamp
  time_t rawtime;
  time (&rawtime);
  char timestamp[sizeof("0000-00-00T00:00:00")];
  struct tm *timeinfo = localtime(&rawtime);
  if (timeinfo) {
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", timeinfo);
  }
  return string(timestamp);
}

const string CbuildUtils::StrPathConv(const string& path) {
  if (path.empty())
    return path;
  string result = path;
  size_t s = 0;
  while ((s = result.find("\\", s)) != string::npos) {
    result.replace(s, 1, "/");
  }
  return result;
}

const string CbuildUtils::StrPathAbsolute(const string& flagText, const string& base) {
  if (flagText.empty())
    return flagText;
  const set<string>& prefixes = {LDOTS, LDOT};
  size_t offset = 0;
  string path = StrPathConv(flagText);
  path.erase(std::remove(path.begin(), path.end(), '\"'), path.end());
  for (auto prefix : prefixes) {
    offset = path.find(prefix);
    if (offset != string::npos)
      break;
  }
  if (offset == string::npos)
    return flagText;

  // Relative path recognized
  string result = path;
  if (offset > 0)
    path = path.substr(offset, string::npos);
  RteFsUtils::NormalizePath(path, base);
  return result.replace(offset, string::npos, "\"" + path + "\"");
}

string CbuildUtils::GenerateJsonPackList(const std::vector<CbuildPackItem>& packList) {
  string json;
  if (!packList.empty()) {
    json += "[";
    for (const auto& [vendor, name, version] : packList) {
      json += "\n{\"vendor\": \"" + vendor + "\", ";
      json += "\"name\": \"" + name + "\"";
      json += (version.empty()? "" : ", \"version\": \"" + version + "\"") + "},";
    }
    json.pop_back();
    json += "\n]\n";
  }
  return json;
}

bool CbuildUtils::NormalizePath(std::string& path, const std::string& base) {
  bool bNetworkPath = ((path.compare(0, 2, DS) == 0) || (path.compare(0, 2, DBS) == 0)) ? true : false;
  string pathStr = bNetworkPath ? path : base + path;
  if (!RteFsUtils::Exists(pathStr)) {
    return false;
  }
  RteFsUtils::NormalizePath(path, bNetworkPath ? RteUtils::EMPTY_STRING : base);
  return true;
}
