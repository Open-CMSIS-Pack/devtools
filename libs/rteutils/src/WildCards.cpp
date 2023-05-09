/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file WildCards.cpp
* @brief Simplified symmetric wild card matching
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "WildCards.h"
#include <regex>


bool WildCards::Match(const std::string& s1, const std::string& s2)
{
  // first compare strings as is to save time by regular expression match
  if (s1 == s2) {
    return true;
  }

  if (s1.empty() || s2.empty()) {
    return false;
  }
  // check if s1 is a pattern and match against s2
  if (IsWildcardPattern(s1) && MatchToPattern(s2, s1)) {
    return true;
  }

  // swap the arguments if s2 is a pattern
  if (IsWildcardPattern(s2) && MatchToPattern(s1, s2)) {
    return true;
  }
  return false;
}

std::string WildCards::ToRegEx(const std::string& s)
{
  // Char translations:
  // 1) escape '.', '{', '}', '(', ')', '$', and '+' if any
  // 2) replace '?' with '.'
  // 3) replace '*' with ".*"
  static const std::string from[] = {".", "$", "+", "{", "}", "(", ")", "?", "*" };
  static const std::string to[] = { "\\.", "\\$", "\\+", "\\{", "\\}", "\\(", "\\)", ".", ".*" };
  std::string str = s;
  for (int i = 0; i < 9; i++) {
    size_t start_pos = 0;
    while ((start_pos = str.find(from[i], start_pos)) != std::string::npos) {
      str.replace(start_pos, from[i].length(), to[i]);
      start_pos += to[i].length();
    }
  }
  return str;
}

std::string WildCards::ToX(const std::string& s, bool keepMinusAndDot)
{
  std::string res = s;
  for (std::string::size_type pos = 0; pos < res.length(); pos++) {
    char ch = res[pos];
    if (isalnum(ch) || (keepMinusAndDot && (ch == '-' || ch == '.'))) // allowed characters
      continue;
    else if (ch == '*' || ch == '?') // wildcards
      res[pos] = 'x';
    else // any other character
      res[pos] = '_';
  }
  return res;
}

bool WildCards::IsWildcardPattern(const std::string& s)
{
  return s.find_first_of("?*[]") != std::string::npos;
}


bool WildCards::MatchToPattern(const std::string& s, const std::string& pattern)
{
  try {
    std::regex e(ToRegEx(pattern));
    return std::regex_match(s, e);
  } catch (const std::regex_error&) {
    // fall through, return false if regex has an error
  }
  return false;
}

// End of WildCards.cpp
