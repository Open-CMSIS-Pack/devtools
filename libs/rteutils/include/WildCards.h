#ifndef WildCards_H
#define WildCards_H
/******************************************************************************/
/* RTE  -  CMSIS Run-Time Environment				                          */
/******************************************************************************/
/** @file  WildCards.h
  * @brief Simplified symmetric wild card matching
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

/*
Wild card comparison
====================
The following control characters are supported:
*  - zero or more of any character
?  - any single character
[] - any character in the set

Note that the comparison is symmetrical, both supplied strings can contain wild cards
*/

#include <string>

class WildCards
{

private:
  WildCards() {}; // private constructor for utility class

public:
  /**
   * @brief match two strings containing wild card characters
   * @param s1 string to be matched, can be a wild card pattern
   * @param s2 string to be matched, can be a wild card pattern
   * @return true if strings matching, otherwise false
  */
  static bool Match(const std::string& s1, const std::string& s2);

  /**
   * @brief converts "*" to ".*" and "?" to "."
   * @param s string to be converted
   * @return converted string
  */
  static std::string ToRegEx(const std::string& s);
  /**
   * @brief converts '*' and '?' to 'x' All other non-alnum chars to '_', '.' remains unchanged
   * @param s string to be converted
   * @param keepMinusAndDot flag to keep '-' and'.' chars unchanged (default is true)
   * @return converted string
  */
  static std::string ToX(const std::string& s, bool keepMinusAndDot = true);

  /**
   * @brief checks if the supplied string contains any of wild-card matching symbols:
   * @param s string to be checked
   * @return true if the supplied string contains any of  '*', '?', '[', ']' characters
  */
  static bool IsWildcardPattern(const std::string& s);

  /**
   * @brief matches supplied string against a wild card pattern
   * @param s string to be matched, wild cards are considered as normal characters
   * @param pattern wild card expression to match against
   * @return true if match is successful
  */
  static bool MatchToPattern(const std::string& s, const std::string& pattern);
};

#endif // WildCards_H
