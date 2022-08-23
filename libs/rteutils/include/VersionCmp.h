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

  enum class MatchMode {
    ANY_VERSION,      // any installed version is accepted
    FIXED_VERSION,    // fixed version is accepted
    LATEST_VERSION,   // use the latest version
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
   * @return 0 if version is between range version, > 0 if greater, < 0 if smaller
  */
  static int RangeCompare(const std::string& version, const std::string& versionRange);
  /**
   * @brief remove string after plus sign
   * @param v version string
   * @return string ahead of plus sign
  */
  static std::string RemoveVersionMeta(const std::string& v);
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
   * @return matching version string, based on filter
   *      also, in case of multiple matches, latest matching version
   *      has precedence over other versions
  */
  static const std::string GetMatchingVersion(
    const std::string& filter,
    const std::set<std::string> availableVersions);

  // helper compare class for map containers (version compare operators)
  class Less
  {
  public:
    bool operator()(const std::string& a, const std::string& b) const
    {
      return VersionCmp::Compare(a, b) < 0;
    }
  };

  class LessNoCase
  {
  public:
    bool operator()(const std::string& a, const std::string& b) const
    {
      return VersionCmp::Compare(a, b, false) < 0;
    }
  };

  class Greater
  {
  public:
    bool operator()(const std::string& a, const std::string& b) const
    {
      return VersionCmp::Compare(a, b) > 0;
    }
  };

  class GreaterNoCase
  {
  public:
    bool operator()(const std::string& a, const std::string& b) const
    {
      return VersionCmp::Compare(a, b, false) > 0;
    }
  };
};

#endif // VersionCmp_H
