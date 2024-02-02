/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef COLLECTION_UTILS_H
#define COLLECTION_UTILS_H

#include <algorithm>
#include <list>
#include <map>
#include <vector>
#include <set>
#include <string>

/**
 * @brief Returns value stored in a map for a given key or default value if no entry is found
 * @tparam M template parameter representing a map
 * @param m a key-value map
 * @param k key to search for
 * @param v default value
 * @return stored value if found, default value otherwise
*/
template<typename M>
typename M::mapped_type get_or_default(const M& m, const typename M::key_type& k, const typename M::mapped_type& v) {
    auto itr = m.find(k);
    if (itr != m.end()) {
        return itr->second;
    }
    return v;
}

/**
 * @brief Returns value stored in a map for a given key or nullptr
 * @tparam M template parameter representing a map with the value of pointer type
 * @param m a key-value map with a value of pointer type
 * @param k key to search for
 * @return stored value if found, nullptr otherwise
*/
template<typename M>
typename M::mapped_type get_or_null(const M& m, const typename M::key_type& k) {
    return get_or_default(m, k, nullptr);
}

/**
 * @brief Checks if a map contains given key
 * @tparam M template parameter representing a map
 * @param m a key-value map
 * @param k key to search for
 * @return true if key is found
*/
template<typename M>
bool contains_key(const M& m, const typename M::key_type& k) {
    return m.find(k) != m.end();
}

/**
  * @brief string pair
 */
typedef std::pair<std::string, std::string> StrPair;

/**
  * @brief string pair
 */
typedef std::pair<std::string, int> StrIntPair;

/**
 * @brief string vector
*/
typedef std::vector<std::string> StrVec;

/**
 * @brief string set
*/
typedef std::set<std::string> StrSet;

/**
 * @brief vector of string pair
*/
typedef std::vector<StrPair> StrPairVec;

/**
 * @brief vector of string pair pointer
*/
typedef std::vector<const StrPair*> StrPairPtrVec;

/**
 * @brief map of vector of string pair
*/
typedef std::map<std::string, StrPairVec> StrPairVecMap;

/**
 * @brief map of string vector
*/
typedef std::map<std::string, StrVec> StrVecMap;

/**
 * @brief map of int
*/
typedef std::map<std::string, int> IntMap;

/**
 * @brief map of bool
*/
typedef std::map<std::string, bool> BoolMap;

/**
 * @brief map of string
*/
typedef std::map<std::string, std::string> StrMap;

class CollectionUtils
{
private:
  CollectionUtils() {}; // private constructor to forbid instantiation of a utility class

public:
  /**
   * @brief add a string value into a vector if it does not already exist in the vector
   * @param vec the vector to add the value into
   * @param value the string value to add
  */
  static void PushBackUniquely(std::vector<std::string>& vec, const std::string& value);

  /**
   * @brief add a string value into a list if it does not already exist in the list
   * @param lst the list to add the value into
   * @param value the value to add
  */
  static void PushBackUniquely(std::list<std::string>& lst, const std::string& value);

  /**
   * @brief add a value pair into a vector if it does not already exist in the vector
   * @param vec the vector to add the value into
   * @param value the value pair to add
  */
  static void PushBackUniquely(StrPairVec& vec, const StrPair& value);

  /**
   * @brief merge two string vector maps
   * @param map1 first string vector map
   * @param map2 second string vector map
   * @return StrVecMap merged map
  */
 static StrVecMap MergeStrVecMap(const StrVecMap& map1, const StrVecMap& map2);

   /**
   * @brief remove duplicate elements from vector without changing the order of elements
   * @param input vector to be processed
  */
  template <typename T>
  static void RemoveVectorDuplicates(std::vector<T>& elemVec) {
    auto end = elemVec.end();
    for (auto itr = elemVec.begin(); itr != end; ++itr) {
      end = std::remove(itr + 1, end, *itr);
    }
    elemVec.erase(end, elemVec.end());
  }
};

#endif // COLLECTION_UTILS_H
