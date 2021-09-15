/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "StreamReader.h"

std::string StreamReader::AsyncRead(file_desc_t fd) {
  DWORD bytesRead;
  char buffer[BUFSIZE] = { 0 };

  ReadFile(fd, buffer, BUFSIZE, &bytesRead, nullptr);

  return (bytesRead <= 0) ? "" : std::string(buffer, buffer + bytesRead);
}
