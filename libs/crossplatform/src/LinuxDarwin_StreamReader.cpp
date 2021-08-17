/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "StreamReader.h"

std::string StreamReader::AsyncRead(file_desc_t fd) {
  char buffer[BUFSIZE];
  ssize_t bytesRead;

  bytesRead = read(fd, &buffer, BUFSIZE);

  return (bytesRead < 0) ? "" : std::string(buffer, buffer + bytesRead);
}
