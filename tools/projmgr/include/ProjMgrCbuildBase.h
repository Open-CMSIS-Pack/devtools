/*
 * Copyright (c) 2020-2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PROJMGRCBUILDBASE_H
#define PROJMGRCBUILDBASE_H

#include <vector>
#include <string>

/**
 * Forward declarations
*/
namespace YAML {
  class Node;
}

struct CustomItem;

/**
  * @brief projmgr base class for output yaml files
*/
class ProjMgrCbuildBase {
protected:
  ProjMgrCbuildBase(bool useAbsolutePaths = false) : m_useAbsolutePaths(useAbsolutePaths) {};
  void SetNodeValue(YAML::Node node, const std::string& value);
  void SetNodeValue(YAML::Node node, const std::vector<std::string>& vec);
  void SetNodeValueUniquely(YAML::Node node, const std::string& value);
  void SetCustomNodes(YAML::Node node, const CustomItem& custom);
  YAML::Node GetCustomNode(const CustomItem& value);
  const std::string FormatPath(const std::string& original, const std::string& directory);

  bool m_useAbsolutePaths;
};

#endif  // PROJMGRCBUILDBASE_H
