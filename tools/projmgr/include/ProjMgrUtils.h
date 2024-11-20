/*
 * Copyright (c) 2020-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PROJMGRUTILS_H
#define PROJMGRUTILS_H

#include "ProjMgrKernel.h"
#include "ProjMgrParser.h"

#include "RteConstants.h"

/**
 * @brief vector of ConnectItem pointers
*/
typedef std::vector<const ConnectItem*> ConnectPtrVec;

/**
 * @brief map of ConnectItem active flags
*/
typedef std::map<const ConnectItem*, bool> ActiveConnectMap;

/**
 * @brief connections collection item containing
 *        filename reference
 *        layer type
 *        vector of ConnectItem pointers
 *        copy assignment operator
 *        default copy constructor
*/
struct ConnectionsCollection {
  const std::string& filename;
  const std::string type;
  ConnectPtrVec connections;
  ConnectionsCollection& operator=(const ConnectionsCollection& c) { return *this; };
  ConnectionsCollection(const ConnectionsCollection& c) = default;
};

/**
 * @brief output type item containing
 *        on boolean
 *        string filename
*/
struct OutputType {
  bool on;
  std::string filename;

  OutputType() : on(false), filename(RteUtils::EMPTY_STRING)
  {}
};

/**
 * @brief output types item containing
 *        bin output type
 *        elf output type
 *        hex output type
 *        lib output type
 *        cmse output type
 *        map output type
*/
struct OutputTypes {
  OutputType bin;
  OutputType elf;
  OutputType hex;
  OutputType lib;
  OutputType cmse;
  OutputType map;
};

/**
 * @brief pack info containing
 *        pack name,
 *        pack vendor,
 *        pack version
*/
struct PackInfo {
  std::string name;
  std::string vendor;
  std::string version;
};

/**
 * @brief semantic version with unsigned integer elements
 *        major,
 *        minor,
 *        patch,
*/
struct SemVer {
  unsigned int major;
  unsigned int minor;
  unsigned int patch;
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
 * @brief map of ConnectPtrVec
*/
typedef std::map<std::string, ConnectPtrVec> ConnectPtrMap;

/**
 * @brief project manager utility class
*/
class ProjMgrUtils {

protected:
  /**
   * @brief protected class constructor to avoid instantiation
  */
  ProjMgrUtils() {};

public:

  /**
   * @brief read gpdsc file
   * @param path to gpdsc file
   * @param pointer to rte generator model
   * @param reference to validation result
   * @return pointer to created RtePackage if successful, nullptr otherwise
  */
  static RtePackage* ReadGpdscFile(const std::string& gpdsc, bool& valid);

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

  /**
   * @brief get compiler root
   * @param compilerRoot reference
  */
  static void GetCompilerRoot(std::string& compilerRoot);

  /**
   * @brief parse context entry
   * @param contextEntry string in the format <project-name>.<build-type>+<target-type>
   * @param context output structure with project name, build type and target type strings
   * @return true if parse successfully, false otherwise
  */
  static bool ParseContextEntry(const std::string& contextEntry, ContextName& context);

  /**
   * @brief set output type
   * @param typeString string with output type
   * @param type reference to OutputTypes structure
  */
  static void SetOutputType(const std::string typeString, OutputTypes& type);

  struct Error {
    Error(std::string errMsg = RteUtils::EMPTY_STRING) {
      m_errMsg = errMsg;
    }
    std::string m_errMsg;
    operator bool() const { return (m_errMsg != RteUtils::EMPTY_STRING); }
  };

  /**
   * @brief get selected list of contexts
   * @param selectedContexts list of matched contexts
   * @param allAvailableContexts list of all available contexts
   * @param contextFilter filter criteria
   * @return list of filters for which match was not found, else empty list
  */
  static Error GetSelectedContexts(
    std::vector<std::string>& selectedContexts,
    const std::vector<std::string>& allAvailableContexts,
    const std::vector<std::string>& contextFilters);

  /**
   * @brief convert a pack ID to a pack info
   * @param packId the pack id (YML format)
   * @param packInfo the pack info struct
   * @return true on success
  */
  static bool ConvertToPackInfo(const std::string& packId, PackInfo& packInfo);

  /**
   * @brief check if the two pack info structs match
   * @param exactPackInfo fully qualified pack ID, without wildcards or ranges
   * @param packInfoToMatch pack ID, may include wildcards or ranges
   * @return true if match
  */
  static bool IsMatchingPackInfo(const PackInfo& exactPackInfo, const PackInfo& packInfoToMatch);

  /**
   * @brief convert version in YML format to CPRJ range format
   * @param version version in YML format
   * @return version in CPRJ range format
  */
  static std::string ConvertToVersionRange(const std::string& version);

  /**
   * @brief create IO sequences table according to executes nodes input/output
   * @param vector of executes nodes
   * @return map with IO sequences
  */
  static StrMap CreateIOSequenceMap(const std::vector<ExecutesItem>& executes);

  /**
   * @brief replace delimiters "::|:|&|@>=|@|.|/| " by underscore character
   * @param input string
   * @return string with replaced characters
  */
  static std:: string ReplaceDelimiters(const std::string input);

  /**
   * @brief find referenced context
   * @param currentContext current context string
   * @param refContext referenced context string
   * @param selectedContexts selected contexts string vector
   * @return string with replaced characters
  */
  static const std::string FindReferencedContext(const std::string& currentContext, const std::string& refContext, const StrVec& selectedContexts);

  /**
   * @brief check whether a string contains access sequence
   * @param string value
   * @return true on success
  */
  static bool HasAccessSequence(const std::string value);

  /**
   * @brief get semantic version elements
   * @param string version
   * @return structured semantic version
  */
  static SemVer GetSemVer(const std::string version);

  /**
   * @brief format path
   * @param string original path
   * @param string base destination directory
   * @param bool use absolute paths
   * @return string formatted path
  */
  static const std::string FormatPath(const std::string& original, const std::string& directory, bool useAbsolutePaths = false);

protected:
  static std::string ConstructID(const std::vector<std::pair<const char*, const std::string&>>& elements);
  /**
   * @brief get filtered list of contexts
   * @param allContexts list of all available contexts
   * @param contextFilter filter criteria
   * @return list of filters for which match was not found, else empty list
  */
  static std::vector<std::string> GetFilteredContexts(
    const std::vector<std::string>& allContexts,
    const std::string& contextFilter);
};

#endif  // PROJMGRUTILS_H
