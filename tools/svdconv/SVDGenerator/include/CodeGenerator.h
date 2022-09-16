/*
 * Copyright (c) 2010-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef CodeGenerator_H
#define CodeGenerator_H

#include <fmt/printf.h>

#include <string>
#include <vector>


class CodeGenerator
{
public:
  CodeGenerator();
  ~CodeGenerator();
  

protected:

  /* Use the first ... arguments to feed the helper function, drop the remaining args into fmt::sprinf
  * Fn: helper function
  * T: class of helper function
  * Ts, Args: variadic template arguments
  */
  template<typename Fn, typename T, typename = std::enable_if_t<std::is_base_of_v<CodeGenerator, T>>, typename ...Ts, typename ...Args>
  auto parse_and_call(Fn (T::*func)(const std::string&, Ts...), const std::string& format, Args&&... args) noexcept {
      return ([this, &func, &format](Ts... args, auto ...format_args) {
          // Use the last N args as format inputs
          auto text = fmt::sprintf(format, std::forward<decltype(format_args)>(format_args)...);
          // The first M args will be passed to the helper function
          return (((T*)this)->*func)(text, std::forward<Ts>(args)...);
      })(std::forward<Args>(args)...);
  }

  static const std::string EMPTY_STRING;

private:
};

#endif
