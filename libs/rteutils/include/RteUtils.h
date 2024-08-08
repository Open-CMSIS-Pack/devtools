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
 * Copyright (c) 2020-2024 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "AlnumCmp.h"
#include "CollectionUtils.h"
#include "DeviceVendor.h"
#include "VersionCmp.h"
#include "WildCards.h"

class RteUtils
{
private:
  RteUtils() {}; // private constructor to forbid instantiation of a utility class

public:
  /**
   * @brief determine prefix of a string
   * @param s string containing the prefix
   * @param delimiter char delimiter after the prefix
   * @param withDelimiter true if returned string should contain delimiter, otherwise false
   * @return return prefix of a string with or without delimiter
  */
  static std::string GetPrefix(const std::string& s, char delimiter = ':', bool withDelimiter = false);
  /**
   * @brief determine suffix of a string
   * @param s string containing the suffix
   * @param delimiter char delimiter before suffix
   * @param withDelimiter true if returned string should contain delimiter, otherwise false
   * @return return suffix of a string with or without delimiter
  */
  static std::string GetSuffix(const std::string& s, char delimiter = ':', bool withDelimiter = false);
  /**
   * @brief determine suffix as integer
   * @param s string containing the suffix
   * @param delimiter char delimiter before suffix
   * @return return suffix as integer
  */
  static int    GetSuffixAsInt(const std::string& s, char delimiter = ':');
  /**
   * @brief determine string after a delimiter
   * @param s string containing delimiter
   * @param delimiter string delimiter to look for
   * @return return string after delimiter
  */
  static std::string RemovePrefixByString(const std::string& s, const char* delimiter = ":");
  /**
   * @brief determine string ahead of a delimiter
   * @param s string containing delimiter
   * @param delimiter string delimiter to look for
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
   * @brief split string into a set of unique substrings
   * @param args string to split
   * @param delimiter string with one or more delimiters separating substrings
   * @return set of substrings strings
  */
  static std::set<std::string> SplitStringToSet(const std::string& args, const std::string& delimiter = RteUtils::SPACE_STRING);

  /**
   * @brief checks if two std::string objects are case insensitive equal
   * @param a string to be compared
   * @param b string to be compared
   * @return true if equal
  */
  static bool EqualNoCase(const std::string& a, const std::string& b);

  /**
   * @brief construct an ID string from a vector of key-value pairs
   * @param elements vector with key-value pairs
   * @return constructed ID
  */
  static std::string ConstructID(const std::vector<std::pair<const char*, const std::string&>>& elements);

  /**
   * @brief remove trailing backslash(s)
   * @param s string to be manipulated
   * @return string without trailing backslash(s)
  */
  static std::string RemoveTrailingBackslash(const std::string& s);
  /**
   * @brief determine string between two quotes
   * @param s string to be processed for
   * @return string between two quotes
  */
  static std::string RemoveQuotes(const std::string& s);
  /**
   * @brief adds quotes to a string if it contains spaces and not yet quoted
   * @param s string to be processed
   * @return string between two quotes
  */
  static std::string AddQuotesIfSpace(const std::string& s);

  /**
   * @brief check if name (e.g. Dname) is CMSIS-conformed
   * @param s name to be checked
   * @return true if name is CMSIS-conformed
  */
  static bool CheckCMSISName(const std::string& s);

  /**
   * @brief replace all substring occurrences in the supplied string
   * @param str reference to string to be manipulated
   * @param toReplace substring to replace
   * @param with substitute string
   * @return reference supplied string
   */
  static std::string& ReplaceAll(std::string& str, const std::string& toReplace, const std::string& with);

  /**
   * @brief expand string by replacing $keyword$ with corresponding values
   * @param src source string to expand
   * @param variables string to string map with keyword values
   * @return expanded string
  */
  static std::string ExpandAccessSequences(const std::string& src, const StrMap& variables);

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
   * @brief complete line endings with CRLF ("\r\n")
   * @param s string to be manipulated
   * @return string with CRLF line endings
  */
  static std::string EnsureCrLf(const std::string& s);

  /**
   * @brief complete line endings LF ('n')
   * @param s string to be manipulated
   * @return string with CRLF line endings
  */
  static std::string EnsureLf(const std::string& s);

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
   * @param versionPrefix string to insert before version such as "base" or"update"
   * @return constructed filename in the format: path/name.ext.versionPrefix@version
  */
  static std::string AppendFileVersion(const std::string& fileName, const std::string& version, const std::string& versionPrefix);

  /**
   * @brief construct filename with appended version string and "base" prefix
   * @param fileName path to config file to use, can be absolute or relative
   * @param version string to append
   * @return constructed filename in the format: path/name.ext.base@version
  */
  static std::string AppendFileBaseVersion(const std::string& fileName, const std::string& version);

  /**
   * @brief construct filename with appended version string and "update" prefix
   * @param fileName path to config file to use, can be absolute or relative
   * @param version string to append
   * @return constructed filename in the format: path/name.ext.update@version
  */
  static std::string AppendFileUpdateVersion(const std::string& fileName, const std::string& version);

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
   * @brief determine first occurrence of a digit in a string
   * @param s string containing the numerals
   * @return index of first digit found, else string::npos
  */
  static std::string::size_type FindFirstDigit(const std::string& s);
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
   * @brief convert string to boolean
   * @param value given string
   * @param defaultValue value to be returned if given string is empty
   * @return true if given string is equal to "1" or "true"
  */
  static bool StringToBool(const std::string& value, bool defaultValue = false);

  /**
   * @brief convert string to integer
   * @param value given string
   * @param defaultValue value to be returned in case of empty string or conversion error
   * @return converted integer value
  */
  static int StringToInt(const std::string& value, int defaultValue = -1);

  /**
   * @brief convert string to unsigned integer
   * @param value string to be converted
   * @param defaultValue value to be returned in case of empty string or conversion error
   * @return converted unsigned integer value
  */
  static unsigned StringToUnsigned(const std::string& value, unsigned defaultValue = 0);

  /**
   * @brief convert string to unsigned long long
   * @param value decimal or hexadecimal string as to be converted
   * @param defaultValue value to be returned in case of empty string or conversion error
   * @return converted value of type unsigned long long
  */
  static unsigned long long StringToULL(const std::string& value, unsigned long long defaultValue = 0L);

  /**
   * @brief converts long value to string
   * @param value long value to convert
   * @param radix conversion radix, default 10
   * @return string representation of supplied value, prefixed with 0x if radix is 16
  */
  static std::string LongToString(long value, int radix = 10);

  /**
   * @brief trim whitespace characters
   * @param s string to be trimmed
   * @return trimmed string without whitespace characters
  */
  static std::string Trim(const std::string& s);

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

  /**
   * @brief get next access sequence text and its position in supplier string
   * @param offset (in/out) to start search for first delimiter, on output position after end delimiter
   * @param src (in) source string to search in
   * @param sequence (out) found sequence string
   * @param start leading delimiter
   * @param end trailing delimiter
   * @return false if delimiters do not match, true otherwise
  */
  static bool GetAccessSequence(size_t& offset, const std::string& src, std::string& sequence, const char start, const char end);

  /**
   * @brief selectively copy strings from source vector to a destination vector
   * @param origin source vector
   * @param filter set of substrings to match (all must match)
   * @param result destination vector
  */
  static void ApplyFilter(const std::vector<std::string>& origin, const std::set<std::string>& filter, std::vector<std::string>& result);

  /**
   * @brief Remove leading whitespace characters after newline
   * @param s string to be trimmed
   * @return trimmed string without whitespace characters after newline character
  */
  static std::string RemoveLeadingSpaces(const std::string& input);
// static constants
public:
  static const std::string EMPTY_STRING;
  static const std::string SPACE_STRING;
  static const std::string DASH_STRING;
  static const std::string CRLF_STRING;
  static const std::string CR_STRING;
  static const std::string LF_STRING;

  static const std::string ERROR_STRING;
  static const std::string BASE_STRING;
  static const std::string UPDATE_STRING;

  static const char CatalogName[];
  static const std::set<std::string> EMPTY_STRING_SET;
  static const std::list<std::string> EMPTY_STRING_LIST;
  static const std::vector<std::string> EMPTY_STRING_VECTOR;
};

#endif // RteUtils_H
