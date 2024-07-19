/*
 * Copyright (c) 2020-2024 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "CollectionUtils.h"
#include "RteUtils.h"
#include <algorithm>

using namespace std;

void CollectionUtils::PushBackUniquely(vector<string>& vec, const string& value) {
  if (find(vec.cbegin(), vec.cend(), value) == vec.cend()) {
    vec.push_back(value);
  }
}

void CollectionUtils::PushBackUniquely(list<string>& list, const string& value) {
  if (find(list.cbegin(), list.cend(), value) == list.cend()) {
    list.push_back(value);
  }
}

void CollectionUtils::PushBackUniquely(StrPairVec& vec, const StrPair& value) {
  for (const auto& item : vec) {
    if ((value.first == item.first) && (value.second == item.second)) {
      return;
    }
  }
  vec.push_back(value);
}

void CollectionUtils::AddStringItemsUniquely(vector<string>& dst, const vector<string>& src) {
  for (const auto& value : src) {
    PushBackUniquely(dst,value);
  }
}

void CollectionUtils::RemoveStringItems(vector<string>& dst, const vector<string>& src) {
  for (const auto& value : src) {
    if (value == "*") {
      dst.clear();
      return;
    }
    const auto& valueIt = find(dst.cbegin(), dst.cend(), value);
    if (valueIt != dst.cend()) {
      dst.erase(valueIt);
    }
  }
}

void CollectionUtils::RemoveDefines(vector<string>& dst, vector<string>& src) {
  for (const auto& value : src) {
    if (value == "*") {
      dst.clear();
      return;
    }
    const auto& valueIt = find_if(dst.cbegin(), dst.cend(),
      [value](const string& defineStr) -> bool {
        if (defineStr == value) {
          return true;
        }
        auto defKey = RteUtils::GetPrefix(defineStr, '=');
        return (defKey == value);
      });
    if (valueIt != dst.cend()) {
      dst.erase(valueIt);
    }
  }
}

StrVecMap CollectionUtils::MergeStrVecMap(const StrVecMap& map1, const StrVecMap& map2) {
  StrVecMap mergedMap(map1);
  for (const auto& [key, vec] : map2) {
    mergedMap[key].insert(mergedMap[key].end(), vec.begin(), vec.end());
  }
  return mergedMap;
}

void CollectionUtils::MergeDefines(StringVectorCollection& item) {
  for (const auto& element : item.pair) {
    AddStringItemsUniquely(*item.assign, *element.add);
    RemoveDefines(*item.assign, *element.remove);
  }
}

void CollectionUtils::MergeStringVector(StringVectorCollection& item) {
  for (const auto& element : item.pair) {
    AddStringItemsUniquely(*item.assign, *element.add);
    RemoveStringItems(*item.assign, *element.remove);
  }
}

// end of CollectionUtils.cpp
