#ifndef RteConstants_H
#define RteConstants_H
/******************************************************************************/
/* RTE  -  CMSIS Run-Time Environment				                          */
/******************************************************************************/
/** @file  RteConstants.h
  * @brief CMSIS RTE Data Model
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/
#include <string>
#include <list>
#include <vector>
#include <set>

class RteConstants
{
private:
  RteConstants() {}; // private constructor to forbid instantiation of a utility class

public:
  // common constants
  static constexpr const char* CR   = "\r";
  static constexpr const char* LF   = "\n";
  static constexpr const char* CRLF = "\r\n";

  /**
 * @brief component and pack delimiters
*/

  static constexpr const char* AND_STR  = "&";
  static constexpr const char  AND_CHAR = '&';
  static constexpr const char* AT_STR   = "@";
  static constexpr const char  AT_CHAR  = '@';
  static constexpr const char* COLON_STR  = ":";
  static constexpr const char  COLON_CHAR = ':';

  static constexpr const char* COMPONENT_DELIMITERS = ":&@";

  static constexpr const char* SUFFIX_CVENDOR = "::";

  static constexpr const char* PREFIX_CBUNDLE = "&";
  static constexpr const char  PREFIX_CBUNDLE_CHAR = *PREFIX_CBUNDLE;
  static constexpr const char* PREFIX_CGROUP = ":";
  static constexpr const char* PREFIX_CSUB = ":";
  static constexpr const char* PREFIX_CVARIANT = "&";
  static constexpr const char  PREFIX_CVARIANT_CHAR = *PREFIX_CVARIANT;
  static constexpr const char* PREFIX_CVERSION = "@";
  static constexpr const char  PREFIX_CVERSION_CHAR = *PREFIX_CVERSION;
  static constexpr const char* SUFFIX_PACK_VENDOR = "::";
  static constexpr const char* PREFIX_PACK_VERSION = "@";

};

#endif // RteConstants_H
