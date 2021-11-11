/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CBUILDUTILS_H
#define CBUILDUTILS_H

#include "RteFile.h"
#include "RteFsUtils.h"


#define BS "\\"         // Back slash
#define DS "//"         // Double slash
#define SS "/"          // Single slash
#define LDOT "./"       // Leading dot
#define LDOTS "../"     // Leading dots
#define WS " "          // Whitespace
#define EMPTY ""        // Empty std::string
#define EOL "\n"        // End of line
#define PDEXT ".cprj"   // Project description extension
#define CMEXT ".cmake"  // CMake extension
#define TXTEXT ".txt"   // Text extension
#define LOGEXT ".clog"  // Audit file extension
#define ETCDIR "etc/"   // etc folder
#define BINDIR "bin/"   // bin folder

class CbuildUtils {
public:
  typedef std::pair<std::string, int> Result;

  CbuildUtils();
  ~CbuildUtils();

  /**
   * @brief get type of file
   * @param cat file type category
   * @param file std::string path to file
   * @return the type of file
  */
  static const RteFile::Category GetFileType(const RteFile::Category cat, const std::string& file);

  /**
   * @brief Adds a slash '/' to the end of non-empty strings if missing
   * @param path string possibly ending with '/'
   * @return string with ending '/' or empty string
  */
  static const std::string ensureEndWithSlash(const std::string& path);

  /**
   * @brief remove slashes from string
   * @param path string containing slashes
   * @return string without slashes
  */
  static const std::string RemoveSlash(const std::string& path);

  /**
   * @brief remove trailing slash from string
   * @param path string containing trailing slashes
   * @return string without trailing slashes
  */
  static const std::string RemoveTrailingSlash(const std::string& path);

  /**
   * @brief replace colons from string
   * @param path string containing colons
   * @return string without colons
  */
  static const std::string ReplaceColon(const std::string& path);

  /**
   * @brief replace spaces by question marks
   * @param path string containing spaces
   * @return string with spaces replaced with '?'
  */
  static const std::string ReplaceSpacesByQuestionMarks(const std::string& path);

  /**
   * @brief escape quotes from string
   * @param path string containing quote characters
   * @return string with quotes escaped
  */
  static const std::string EscapeQuotes(const std::string& path);

  /**
   * @brief escape spaces from string
   * @param path string containing spaces
   * @return string with quotes escaped
  */
  static const std::string EscapeSpaces(const std::string& path);

  /**
   * @brief get RTE item by tag and attribute
   * @param children list of RTE items to be looked into
   * @param tag string tag to be searched for
   * @param attribute string attribute to be searched for
   * @param value attribute value to be looked for
   * @return Rte item corresponding to tag, attribute and value
  */
  static const RteItem* GetItemByTagAndAttribute(const std::list<RteItem*>& children, const std::string& tag, const std::string& attribute, const std::string& value);

  /**
   * @brief get local timestamp
   * @return local timestamp in string
  */
  static const std::string GetLocalTimestamp();

  /**
   * @brief convert backslashes into forward slashes
   * @param path path containing backslashes
   * @return string path with forward slashes only
  */
  static const std::string StrPathConv(const std::string& path);

  /**
   * @brief execute shell command
   * @param cmd string shell command to be executed
   * @return command execution result <string, error_code>
  */
  static const Result ExecCommand(const std::string& cmd);

  /**
   * @brief function to support directory name with spaces
   * @param path string path with directory names containing spaces
   * @return updated path in string
  */
  static const std::string UpdatePathWithSpaces(const std::string& path);

  /**
   * @brief convert path to absolute if it's unambiguously recognized as relative
   *        accept toolchain flag as input (e.g. key=./relative/path)
   * @param path string relative path to concatenate to base
   * @param base beginning of a fully qualified path
   * @return an absolute path from a relative path and a fully qualified base path
  */
  static const std::string StrPathAbsolute(const std::string& path, const std::string& base);

  /**
   * @brief Add a value into a vector if it does not already exist in the vector
   * @param vec The vector to add the value into
   * @param value the value to add
  */
  static void PushBackUniquely(std::vector<std::string>& vec, const std::string& value);

};

#endif /* CBUILDUTILS_H */

