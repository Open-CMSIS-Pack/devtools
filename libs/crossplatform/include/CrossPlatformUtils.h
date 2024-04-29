/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef CROSSPLATFORM_UTILS_H
#define CROSSPLATFORM_UTILS_H

#include <string>
#include <system_error>
#include <filesystem>

/**
 * @brief CrossPlatformUtils utility class provides proxy methods to call platform-specific functions
*/
class CrossPlatformUtils
{
private:
  // private constructor to prevent instantiating an utility class
  CrossPlatformUtils() {};
  static const std::string PopenCmd(const std::string& cmd);

public:

  /**
   * @brief Returns environment variable value for given name
   * @param name environment variable name without decorations, e.g. PATH
   * @return value of the variable as std::string:
   * non-empty string if variable is supported, found and set to a non-empty value;
   * empty string in all other cases
   *
  */
  static std::string GetEnv(const std::string& name);

  /**
   * @brief Sets environment variable value
   * @param name environment variable name (may not be empty)
   * @param value string value to set (can be empty)
   * @return true if successful
  */
  static bool SetEnv(const std::string& name, const std::string& value);

  /**
   * @brief Returns CMSIS-Pack root directory path
   * @return CMSIS-Pack root directory path set by CMSIS_PACK_ROOT environment variable or default location
  */
  static std::string GetCMSISPackRootDir();

  /**
   * @brief Returns default CMSIS-Pack root directory path (platform specific)
   * @return default CMSIS-Pack root directory path
  */
  static std::string GetDefaultCMSISPackRootDir();

  /**
   * @brief Calculates the wall-clock time used by the calling process
   * @return The elapsed time since the start of the process, measured in milliseconds
  */
  static unsigned long ClockInMsec();

  /**
   * @brief Gets path of running executable
   * @param ec reference to std::error_code object to recieve error information if any
   * @return path to running executable
  */
  static std::string GetExecutablePath(std::error_code& ec);

  /**
   * @brief Gets host type in the form required by RTE libraries
   * @return std::string containing one of : "win", "linux", "mac"
  */
  static const std::string& GetHostType();

  /**
   * @brief Get registry key value for Windows (environment variable value on Linux and Mac)
   * @param key registry key value to obtain (environment variable on Linux and Mac)
   * @return value as std::string if found, empty otherwise
  */
  static std::string GetRegistryString(const std::string& key);

  /**
   * @brief check if given path has execute permissions
   * @param path path to be checked
   * @return true if path provides effective execute permissions
  */
  static bool CanExecute(const std::string& file);

  /**
   * @brief execute shell command
   * @param cmd string shell command to be executed
   * @return command execution result <string, error_code>
  */
  static const std::pair<std::string, int> ExecCommand(const std::string& cmd);

  enum class REG_STATUS {
    ENABLED,
    DISABLED,
    NOT_SUPPORTED
  };
  /**
   * @brief Get status of LongPathsEnabled registry
   * @return return ENABLED, DISABLED, NOT_SUPPORTED
  */
  static REG_STATUS GetLongPathRegStatus();

  /**
   * @brief Get the current umask value
   * @return The umask value as a std::filesystem::perms type
  */
  static std::filesystem::perms GetCurrentUmask();
};

#endif  /* CROSSPLATFORM_UTILS_H */
