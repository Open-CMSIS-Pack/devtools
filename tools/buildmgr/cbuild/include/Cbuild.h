/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CBUILD_H
#define CBUILD_H

#include <list>
#include <map>
#include <string>

// CMSIS Layer commands
#define L_EXTRACT 1
#define L_COMPOSE 2
#define L_ADD     3
#define L_REMOVE  4

/**
 * @brief args to construct RTE
*/
struct CbuildRteArgs
{
  const std::string &file;
  const std::string &rtePath;
  const std::string &compilerRoot;
  const std::string &toolchain;
  const std::string &ext;
  const std::string &update;
  const std::string &intDir;
  const bool checkPack;
};

/**
 * @brief args for layer operations
*/
struct CbuildLayerArgs
{
  const std::string &file;
  const std::string &rtePath;
  const std::string &compilerRoot;
  const std::list<std::string> &layerFiles;
  const std::string& name;
  const std::string& description;
  const std::string& output;
};

/**
 * @brief create RTE
 * @param args arguments to construct RTE
 * @return true if RTE created, otherwise false
*/
bool CreateRte(const CbuildRteArgs& args);

/**
 * @brief initialize message table with all supported messages
*/
void InitMessageTable ();

/**
 * @brief run layer commands
 * @param cmd layer command to be executed
 * @param args arguments that control the operation of a command
 * @return true if command runs successfully, otherwise false
*/
bool RunLayer(const int &cmd, const CbuildLayerArgs& args);

#endif // CBUILD_H
