/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ProcessRunner.h"

ProcInfo ProcessRunner::Launch(const std::string& path,
                                const std::list<std::string>& args,
                                bool readStream,
                                ErrorCode& errcode) {
  std::string cmd;
  STARTUPINFO startupInfo;

  struct ProcInfo pinfo = {
    INVALID_PID,
    INVALID_FILEDESCRIPTOR,
    INVALID_FILEDESCRIPTOR
  };

  PROCESS_INFORMATION processInfo;
  ZeroMemory(&processInfo, sizeof(processInfo));

  ZeroMemory(&startupInfo, sizeof(startupInfo));
  startupInfo.cb = sizeof(startupInfo);

  std::string params;
  for (auto arg : args) {
    params += std::string(" ") + arg;
  }

  cmd = path + params;

  file_desc_t fdstdout_Rd = INVALID_FILEDESCRIPTOR;
  file_desc_t fdstdout_Wr = INVALID_FILEDESCRIPTOR;

  file_desc_t fdstderr_Rd = INVALID_FILEDESCRIPTOR;
  file_desc_t fdstderr_Wr = INVALID_FILEDESCRIPTOR;

  if (readStream)
  {
    SECURITY_ATTRIBUTES security_attributes;
    security_attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    security_attributes.bInheritHandle = TRUE;
    security_attributes.lpSecurityDescriptor = nullptr;

    if (!CreatePipe(&fdstdout_Rd, &fdstdout_Wr, &security_attributes, 0) ||
      !SetHandleInformation(fdstdout_Rd, HANDLE_FLAG_INHERIT, 0))
    {
      return pinfo;
    }

    if (!CreatePipe(&fdstderr_Rd, &fdstderr_Wr, &security_attributes, 0) ||
      !SetHandleInformation(fdstderr_Rd, HANDLE_FLAG_INHERIT, 0))
    {
      return pinfo;
    }

    startupInfo.hStdError  = fdstderr_Wr;
    startupInfo.hStdOutput = fdstdout_Wr;
    startupInfo.dwFlags |= STARTF_USESTDHANDLES;
  }

  BOOL success = CreateProcess(
    NULL, &cmd[0], NULL, NULL, readStream ? TRUE : FALSE,
    0, NULL, NULL, &startupInfo, &processInfo);

  if (!success) {
    errcode = GetLastError();
    return pinfo;
  }

  pinfo.pid      = processInfo.hProcess;
  pinfo.stdoutfd = fdstdout_Rd;
  pinfo.stderrfd = fdstderr_Rd;

  CloseHandle(processInfo.hThread);
  CloseHandle(fdstdout_Wr);
  CloseHandle(fdstderr_Wr);

  return pinfo;
}

bool ProcessRunner::Terminate(ProcInfo pinfo) {
  bool success;

  success = (0 != ::TerminateProcess(pinfo.pid, 0));
  CloseHandle(pinfo.pid);
  CloseHandle(pinfo.stdoutfd);
  CloseHandle(pinfo.stderrfd);
 
  return success;
}

bool ProcessRunner::IsRunning(ProcInfo pinfo, unsigned int waitTimeSec) {
  unsigned long exitCode;

  if (GetExitCodeProcess(pinfo.pid, &exitCode) == 0) {
    return false;
  }

  return exitCode == STILL_ACTIVE;
}
