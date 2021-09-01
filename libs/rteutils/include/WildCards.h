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

Note that the processing of '*' differs from standard implementations (e.g. in Visual Studio):
patterns "MK50DX*x7" and "MK50DX*???7" will NOT find string "MK50DX256xxx7"
instead "MK50DX*xxx7" or "MK50DX*x??7" pattern should be used

That is done to ensure symmetry in comparison two strings with wild cards (s1 == s2) == (s2 == s1)
*/

#include <string>

class WildCards
{

private:
  WildCards() {}; // private constructor for utility class

public:
  /**
   * @brief compare two strings containing wild card characters
   * @param s1 string to be compared
   * @param s2 string to be compared
   * @return true if strings matched, otherwise false
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
   * @return converted string
  */
  static std::string ToX(const std::string& s);
};

#endif // WildCards_H
