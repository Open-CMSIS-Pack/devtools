/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef XML_InputSourceReaderFile_H
#define XML_InputSourceReaderFile_H

#include <cstdio>
#include <cstring>
#include <cassert>
#include "limits.h"

#include "XML_Reader.h"


class XML_InputSourceReaderFile : public XML_InputSourceReader
{
public:
  XML_InputSourceReaderFile() : XML_InputSourceReader(), m_fpInFile(0), m_bFile(false) {
  }

  ~XML_InputSourceReaderFile() override {
    XML_InputSourceReaderFile::Close();
  }

  bool IsValid() const override {
    return m_bFile ? m_fpInFile != NULL : true;
  }

  void Close() override {
    if (m_fpInFile) {
      fclose(m_fpInFile);
      m_fpInFile = 0;
      m_bFile = false;
    }
    XML_InputSourceReader::Close();
  }

  size_t ReadLine(char* buf, size_t maxLen) override {
    if (m_bFile) {
      if (m_fpInFile) {
        return fread(buf, sizeof(char), maxLen, m_fpInFile);
      }
      return 0;
    }
    return XML_InputSourceReader::ReadLine(buf, maxLen);
  }

protected:
  XmlTypes::Err DoOpen() override {
    if (m_source->xmlString && strlen(m_source->xmlString) > 0) {
      m_bFile = false;
      return XML_InputSourceReader::DoOpen();
    }
    m_bFile = true;

    if (!m_source->fileName.length()) {
      return XmlTypes::Err::ERR_NO_INPUT_FILE;
    }
    m_fpInFile = fopen(m_source->fileName.c_str(), "r");
    if (!m_fpInFile) {
      return XmlTypes::Err::ERR_OPEN_FAILED;
    }

    fseek(m_fpInFile, 0, SEEK_END);               // Read File size
    m_size = ftell(m_fpInFile);

    size_t seekPos = m_source->seekPos;           // set position
    if(seekPos > LONG_MAX) {                      // fseek takes "long"
      return XmlTypes::Err::ERR_OPEN_FAILED;
    }

    if(fseek(m_fpInFile, (long)seekPos, SEEK_SET)) {
      return XmlTypes::Err::ERR_OPEN_FAILED;
    }

    return XmlTypes::Err::ERR_NOERR;
  }

protected:
  FILE  *m_fpInFile;
  bool   m_bFile;
};

#endif // !XML_InputSourceReaderFile_H
