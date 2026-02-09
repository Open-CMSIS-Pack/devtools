/*
 * Copyright (c) 2020-2026 Arm Limited. All rights reserved.
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
#include <optional>

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
 * @brief Returns reference to value stored in a map for a given key or default value reference if no entry is found
 * @param M template parameter representing a map
 * @param m a key-value map
 * @param k key to search for
 * @param v default value
 * @return stored value if found, default value otherwise
*/
template<typename M>
typename M::mapped_type const& get_or_default_const_ref(const M& m, const typename M::key_type& k, const typename M::mapped_type& v) {
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
 * @brief Returns set of key in the map
 * @tparam M template parameter representing a map
 * @param m a key-value map
 * @return std::set of keys
*/
template <typename M>
auto key_set(const M& m) {
    using Key = typename M::key_type;
    std::set<Key> ks;
    for (const auto& kv : m)
        ks.insert(kv.first);
    return ks;
}

/**
 * @brief Finds the first element in a container that satisfies a predicate.
 *
 * Searches the container in forward order and returns a reference to the
 * first element for which the predicate returns true.
 *
 * @tparam Container A container type providing begin()/end() and value_type.
 * @tparam Predicate A callable with signature bool(const value_type&).
 *
 * @param c     Container to search.
 * @param pred  Predicate applied to each element.
 *
 * @return std::optional containing a reference to the matching element,
 *         or std::nullopt if no such element is found.
 *
 * @note The returned reference remains valid only as long as the container
 *       is not structurally modified (e.g. erase, reallocation).
 *
 * @complexity Linear in the size of the container.
 */
#if defined(__cpp_lib_optional)
template <typename Container, typename Predicate>
auto find_item(Container& c, Predicate pred)
    -> std::optional<std::reference_wrapper<typename Container::value_type>>
{
    auto it = std::find_if(std::begin(c), std::end(c), pred);
    if(it == std::end(c)) {
      return std::nullopt;
    }
    return *it;   // reference, not a copy
}
#endif

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

/**
 * @brief string collection containing
 *        pointer to destination element
 *        vector with pointers to source elements
*/
struct StringCollection {
  std::string* assign;
  std::vector<std::string*> elements;
};

/**
 * @brief string vector pair containing
 *        items to add
 *        items to remove
*/
struct StringVectorPair {
  std::vector<std::string>* add;
  std::vector<std::string>* remove;
};

/**
 * @brief string vector collection containing
 *        pointer to destination
 *        vector with items to add/remove
*/
struct StringVectorCollection {
  std::vector<std::string>* assign;
  std::vector <StringVectorPair> pair;
};



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
   * @brief add all items from source vector to destination avoiding duplicates
   * @param dst destination vector to modify
   * @param src source vector
  */
  static void AddStringItemsUniquely(std::vector<std::string>& dst, const std::vector<std::string>& src);

  /**
   * @brief remove strings found in source vector from destination vector
   * @param dst destination vector to modify
   * @param src source vector
  */
  static void RemoveStringItems(std::vector<std::string>& dst, const std::vector<std::string>& src);

  /**
   * @brief remove define[=value] strings found in source vector from destination vector
   * @param dst destination vector to modify
   * @param src source vector
  */
  static void RemoveDefines(std::vector<std::string>& dst, std::vector<std::string>& src);

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
  /**
   * @brief merge string from vectors in to one
   * @param item StringVectorCollection to modify
  */
  static void MergeStringVector(StringVectorCollection& item);

  /**
   * @brief merge define strings from vectors in one
   * @param item StringVectorCollection to modify
  */
  static void MergeDefines(StringVectorCollection& item);
};

#endif // COLLECTION_UTILS_H
