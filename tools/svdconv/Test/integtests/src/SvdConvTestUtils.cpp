/*
 * Copyright (c) 2010-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "SvdConvTestUtils.h"

#include "gtest/gtest.h"

using namespace std;
using namespace testing;



SvdConvTestUtils::SvdConvTestUtils()
{
}

SvdConvTestUtils::~SvdConvTestUtils()
{
}

list<smatch> SvdConvTestUtils::FindRegex(const string& buf, const regex& pattern)
{
  list<smatch> result;
  smatch match;
  for(auto first = buf.cbegin();
      regex_search(first, buf.cend(), match, pattern);
      first = match.suffix().first)
  {
    result.push_back(match);
  }
  return result;
}

bool SvdConvTestUtils::FindEntry(const list<smatch>& result, const string& entry)
{
  if(!result.size()) {
    ADD_FAILURE() << "Entry not found: match results empty!";
    return false;
  }

  for(auto res : result) {
    if(res.size() < 2) {
      continue;
    }

    if(!res[1].str().compare(entry)) {
      cout << "All entries found during RegEx Search" << endl;
      return true;
    }
  }

  ADD_FAILURE() << "Entry not found: " << entry << endl;

  return false;
}

bool SvdConvTestUtils::FindAllEntries(const list<smatch>& result, const list<string>& entries)
{
  if(!result.size()) {
    ADD_FAILURE() << "Entry not found: match results empty!";
    return false;
  }

  for(auto entry : entries) {
    if(!FindEntry(result, entry)) {
      ADD_FAILURE() << "Entry not found: " << entry;
      return false;
    }
  }

  cout << "All entries found during RegEx Search" << endl;

  return true;
}

