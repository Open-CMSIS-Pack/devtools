/*
 * Copyright (c) 2020-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CBUILDGEN_H
#define CBUILDGEN_H

#include <cxxopts.hpp>

#include <string>
#include <vector>

class CbuildGen {
public:
  /**
   * @brief entry point for running cbuildgen
   * @param argc command line argument count
   * @param argv command line argument vector
   * @param envp environment variables
  */
  static int RunCbuildGen(int argc, char** argv, char** envp);

protected:
  /**
   * @brief class constructor
  */
  CbuildGen(void);

  /**
   * @brief class destructor
  */
  ~CbuildGen(void);

  /**
   * @brief prints module name, version and copyright
   * @param
  */
  void Signature(void);

  /**
   * @brief print module's usage help
   * @param
  */
  void Usage(void);

  /**
* @brief print command based usage
* @param cmdOptionsDict map of command and options
* @param cmd command for which usage is to be generated
* @param bAuxCmd flag true when the cmd is aux command, else false
* @return true if successful, otherwise false
*/
  bool PrintUsage(
    const std::map<std::string, std::pair<std::vector<cxxopts::Option>, std::string>>& cmdOptionsDict,
    const std::string& cmd);

  /**
   * @brief print version info
   * @param
  */
  void ShowVersion(void);
};


#endif  // PROJMGR_H
