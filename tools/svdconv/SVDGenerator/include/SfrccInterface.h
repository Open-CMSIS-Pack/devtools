/*
 * Copyright (c) 2010-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SfrccInterface_H
#define SfrccInterface_H

#include <string>


class FileIo;

class SfrccInterface
{
public:
  SfrccInterface();
  ~SfrccInterface();

  bool                Compile        (const std::string& fileName, bool bCleanup = true);
  bool                SetModulePath  (const std::string& modulePath)   { m_modulePath = modulePath; return true; }
  const std::string&  GetModulePath  ()                                { return m_modulePath; }

protected:
  std::string   GetPathToSfrcc       ();

private:
  std::string  m_modulePath;
};

#endif // SfrccInterface_H
