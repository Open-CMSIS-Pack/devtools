/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file AlnumCmp.cpp
* @brief Alpha-numeric comparison
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "AlnumCmp.h"


int AlnumCmp::Compare(const char* str1, const char* str2, bool cs)
{
  if (str1 == str2)
    return 0; // constant case
  if (!str1)
    return (!str2) ? 0 : -1;
  else if (!str2)
    return 1;

  while (*str1 && *str2)
  {
    if (isdigit((unsigned char)*str1) && isdigit((unsigned char)*str2))
    {
      int n1 = atoi(str1);
      int n2 = atoi(str2);
      if (n1 > n2)
        return 1;
      else if (n1 < n2)
        return -1;

      // skip digits
      while (*str1 && isdigit((unsigned char)*str1))
        str1++;
      while (*str2 && isdigit((unsigned char)*str2))
        str2++;
    } else
    {
      char c1 = *str1;
      char c2 = *str2;
      if (!cs)
      {
        c1 = (char)toupper(c1);
        c2 = (char)toupper(c2);
      }

      if (c1 > c2)
        return 1;
      else if (c1 < c2)
        return -1;
      str1++;
      str2++;
    }
  }
  return (!*str1 && !*str2) ? 0 : *str1 ? 1 : -1;
}

int AlnumCmp::Compare(const std::string& a, const std::string& b, bool cs)
{
  if (a.empty())
    return (b.empty()) ? 0 : -1;
  else if (b.empty())
    return 1;
  return Compare(a.c_str(), b.c_str(), cs);
}

int AlnumCmp::CompareLen(const std::string& a, const std::string& b, bool cs) {
  int res = AlnumCmp::Compare(a, b, cs);
  if (res == 0) {
    // in case AlnumCmp returns 0, the string lengths are compared to ensure ATSAM3N00B != ATSAM3N0B
    res = (int)(a.size() - b.size());
  }
  return res;
}
// End of AlnumCmp.cpp
