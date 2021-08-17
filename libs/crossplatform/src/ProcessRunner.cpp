/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ProcessRunner.h"

ProcessRunner::ProcessRunner(bool readStream)
{
  m_pinfo = ProcInfo {
              INVALID_PID,
              INVALID_FILEDESCRIPTOR,
              INVALID_FILEDESCRIPTOR
  };

  m_errcode = 0;
  m_state = STATE::UNKNOWN;
  m_readStream = readStream;
  m_pstreamReader = nullptr;

  if (readStream) {
    m_pstreamReader = new StreamReader();
  }
}

ProcessRunner::~ProcessRunner() {
  delete m_pstreamReader;
  m_pstreamReader = nullptr;

  Kill();
}

bool ProcessRunner::Run(const std::string& path, const std::list<std::string>& args) {
  if (m_state == STATE::STARTED) {
    return false;
  }

  m_pinfo = Launch(path, args, m_readStream, m_errcode);
  if (INVALID_PID == m_pinfo.pid) {
    return false;
  }

  if (m_readStream) {
    m_pstreamReader->Start(m_pinfo);
  }

  m_state = STATE::STARTED;
  return true;
}

bool ProcessRunner::Kill() {
  bool success = false;

  if (m_pinfo.pid != INVALID_PID) {
    success = Terminate(m_pinfo);
  }

  m_errcode = 0;
  m_pinfo.pid = INVALID_PID;
  m_state = STATE::KILLED;

  return success;
}

bool ProcessRunner::IsRunning() {
  if (m_pinfo.pid == INVALID_PID) {
    return false;
  }

  return IsRunning(m_pinfo);
}

bool ProcessRunner::HasStarted(unsigned int waitSec) {
  bool result = false;
  unsigned int remainingTime = waitSec * 1000;

  // checks every 10ms
  do {
    remainingTime = remainingTime - 10;
    result = IsRunning();
    if (!result) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  } while (!result && remainingTime > 0);

  return result;
}

bool ProcessRunner::HasStopped(unsigned int waitSec) {
  bool result = false;
  unsigned int remainingTime = waitSec * 1000;

  // Runs every 10ms
  do {
    remainingTime = remainingTime - 10;
    result = !IsRunning();
    if (!result) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  } while (!result && remainingTime > 0);

  return result;
}

ErrorCode ProcessRunner::GetErrorCode() const {
  return m_errcode;
}

bool ProcessRunner::WaitForProcessOutput(std::function<bool(const std::string)> cond, unsigned int timoutSec) {
  if (!m_readStream) {
    return false;
  }

  bool result = false;
  unsigned int remainingTime = timoutSec * 1000;
  std::string childStdout;

  // Runs every 10ms
  do {
    remainingTime = remainingTime - 10;
    childStdout = m_pstreamReader->PopItem();
    result = cond(childStdout);
    if (!result) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  } while (!result && remainingTime > 0);

  return result;
}

