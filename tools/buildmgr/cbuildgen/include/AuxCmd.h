/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

 // AuxCmd.h
#ifndef AUXCMD_H
#define AUXCMD_H

#include <list>
#include <string>

#define AUX_MKDIR 1
#define AUX_RMDIR 2
#define AUX_TOUCH 3

class AuxCmd {
public:
  AuxCmd(void);
  ~AuxCmd(void);

  /**
   * @brief run auxiliary command
   * @param cmd integer command identifier
   * @param params list of command parameters
   * @param except string path to the directories not to be removed
   * @return true if command executed successfully, otherwise false
  */
  bool RunAuxCmd(int cmd, const std::list<std::string>& params, const std::string& except);

protected:
  bool MkdirCmd(const std::list<std::string>& params);
  bool RmdirCmd(const std::list<std::string>& params, const std::string& except);
  bool TouchCmd(const std::list<std::string>& params);
};

#endif  // AUXCMD_H
