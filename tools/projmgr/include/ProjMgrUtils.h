/*
 * Copyright (c) 2020-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PROJMGRUTILS_H
#define PROJMGRUTILS_H

#include "ProjMgrKernel.h"

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
   * @brief get fully specified package identifier
   * @param rte package
   * @return string package identifier
  */
  static std::string GetPackageID(const RteItem* pack);

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

protected:
  static std::string ConstructID(const std::vector<std::pair<const char*, const std::string&>>& elements);
};

#endif  // PROJMGRUTILS_H
