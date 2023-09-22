#ifndef VersionCmp_H
#define VersionCmp_H
/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file VersionCmp.h
* @brief Semantic Version comparison according to http://semver.org/
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include <string>
#include <set>

class VersionCmp
{

private:
  VersionCmp() {}; // protection

public:

  enum MatchMode {
    ENFORCED_VERSION, // strictly fixed version is required (pack and condition)
    FIXED_VERSION,    // fixed version is accepted
    LATEST_VERSION,   // use the latest version (default)
    ANY_VERSION,      // any version satisfies
    EXCLUDED_VERSION, // exclude specified version
    HIGHER_OR_EQUAL   // higher or equal version
  };

  static constexpr const char* PREFIX_VERSION = "@";
  static constexpr const char* HIGHER_OR_EQUAL_OPERATOR = ">=";

public:
  /**
   * @brief Split v1 and v2 according to http://semver.org/ and compare individually
   * @param v1 version to be compared
   * @param v2 version to be compared
   * @param cs true in case of case sensitive comparison
   * @return 0 if both versions are equal, > 0 if v1 is greater than v2, < 0 if v2 is greater than v1
  */
  static int Compare(const std::string& v1, const std::string& v2, bool cs = true);
  /**
   * @brief compare version with range versions in the form major.minor.release:major.minor.release
   * @param version version to be compared
   * @param versionRange range version to be compared
   * @param bCompatible if upper rang is not given, limit upper range by next major version
   * @return 0 if version is between range version, > 0 if greater, < 0 if smaller
  */
  static int RangeCompare(const std::string& version, const std::string& versionRange, bool bCompatible = false);

  /**
   * @brief equivalent to RangeCompare(version, versionRange, true)
   * @param version version to be compared
   * @param versionRange range version to be compared
   * @return 0 if version is between range version, > 0 if greater, < 0 if smaller
  */
  static int CompatibleRangeCompare(const std::string& version, const std::string& versionRange) {
    return RangeCompare(version, versionRange, true);
  }

  /**
   * @brief remove string after plus sign
   * @param v version string
   * @return string ahead of plus sign
  */
  static std::string RemoveVersionMeta(const std::string& v);

  /**
   * @brief calculates next major version
   * @param v version string
   * @param bMinus append minus sign as it can be used in range comparison
   * @return string rounded to upper major version
  */
  static std::string Ceil(const std::string& v, bool bMinus = true);

  /**
   * @brief calculates previous major version
   * @param v version string
   * @return string rounded to upper major version
  */
  static std::string Floor(const std::string& v);


  /**
   * @brief compare string to return mode constant
   * @param mode mode specification: "fixed" | "latest" | "excluded"
   * @return mode constant
  */
  static MatchMode MatchModeFromString(const std::string& mode);
  /**
   * @brief compare version string to return mode constant
   * @param version filter version string e.g: @1.2.3, @>=1.2.3
   * @return mode constant
  */
  static MatchMode MatchModeFromVersionString(const std::string& version);
  /**
   * @brief compare mode with constant to return string
   * @param mode mode constant
   * @return mode string: "fixed" | "latest" | "excluded"
  */
  static std::string MatchModeToString(VersionCmp::MatchMode mode);
  /**
   * @brief get matched version from available versions
   * @param filter version filter or version range e.g: @>=1.2.3, 1.2.3:1.4.0
   * @param availableVersions list of available versions
   * @param bCompatible if upper rang is not given, limit upper range by next major version in
   * @return matching version string, based on filter
   *      also, in case of multiple matches, latest matching version
   *      has precedence over other versions
  */
  static const std::string GetMatchingVersion(const std::string& filter,
    const std::set<std::string>& availableVersions, bool bCompatible = false);

  class ComparatorBase
  {
  public:
    ComparatorBase(char delimiter = 0) : m_delimiter(delimiter) {}
    int Compare(const std::string& v1, const std::string& v2, bool cs = true) const;
  protected:
    char m_delimiter;
  };

  // helper compare class for map containers (version compare operators)
  class Less : public ComparatorBase
  {
  public:
    Less(char delimiter = 0) : ComparatorBase(delimiter) {}
    bool operator()(const std::string& a, const std::string& b) const
    {
      return Compare(a, b) < 0;
    }
  };

  class LessNoCase : public ComparatorBase
  {
  public:
    LessNoCase(char delimiter = 0) : ComparatorBase(delimiter) {}
    bool operator()(const std::string& a, const std::string& b) const
    {
      return Compare(a, b, false) < 0;
    }
  };

  class Greater : public ComparatorBase
  {
  public:
    Greater(char delimiter = 0) : ComparatorBase(delimiter) {}
    bool operator()(const std::string& a, const std::string& b) const
    {
      return Compare(a, b) > 0;
    }
  };

  class GreaterNoCase : public ComparatorBase
  {
  public:
    GreaterNoCase(char delimiter = 0) : ComparatorBase(delimiter) {}
  public:
    bool operator()(const std::string& a, const std::string& b) const
    {
      return Compare(a, b, false) > 0;
    }
  };
};

#endif // VersionCmp_H
