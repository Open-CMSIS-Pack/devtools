/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PACKGEN_H
#define PACKGEN_H

#include "XMLTreeSlim.h"
#include "yaml-cpp/yaml.h"
#include <string>

/**
 * @brief build information structure containing
 *        source code files,
 *        include paths,
 *        defines
*/
struct buildInfo {
  std::set<std::string> src;
  std::set<std::string> inc;
  std::set<std::string> def;
};

/**
 * @brief build options structure containing
 *        build's name for internal use,
 *        CMake build options
*/
struct buildOptionsInfo {
  std::string name;
  std::string options;
};

/**
 * @brief target structure containing
 *        build information structure,
 *        set of dependencies
*/
struct targetInfo {
  buildInfo build;
  std::set<std::string> dependency;
};

/**
 * @brief build list structure containing
 *        operation to perform (intersection, difference),
 *        list of build names
*/
struct buildListInfo {
  std::string operation;
  std::list<std::string> names;
};

/**
 * @brief requirements structure containing
 *        list of maps describing required packages,
 *        list of maps describing required compilers,
 *        list of maps describing required languages
*/
struct requirementInfo {
  std::list<std::map<std::string, std::string>> packages;
  std::list<std::map<std::string, std::string>> compilers;
  std::list<std::map<std::string, std::string>> languages;
};

/**
 * @brief condition structure containing
 *        the condition rule (require or accept or deny),
 *        map of required, accepted or denied attributes
*/
struct conditionInfo {
  std::string rule;
  std::map<std::string, std::string> attributes;
};

/**
 * @brief file structure containing
 *        name of file,
 *        map of file attributes,
 *        list of file conditions
*/
struct fileInfo {
  std::string name;
  std::map<std::string, std::string> attributes;
  std::list<conditionInfo> conditions;
};

/**
 * @brief component structure containing
 *        list of CMake targets,
 *        set of build options to consider,
 *        build information structure,
 *        list of dependencies,
 *        list of component conditions,
 *        list of component files,
 *        map of component attributes,
 *        description string
*/
struct componentInfo {
  std::list<std::string> target;
  buildListInfo builds;
  buildInfo build;
  std::list<std::string> dependency;
  std::list<conditionInfo> condition;
  std::list<fileInfo> files;
  std::map<std::string, std::string> attributes;
  std::string description;
};

/**
 * @brief taxonomy structure containing
 *`        map of component attributes,
 *`        description string
*/
struct taxonomyInfo {
  std::map<std::string, std::string> attributes;
  std::string description;
};

/**
 * @brief api structure containing
 *        list of api files
 *        map of component attributes,
 *        description string
*/
struct apiInfo {
  std::list<fileInfo> files;
  std::map<std::string, std::string> attributes;
  std::string description;
};

/**
 * @brief repository structure containing
 *        type of repository
 *        url of repository
*/
struct repositoryInfo {
  std::string type;
  std::string url;
};

/**
 * @brief release structure containing
 *        map of release attributes
*/
struct releaseInfo {
  std::map<std::string, std::string> attributes;
};

/**
 * @brief pack structure containing
 *        pack name string,
 *        pack description string,
 *        pack vendor string,
 *        license filename,
 *        url location of the pack,
 *        list of release elements,
 *        requirements structure,
 *        list of taxonomy elements,
 *        list of api elements,
 *        list of component elements,
 *        pack output directory
*/
struct packInfo {
  std::string name;
  std::string description;
  std::string vendor;
  std::string version;
  std::string license;
  std::string url;
  repositoryInfo repository;
  std::list<releaseInfo> releases;
  requirementInfo requirements;
  std::list<taxonomyInfo> taxonomy;
  std::list<std::string> apis;
  std::list<std::string> components;
  std::string outputDir;
};

/**
 * @brief YAML query requests structure for CMake File API
*/
struct queryRequests {
  std::string kind;
  int major;
  int minor;
};

/**
 * @brief YAML query emitter operator for CMake File API
*/
YAML::Emitter& operator << (YAML::Emitter& out, const queryRequests& req);

class PackGen {
public:

  /**
   * @brief class constructor
  */
  PackGen(void);

  /**
   * @brief class destructor
  */
  ~PackGen(void);

  /**
   * @brief entry point for running packgen
   * @param argc command line argument count
   * @param argv command line argument vector
  */
  static int RunPackGen(int argc, char** argv);

  /**
   * @brief generate CMake File API query
   * @return true if no errors happened, false otherwise
  */
  bool CreateQuery(void);

  /**
   * @brief parse CMake File API reply to retrieve CMake targets
   * @return true if no errors happened, false otherwise
  */
  bool ParseReply(void);

  /**
   * @brief parse input manifest YAML file
   * @return true if no errors happened, false otherwise
  */
  bool ParseManifest(void);

  /**
   * @brief create components contents by merging and/or filtering out CMake targets
   * @return true if no errors happened, false otherwise
  */
  bool CreateComponents(void);

  /**
   * @brief generate pdsc file and copy referenced files and include paths into output folder
   * @return true if no errors happened, false otherwise
  */
  bool CreatePack(void);

  /**
   * @brief run packchk to validate the generated pack
   * @return true if no errors happened, false otherwise
  */
  bool CheckPack(void);

  /**
   * @brief run 7z to compress the generated pack
   * @return true if no errors happened, false otherwise
  */
  bool CompressPack(void);

protected:
  std::string m_manifest;
  std::string m_repoRoot;
  std::string m_outputRoot;
  std::vector<std::string> m_externalPdsc;
  bool m_verbose = false;
  bool m_regenerate = false;
  bool m_noComponents = true;

  XMLTree* m_pdscTree = NULL;
  std::list<packInfo> m_pack;
  std::map<std::string, std::map<std::string, targetInfo>> m_target;
  std::map<std::string, componentInfo> m_components;
  std::map<std::string, apiInfo> m_apis;
  std::list<buildOptionsInfo> m_buildOptions;
  std::map<std::string, std::list<std::string>> m_extensions;

  static void SetAttribute(XMLTreeElement* element, const std::string& name, const std::string& value);
  static bool CopyItem(const std::string& src, const std::string& dst, std::list<std::string>& ext);
  static const std::string GetFileCategory(const std::string& file, std::list<std::string>& ext);
  static uint32_t CountNodes(const YAML::Node node, const std::string& name);
  void AddComponentBuildInfo(const std::string& componentName, buildInfo& reference);
  void InsertBuildInfo(buildInfo& build, const std::string& targetName, const std::string& buildName);
  void FilterOutDependencies(const std::string& name, const componentInfo& component);
  void GetBuildInfo(buildInfo& reference, const std::list<std::string>& targetNames, const std::list<std::string>& buildNames, const std::string& operation);
  void GetBuildInfoIntersection(buildInfo& reference, buildInfo& actual, buildInfo& intersect);
  void GetBuildInfoDifference(buildInfo& reference, buildInfo& actual, buildInfo& difference);
  void CreatePackInfo(XMLTreeElement* rootElement, packInfo& pack);
  void CreatePackRequirements(XMLTreeElement* rootElement, packInfo& pack);
  void CreatePackReleases(XMLTreeElement* rootElement, packInfo& pack);
  void CreatePackApis(XMLTreeElement* rootElement, packInfo& pack);
  void CreatePackTaxonomy(XMLTreeElement* rootElement, packInfo& pack);
  void CreatePackComponentsAndConditions(XMLTreeElement* rootElement, packInfo& pack);
  void ParseManifestInfo(const YAML::Node node, packInfo& pack);
  void ParseManifestReleases(const YAML::Node node, packInfo& pack);
  void ParseManifestRequirements(const YAML::Node node, packInfo& pack);
  void ParseManifestTaxonomy(const YAML::Node node, packInfo& pack);
  void ParseManifestApis(const YAML::Node node, packInfo& pack);
  bool ParseManifestComponents(const YAML::Node node, packInfo& pack);
  void ShowVersion(void);
};

#endif  // PACKGEN_H
