#ifndef AlnumCmp_H
#define AlnumCmp_H
/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file AlnumCmp.h
* @brief Alpha-numeric comparison
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/
/**
* Class to compare strings containing decimal digits alpha-numerically. That is
* in particular to sort strings containing numbers
* <p/>
* The class can be used:
* <ul>
* <li> as comparator to sort collections (in descending order by default)
* <li> to compare strings directly using <code>alnumCompare()</code> static functions
* </ul>

* <p/>
* Groups of digits are converted into numbers for comparison, other characters
* are compared in standard way. In addition string lengths are used to ensure 2.01 > 2.1
* <p>
* In contrast, standard lexicographical string comparison treats digits as characters, for example:
* <dl>
* <dt>alpha-numeric comparison:</dt>
* <dd>"10.1" > "2.1"</dd>
* <dd>"2.01" > "2.1"</dd>
* <dd>"2.01" == "2.01"</dd>
* <dt>standard lexicographical comparison:</dt>
* <dd>"10.1" < "2.1"</dd>
* <dd>"2.01" < "2.1"</dd>
* <dd>"2.01" == "2.01"</dd>
* </dl>
* </p>
*
*/

#include <string>

// class used to compare alphanumerically
class AlnumCmp
{
private:
  AlnumCmp() {}; // private constructor for utility class

public:
  /**
   * @brief alphanumerically compare two strings, e.g. "10.1" > "2.1"
   * @param str1 string to be compared
   * @param str2 string to be compared
   * @param cs true if comparison is case sensitive
   * @return 0 if strings are equal, > 0 if str1 is greater than str2, otherwise < 0
  */
  static int Compare(const char* str1, const char* str2, bool cs = true);
  /**
   * @brief alphanumerically compare two strings, e.g. "10.1" > "2.1"
   * @param a string to be compared
   * @param b string to be compared
   * @param cs true if comparison is case sensitive
   * @return 0 if strings are equal, > 0 if a is greater than b, otherwise < 0
  */
  static int Compare(const std::string& a, const std::string& b, bool cs = true);
  /**
   * @brief alphanumerically compare two strings with consideration of string length
   * @param a string to be compared
   * @param b string to be compared
   * @param cs true if comparison is case sensitive
   * @return 0 if strings are equal, > 0 if a is greater than b, otherwise < 0
  */
  static int CompareLen(const std::string& a, const std::string& b, bool cs = true); // in case AlnumCmp returns 0, the string lengths are compared

  // helper compare classes s for map container, returns true if a < b

  class Less
  {
  public:
    bool operator()(const std::string& a, const std::string& b) const { return AlnumCmp::Compare(a, b) < 0; }
  };

  class LessNoCase
  {
  public:
    bool operator()(const std::string& a, const std::string& b) const { return AlnumCmp::Compare(a, b, false) < 0; }
  };

  class LenLessNoCase
  {
  public:
    bool operator()(const std::string& a, const std::string& b) const { return AlnumCmp::CompareLen(a, b, false) < 0; }
  };

  // helper compare class for map container, returns true if a > b
  class Greater
  {
  public:
    bool operator()(const std::string& a, const std::string& b) const { return AlnumCmp::Compare(a, b) > 0; }
  };

  class LenGreater
  {
  public:
    bool operator()(const std::string& a, const std::string& b) const { return AlnumCmp::CompareLen(a, b) > 0; }
  };


  class GreaterNoCase
  {
  public:
    bool operator()(const std::string& a, const std::string& b) const { return AlnumCmp::Compare(a, b, false) > 0; }
  };
};

#endif // AlnumCmp_H
