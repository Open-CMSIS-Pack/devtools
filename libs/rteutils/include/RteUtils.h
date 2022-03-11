#ifndef RteUtils_H
#define RteUtils_H
/******************************************************************************/
/* RTE  -  CMSIS Run-Time Environment				                          */
/******************************************************************************/
/** @file  RteUtils.h
  * @brief CMSIS RTE Data Model
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "AlnumCmp.h"
#include "DeviceVendor.h"
#include "VersionCmp.h"
#include "WildCards.h"

#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>


class RteUtils
{
private:
  RteUtils() {}; // private constructor to forbid instantiation of a utility class

public:
  /**
   * @brief determine Pack ID specific to Pack Manager
   * @param path must have the following convention:
   *        [path][\\]<vendor>.<name>.<major version>.<minor version 1>.<minor version 2>.(pack|pdsc)
   * @return Pack ID specific to Pack Manager
  */
  static std::string GetPackID(const std::string &path);
  /**
   * @brief determine prefix of a string
   * @param s string containing the prefix
   * @param delimiter delimiter after the prefix
   * @param withDelimiter true if returned string should contain delimiter, otherwise false
   * @return return prefix of a string with or without delimiter
  */
  static std::string GetPrefix(const std::string& s, char delimiter = ':', bool withDelimiter = false);
  /**
   * @brief determine suffix of a string
   * @param s string containing the suffix
   * @param delimiter delimiter ahead of the suffix
   * @param withDelimiter true if returned string should contain delimiter, otherwise false
   * @return return suffix of a string with or without delimiter
  */
  static std::string GetSuffix(const std::string& s, char delimiter = ':', bool withDelimiter = false);
  /**
   * @brief determine suffix as integer
   * @param s string containing the suffix
   * @param delimiter delimiter ahead of the suffix
   * @return return suffix as integer
  */
  static int    GetSuffixAsInt(const std::string& s, char delimiter = ':');
  /**
   * @brief determine string after a delimiter
   * @param s string containing delimiter
   * @param delimiter delimiter to look for
   * @return return string after delimiter
  */
  static std::string RemovePrefixByString(const std::string& s, const char* delimiter = ":");
  /**
   * @brief determine string ahead of a delimiter
   * @param s string containing delimiter
   * @param delimiter delimiter to look for
   * @return return string ahead of delimiter
  */
  static std::string RemoveSuffixByString(const std::string& s, const char* delimiter = ":");
  /**
   * @brief determine number of delimiters found in the string
   * @param s string containing delimiter(s)
   * @param delimiter string delimiter
   * @return number of delimiters found in the string
  */
  static int    CountDelimiters(const std::string& s, const char* delimiter = ":");
  /**
   * @brief split string into substrings separated by delimiter
   * @param segments list of substrings to fill
   * @param s string to be split
   * @param delimiter character delimiter
   * @return number of resulting segments
  */
  static int    SplitString(std::list<std::string>& segments, const std::string& s, const char delimiter);
  /**
   * @brief checks if two std::string objects are case insensitive equal
   * @param a string to be compared
   * @param b string to be compared
   * @return true if equal
  */
  static bool EqualNoCase(const std::string& a, const std::string& b);

  /**
   * @brief extract vendor name from package ID
   * @param packageId package ID
   * @return vendor name
  */
  static std::string VendorFromPackageId(const std::string &packageId);
  /**
   * @brief determine package name from package ID
   * @param packageId package ID
   * @return package name from package ID
  */
  static std::string NameFromPackageId(const std::string &packageId);

  /**
   * @brief remove trailing backslash(s)
   * @param s string to be manipulated
   * @return string without trailing backslash(s)
  */
  static std::string RemoveTrailingBackslash(const std::string& s);
  /**
   * @brief determine string between two quotes
   * @param s string to be looked for
   * @return string between two quotes
  */
  static std::string RemoveQuotes(const std::string& s);
  /**
   * @brief check if name (e.g. Dname) is CMSIS-conformed
   * @param s name to be checked
   * @return true if name is CMSIS-conformed
  */
  static bool CheckCMSISName(const std::string& s);
  /**
   * @brief replace blank(s) with underscore(s)
   * @param s string to be manipulated
   * @return string with blank(s) replaced with underscore(s)
  */
  static std::string SpacesToUnderscore(const std::string& s);
  /**
   * @brief convert slashes depending on OS
   * @param s string to be manipulated
   * @return string with slashes specific to OS
  */
  static std::string SlashesToOsSlashes(const std::string& s);
  /**
   * @brief convert slashes to backslashes
   * @param fileName string to be manipulated
   * @return string with slashes converted
  */
  static std::string SlashesToBackSlashes(const std::string& fileName);
  /**
   * @brief convert backslashes to slashes
   * @param fileName string to be manipulated
   * @return string with backslashes converted
  */
  static std::string BackSlashesToSlashes(const std::string& fileName);
  /**
   * @brief complete line endings with CRLF
   * @param s string to be manipulated
   * @return string with CRLF line endings
  */
  static std::string EnsureCrLf(const std::string& s);
  /**
   * @brief replace %Instance% with number starting from 0
   * @param s string to be manipulated
   * @param count number of iterations
   * @return expanded string separated by '\n' for each iteration
  */
  static std::string ExpandInstancePlaceholders(const std::string& s, int count);
  /**
   * @brief extract file name from fully qualified file name
   * @param fileName string containing path and file name
   * @return file name without folder specification
  */
  static std::string ExtractFileName(const std::string& fileName);
  /**
   * @brief extract path from fully qualified file name
   * @param fileName string containing path and file name
   * @param withTrailingSlash true if trailing slash should be considered for the returned string
   * @return path without file name
  */
  static std::string ExtractFilePath(const std::string& fileName, bool withTrailingSlash);
  /**
   * @brief extract file name from file name containing file extension
   * @param fileName file name with file extension
   * @return file name without file extension
  */
  static std::string ExtractFileBaseName(const std::string& fileName);
  /**
   * @brief extract file extension from file name
   * @param fileName file name with file extension
   * @param withDot true to return dot with extension
   * @return file extension
  */
  static std::string ExtractFileExtension(const std::string& fileName, bool withDot = false);

  /**
   * @brief construct filename with appended version string
   * @param fileName path to config file to use, can be absolute or relative
   * @param version string to append
   * @param bHidden insert dot in front of base name
   * @return constructed filename in the format: path/name.ext@version or path/.name.ext@version
  */
  static std::string AppendFileVersion(const std::string& fileName, const std::string& version, bool bHidden);

  /**
   * @brief extract first file segments
   * @param fileName fully qualified file name
   * @param nSegments number of segments separated by forward slash
   * @return first segments of fully qualified file name
  */
  static std::string ExtractFirstFileSegments(const std::string& fileName, int nSegments);

  /**
   * @brief extract last file segments
   * @param fileName fully qualified file name
   * @param nSegments number of segments separated by forward slash
   * @return last segments of fully qualified file name
  */
  static std::string ExtractLastFileSegments(const std::string& fileName, int nSegments);

  /**
   * @brief determine number of file segments separated by forward slash
   * @param fileName fully qualified file name
   * @return number of file segments separated by forward slash
  */
  static int    GetFileSegmentCount(const std::string& fileName);
  /**
   * @brief backward compare file segments separated by slash
   * @param f1 fully qualified file name to be compared
   * @param f2 fully qualified file name to be compared
   * @return number of file segments matched
  */
  static int    SegmentedPathCompare(const char* f1, const char* f2); // returns number of matching segments counting backwards
  /**
   * @brief check if string has hexadecimal prefix '0x' or '0X'
   * @param s string to be checked
   * @return true if string has hexadecimal prefix
  */
  static bool HasHexPrefix(const std::string& s);
  /**
   * @brief convert string to a value of type unsigned long
   * @param s string to be converted
   * @return unsigned long value
  */
  static unsigned long ToUL(const std::string& s);
  /**
   * @brief convert string to a value of type unsigned long long
   * @param s string to be converted
   * @return unsigned long long value
  */
  static unsigned long long ToULL(const std::string& s);
  /**
   * @brief fill a buffer with blanks
   * @param indent number of blanks in the buffer to be returned
   * @return a buffer filled with blanks
  */
  static const char* GetIndent(unsigned indent);
  /**
   * @brief convert pairs of string into XML attribute specification
   * @param attributes a std::map<std::string, std::string> representing pairs if attribute name and attribute value
   * @return XML specification of attributes
  */
  static std::string ToXmlString(const std::map<std::string, std::string>& attributes);

  static const std::string EMPTY_STRING;
  static const std::string ERROR_STRING;
  static const char CatalogName[];
  static const std::set<std::string> EMPTY_STRING_SET;
  static const std::list<std::string> EMPTY_STRING_LIST;
  static const std::vector<std::string> EMPTY_STRING_VECTOR;
};

#endif // RteUtils_H
