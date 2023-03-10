/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ErrOutputterSaveToStdoutOrFile_H
#define ErrOutputterSaveToStdoutOrFile_H

#include "ErrOutputter.h"

#include <fstream>
#include <iostream>

// DEPRECATED! Provided for compatibility with existing code base.
// Outputs error strings to file or stdout on save (usually application exit)
class ErrOutputterSaveToStdoutOrFile : public ErrOutputter
{
public:
  void Save() override { // called by ErrLog::Save saves/outputs collected messages
    if(m_logText.empty()) {
      return;
    }
    bool bfile = !m_logFileName.empty();
    std::ofstream fileStream;
    if (bfile) {
      fileStream.open(m_logFileName, std::ios::binary);
    }
    std::ostream& logStream = bfile ? fileStream : std::cout;

    for (auto it = m_logText.begin(); it != m_logText.end(); it++) {
      const std::string &text = *it;
      if (!text.empty()) {
        logStream << text;
      } else {
        logStream << "\n";
      }
    }
    logStream << "\n";
    if (bfile) {
      fileStream.close();
    }
    Clear();
  };
};


#endif // #ifndef ErrOutputterSaveToStdoutOrFile_H
