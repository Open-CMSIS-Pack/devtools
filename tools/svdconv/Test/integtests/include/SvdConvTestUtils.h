/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SvdConvTestUtils_H
#define SvdConvTestUtils_H

#include <string>
#include <map>
#include <list>
#include <vector>
#include <fstream>
#include <regex>


class Arguments {
public:
  explicit Arguments(const std::string& execName) {
    add(execName);
  }
  explicit Arguments(const std::string& execName, const std::string& arg) {
    add(execName);
    add(arg);
  }
  explicit Arguments(const std::string& execName, const std::initializer_list<std::string>& args) {
    add(execName);
    add(args);
  }
  ~Arguments() {
  }

  void add(const std::initializer_list<std::string>& args) {
    for(const auto& arg : args) {
      add(arg);
    }
  }

  void add(const std::string& arg) {
    m_internalArguments.push_back(arg);
  }

  void clear() {
    m_arguments.clear();
    m_internalArguments.clear();
  }

  operator int() {
    return m_internalArguments.size();
  }

  operator const char**() {
    m_arguments.clear();
    for(auto& arg : m_internalArguments) {
      m_arguments.push_back(arg.c_str());
    }

    return m_arguments.data();
  }

private:
  std::list<std::string> m_internalArguments;
  std::vector<const char*> m_arguments;
};



class SvdConvTestUtils
{
private:
  SvdConvTestUtils();
  ~SvdConvTestUtils();

public:
  static std::list<std::smatch> FindRegex(const std::string& buf, const std::regex& pattern);
  static bool FindAllEntries(const std::list<std::smatch>& result, const std::list<std::string>& entries);
  static bool FindEntry(const std::list<std::smatch>& result, const std::string& entry);

private:
};

#endif // SvdConvTestUtils_H
