/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef COLLECTION_UTILS_H
#define COLLECTION_UTILS_H

#include <map>
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

#endif // COLLECTION_UTILS_H
