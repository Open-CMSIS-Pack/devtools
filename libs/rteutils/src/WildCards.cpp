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

class WildcardState
{
public:
  const char* m_s;
  std::size_t m_index;
  std::size_t m_length;
  const char* range;
  std::size_t rangeSize;
  bool isStar;
  bool hasStar;

public:
  WildcardState(const std::string& s) :m_s(s.c_str()), m_index(0), isStar(false), hasStar(false) { m_length = s.size(); init(); };

  void init() { isStar = false; m_index = 0; createRange(); }
  inline bool isEnd() const { return m_index >= m_length; }
  bool isQuestion() const { return range && (*range == '?' || *range == '*'); }

  void next() {
    if (isEnd())
      return;
    m_index++;
    createRange();
  };

  void skip(const WildcardState& ws)
  {
    while (!isEnd() && !compare(ws))
    {
      next();
    }
  }

  bool compare(const WildcardState& ws) const
  {
    if (isQuestion() || ws.isQuestion())
      return true;
    if (!rangeSize && !ws.rangeSize)
      return true;

    for (unsigned i = 0; i < rangeSize; i++) {
      for (unsigned j = 0; j < ws.rangeSize; j++) {
        if (range[i] == ws.range[j])
          return true;
      }
    }

    return false;
  }

private:
  void createRange()
  {
    range = NULL;
    rangeSize = 0;
    isStar = false;
    if (isEnd())
      return;
    char ch = m_s[m_index];
    if (m_s[m_index] == '*') {
      hasStar = isStar = true;
      // skip all stars
      while ((ch == '*' || ch == '?') && m_index < m_length) {
        m_index++;
        ch = m_s[m_index]; // when m_index == m_length ch is '\0'
      }
    }
    if (ch == '[')
    {
      m_index++;
      range = &m_s[m_index];
      while (!isEnd()) {
        ch = m_s[m_index];
        if (ch == ']') {
          break;
        }
        rangeSize++;
        m_index++;
      }
    } else {
      range = &m_s[m_index];
      rangeSize = 1;
    }
  }
public:
  static bool WildCardMatch(WildcardState& ws1, WildcardState& ws2)
  {
    while (true)
    {
      if (ws1.isStar) {
        if (ws1.isEnd())
          return true; // end of s2 is irrelevant
        ws2.skip(ws1);
        if (ws2.isEnd()) {
          return ws2.isStar || ws2.isQuestion();
        }
      }

      if (ws2.isStar) {
        if (ws2.isEnd())
          return true; // end of s1 is irrelevant
        ws1.skip(ws2);
        if (ws1.isEnd()) {
          return ws1.isStar || ws1.isQuestion();
        }
      }

      if (ws1.isEnd() || ws2.isEnd())
        break;

      if (!ws1.compare(ws2)) {
        return false;
      }
      ws1.next();
      ws2.next();
    }
    return ws1.isEnd() && ws2.isEnd();
  }
};


bool WildCards::Match(const std::string& s1, const std::string& s2)
{
  if (s1 == s2)
    return true;

  if (s1.empty() && s2.empty())
    return true;
  if (s1.empty() || s2.empty())
    return false;

  WildcardState ws1(s1);
  if (ws1.isStar && ws1.isEnd())
    return true;
  WildcardState ws2(s2);
  if (ws2.isStar && ws2.isEnd())
    return true;
  bool result = WildcardState::WildCardMatch(ws1, ws2);

  // we need a simmetric comparison
  if (!result && ws1.hasStar && ws2.hasStar) {
    // in case when both strings have '*', we need to compare twice: a*d and a*cd should be treated as equal
    ws1.init();
    ws2.init();
    if (WildcardState::WildCardMatch(ws2, ws1))
      return true;
  }
  return result;
}

std::string WildCards::ToRegEx(const std::string& s)
{
  static const std::string from[2] = { "?", "*" };
  static const std::string to[2] = { ".", ".*" };
  std::string str = s;
  for (int i = 0; i < 2; i++) {
    size_t start_pos = 0;
    while ((start_pos = str.find(from[i], start_pos)) != std::string::npos) {
      str.replace(start_pos, from[i].length(), to[i]);
      start_pos += to[i].length();
    }
  }
  return str;
}

std::string WildCards::ToX(const std::string& s)
{
  std::string res = s;
  for (std::string::size_type pos = 0; pos < res.length(); pos++) {
    char ch = res[pos];
    if (isalnum(ch) || ch == '-' || ch == '.') // allowed characters
      continue;
    else if (ch == '*' || ch == '?') // wildcards
      res[pos] = 'x';
    else // any other character
      res[pos] = '_';
  }
  return res;
}


// End of WildCards.cpp
