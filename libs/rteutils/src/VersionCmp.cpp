/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file VersionCmp.cpp
* @brief Semantic version comparison
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "VersionCmp.h"

#include "AlnumCmp.h"
#include "RteUtils.h"

#include <cstring>

using namespace std;

/**
* Internal helper class ported from CMSIS Eclipse plug-in and optimized for C
*/
static const char* ZERO_STRING = "0";
#define MAX_BUF 256

class Version {
private:
  char buf[MAX_BUF];
  char* m_ptr;
  const char* segments[3]; // first three version segments : MAJOR.MINOR.PATCH
  char* release; // remainder (after '-');

public:
  Version(const string& ver) : m_ptr(0), release(0) {
    init(ver);
  }

private:

  void init(const string& version) {
    // leave one char room for extra 0
    size_t len = version.length();
    if (len >= MAX_BUF - 1) {
      len = MAX_BUF - 2;
    }
    memcpy(buf, version.c_str(), len);
    buf[len] = '\0';
    m_ptr = buf;
    // 1. drop build metadata
    char* p = strchr(m_ptr, '+');
    if (p)
      *p = '\0';
    parse(m_ptr);
  }

  void parse(char* ptr) {
    // 2. extract release
    char* p = strchr(ptr, '-');
    if (p != 0) {
      release = p + 1;
      *p = 0;
    } else if (ptr == buf && *ptr) { // initial parse
    // check for special ST case without dash like 1.2.3b < 1.2.3
      int lastIndex = (int)strlen(buf) - 1;
      for (int pos = lastIndex; pos >= 0; pos--) {
        char ch = buf[pos];
        if (ch == '.')
          break;
        if (!isdigit(ch))
          continue;
        if (pos < lastIndex) {
          for (int i = lastIndex; i > pos; i--)
            ptr[i + 1] = ptr[i];
          ptr[lastIndex + 2] = '\0';
          pos++;
          ptr[pos] = 0;
          release = &(ptr[pos + 1]);
        }
        break;
      }
    }
    // 3. split segments
    p = ptr;
    for (int i = 0; i < 3; i++) {
      if (p && *p) {
        segments[i] = p;
        p = strchr(p, '.');
        if (p) {
          *p = '\0';
          p++;
        }
      } else {
        segments[i] = ZERO_STRING;
      }
    }
  }

public:
  char* getRelease() const {
    return release;
  }

  const char* getSegment(int index) const {
    return segments[index];
  }

  int compareTo(const Version& that) const {
    return compareTo(that, true);
  }

  int compareTo(const Version& that, bool cs) const {
    int result = 3;

    for (int i = 0; i < 3; i++) {
      const char* thisSegment = getSegment(i);
      const char* thatSegment = that.getSegment(i);
      int res = AlnumCmp::Compare(thisSegment, thatSegment, cs);
      if (res != 0)
        return res > 0 ? result : -result;
      if (result > 1)
        result--;
    }

    char* thisRelease = getRelease();
    char* thatRelease = that.getRelease();

    if (!thisRelease && !thatRelease)
      return 0;
    else if (!thisRelease)
      return 1;
    else if (!thatRelease)
      return -1;

    // compare releases
    Version v1(thisRelease);
    Version v2(thatRelease);
    result = v1.compareTo(v2, false); // the release is case - insensitive

    if (result < 0) {
      return -1;
    } else if (result > 0) {
      return 1;
    }
    return 0;
  }
};


int VersionCmp::Compare(const string& v1, const string& v2, bool cs) {
  if (v1 == v2) {
    return 0;
  }
  // Split v1 and v2 according to http://semver.org/ and compare individually
  Version ver1(v1);
  Version ver2(v2);
  int res = ver1.compareTo(ver2, cs);
  return res;
}

int VersionCmp::RangeCompare(const string& version, const string& versionRange, bool bCompatible)
{
  if (version == versionRange) {
    return 0;
  }

  string verMin = RteUtils::GetPrefix(versionRange);
  string verMax = RteUtils::GetSuffix(versionRange);
  int resMin = 0;
  if (!verMin.empty()) {
    resMin = Compare(version, verMin);
    if (resMin < 0 || verMin == verMax) // lower than min or exact match is required?
      return resMin;
  }
  if (!verMax.empty()) {
    int resMax = Compare(version, verMax);
    if (resMax > 0)
      return resMax;
  }else if(bCompatible && resMin > 2) {
    return resMin; // semantic version: major version change -> incompatible
  }
  return 0;
}

string VersionCmp::RemoveVersionMeta(const string& v) {
  string::size_type pos = v.find('+');
  if (pos != string::npos)
    return v.substr(0, pos);
  return v;
}

std::string VersionCmp::Ceil(const std::string& v, bool bMinus)
{
  int major = RteUtils::StringToInt(RteUtils::GetPrefix(v, '.'));
  if (major < 0) {
    major = 0;
  }
  std::string majorVer = RteUtils::LongToString(major + 1, 10);
  majorVer += ".0.0";
  if (bMinus) {
    majorVer += "-";
  }
  return majorVer;
}

std::string VersionCmp::Floor(const std::string& v)
{
  return RteUtils::GetPrefix(v, '.') + ".0.0";
}


VersionCmp::MatchMode VersionCmp::MatchModeFromString(const std::string& mode)
{
  if (mode == "fixed")
    return MatchMode::FIXED_VERSION;
  else if (mode == "enforced")
    return MatchMode::ENFORCED_VERSION;
  else if (mode == "latest")
    return MatchMode::LATEST_VERSION;
  else if (mode == "excluded")
    return MatchMode::EXCLUDED_VERSION;
  // all other cases
  return MatchMode::LATEST_VERSION;
}

VersionCmp::MatchMode VersionCmp::MatchModeFromVersionString(const std::string& version)
{
  string op, filter;
  VersionCmp::MatchMode mode = MatchMode::LATEST_VERSION;
  filter = RteUtils::GetSuffix(version, '@');
  if (filter.empty())
    return mode;
  auto pos = RteUtils::FindFirstDigit(filter);
  if (pos == std::string::npos)
    return mode;
  op = filter.substr(0, pos);
  if (op.empty())
    mode = MatchMode::FIXED_VERSION;
  else if (op == HIGHER_OR_EQUAL_OPERATOR)
    mode = MatchMode::HIGHER_OR_EQUAL;
  return mode;
}

std::string VersionCmp::MatchModeToString(VersionCmp::MatchMode mode)
{
  string s;
  switch (mode) {
  case MatchMode::FIXED_VERSION:
    s = "fixed";
    break;
  case MatchMode::LATEST_VERSION:
  case MatchMode::HIGHER_OR_EQUAL:
    s = "latest";
    break;
  case MatchMode::ENFORCED_VERSION:
    s = "enforced";
    break;
  case MatchMode::EXCLUDED_VERSION:
    s = "excluded";
    break;
  case MatchMode::ANY_VERSION:
  default:
    break;
  }
  return s;
}

const std::string VersionCmp::GetMatchingVersion(const std::string& filter,
  const std::set<std::string>& availableVersions, bool bCompatible)
{
  string matchedVersion;
  if (std::string::npos == filter.find('@')) {
    // version range
    vector<std::string> matchedVersions;
    for (auto& version : availableVersions) {
      if (0 == RangeCompare(version, filter, bCompatible)) {
        matchedVersions.push_back(version);
      }
    }
    auto itr = std::max_element(matchedVersions.begin(), matchedVersions.end(),
      [](const auto& a, const auto& b) {
        return (VersionCmp::Compare(a, b) < 0);
      });
    if (itr != matchedVersions.end()) {
      matchedVersion = *itr;
    }
  }
  else {
    VersionCmp::MatchMode mode = VersionCmp::MatchModeFromVersionString(filter);
    string filterVersion = RteUtils::RemovePrefixByString(filter, PREFIX_VERSION);
    if (mode == MatchMode::HIGHER_OR_EQUAL) {
      filterVersion = RteUtils::RemovePrefixByString(filterVersion, HIGHER_OR_EQUAL_OPERATOR);
    }
    for (auto& version : availableVersions) {
      int result = VersionCmp::Compare(version, filterVersion, false);
      switch (mode) {
      case MatchMode::FIXED_VERSION:
        if (result == 0)
          return version;
        break;
      case MatchMode::LATEST_VERSION:
      case MatchMode::HIGHER_OR_EQUAL:
        if (result > 0 || result == 0) {
          matchedVersion = version;
          filterVersion = matchedVersion;
        }
        break;
      default: // never happening
        break;
      }
    }
  }
  return matchedVersion;
}

int VersionCmp::ComparatorBase::Compare(const std::string& v1, const std::string& v2, bool cs) const
{
  if (m_delimiter) {
    int res = AlnumCmp::CompareLen(RteUtils::GetPrefix(v1, m_delimiter), RteUtils::GetPrefix(v2, m_delimiter), cs);
    if (res == 0) {
      res = VersionCmp::Compare(RteUtils::GetSuffix(v1, m_delimiter), RteUtils::GetSuffix(v2, m_delimiter), cs);
    }
    return res;
  }
  return VersionCmp::Compare(v1, v2, cs);
}

// End of VersionCmp.cpp
