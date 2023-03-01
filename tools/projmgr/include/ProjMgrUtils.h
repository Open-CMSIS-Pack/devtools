/*
 * Copyright (c) 2020-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PROJMGRUTILS_H
#define PROJMGRUTILS_H

#include "ProjMgrKernel.h"
#include "ProjMgrParser.h"

 /**
 * @brief vector of ConnectItem pointers
 */
typedef std::vector<const ConnectItem*> ConnectPtrVec;

/**
 * @brief connections collection item containing
 *        filename pointer
 *        layer type pointer
 *        vector of ConnectItem pointers
 *        copy assignment operator
*/
struct ConnectionsCollection {
  const std::string& filename;
  const std::string& type;
  ConnectPtrVec connections;
  ConnectionsCollection& operator=(const ConnectionsCollection& c) { return *this; };
};

/**
 * @brief vector of ConnectionsCollection
*/
typedef std::vector<ConnectionsCollection> ConnectionsCollectionVec;

/**
 * @brief map of ConnectionsCollection
*/
typedef std::map<std::string, ConnectionsCollectionVec> ConnectionsCollectionMap;

/**
  * @brief string pair
 */
typedef std::pair<std::string, std::string> StrPair;

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
 * @brief map of string vector
*/
typedef std::map<std::string, StrVec> StrVecMap;

/**
 * @brief map of int
*/
typedef std::map<std::string, int> IntMap;

/**
 * @brief map of string
*/
typedef std::map<std::string, std::string> StrMap;

/**
 * @brief projmgr utils class
*/
class ProjMgrUtils {
public:

  typedef std::pair<std::string, int> Result;

  static constexpr const char* COMPONENT_DELIMITERS = ":&@";
  static constexpr const char* SUFFIX_CVENDOR = "::";
  static constexpr const char* PREFIX_CBUNDLE = "&";
  static constexpr const char* PREFIX_CGROUP = ":";
  static constexpr const char* PREFIX_CSUB = ":";
  static constexpr const char* PREFIX_CVARIANT = "&";
  static constexpr const char* PREFIX_CVERSION = "@";
  static constexpr const char* SUFFIX_PACK_VENDOR = "::";
  static constexpr const char* PREFIX_PACK_VERSION = "@";

  static constexpr const char* AS_PROJECT = "Project";
  static constexpr const char* AS_COMPILER = "Compiler";
  static constexpr const char* AS_BUILD_TYPE = "BuildType";
  static constexpr const char* AS_TARGET_TYPE = "TargetType";
  static constexpr const char* AS_DNAME = "Dname";
  static constexpr const char* AS_PNAME = "Pname";
  static constexpr const char* AS_BNAME = "Bname";

  /**
   * @brief class constructor
  */
  ProjMgrUtils(void);

  /**
   * @brief class destructor
  */
  ~ProjMgrUtils(void);

  /**
   * @brief get fully specified component identifier
   * @param rte component
   * @return string component identifier
  */
  static std::string GetComponentID(const RteItem* component);

  /**
   * @brief get fully specified condition identifier
   * @param rte condition
   * @return string condition identifier
  */
  static std::string GetConditionID(const RteItem* condition);

  /**
   * @brief get fully specified component aggregate identifier
   * @param rte component aggregate
   * @return string component aggregate identifier
  */
  static std::string GetComponentAggregateID(const RteItem* component);

  /**
   * @brief get partial component identifier (without vendor and version)
   * @param rte component
   * @return string partial component identifier
  */
  static std::string GetPartialComponentID(const RteItem* component);

  /**
   * @brief get fully specified package identifier
   * @param rte package
   * @return string package identifier
  */
  static std::string GetPackageID(const RteItem* pack);
  static std::string GetPackageID(const std::string& vendor, const std::string& name, const std::string& version);

  /**
   * @brief read gpdsc file
   * @param path to gpdsc file
   * @param pointer to rte generator model
   * @return true if file is read successfully, false otherwise
  */
  static bool ReadGpdscFile(const std::string& gpdsc, RteGeneratorModel* gpdscModel);

  /**
   * @brief execute shell command
   * @param cmd string shell command to be executed
   * @return command execution result <string, error_code>
  */
  static const Result ExecCommand(const std::string& cmd);

  /**
   * @brief get file category according to file extension
   * @param filename with extension
   * @return string category
  */
  static const std::string GetCategory(const std::string& file);

  /**
   * @brief add a value into a vector/list if it does not already exist in the vector/list
   * @param vec/list the vector/list to add the value into
   * @param value the value to add
  */
  static void PushBackUniquely(std::vector<std::string>& vec, const std::string& value);
  static void PushBackUniquely(std::list<std::string>& lst, const std::string& value);
  static void PushBackUniquely(StrPairVec& vec, const StrPair& value);

  /**
   * @brief convert string to int, return 0 if it's empty or not convertible
   * @param string
   * @return int
  */
  static int StringToInt(const std::string& value);

  /**
   * @brief expand compiler id the format <name>@[>=]<version> into name, minimum and maximum versions
   * @param compiler id
   * @param name reference to compiler name
   * @param minVer reference to compiler minimum version
   * @param maxVer reference to compiler maximum version
  */
  static void ExpandCompilerId(const  std::string& compiler, std::string& name, std::string& minVer, std::string& maxVer);

  /**
   * @brief check if compilers are compatible in the format <name>@[>=]<version>
   * @param first compiler id
   * @param second compiler id
   * @return true if compilers are compatible, false otherwise
  */
  static bool AreCompilersCompatible(const std::string& first, const std::string& second);

  /**
   * @brief get compilers version range intersection in the format <name>@[>=]<version>
   * @param first compiler id
   * @param second compiler id
   * @param intersection reference to intersection id
  */
  static void CompilersIntersect(const std::string& first, const std::string& second, std::string& intersection);

protected:
  static std::string ConstructID(const std::vector<std::pair<const char*, const std::string&>>& elements);
};

#endif  // PROJMGRUTILS_H
