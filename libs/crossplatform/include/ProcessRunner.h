/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef PROCESS_H
#define PROCESS_H

#include "StreamReader.h"

#include <list>

/*
* ProcessRunner platform independent implementation
*/
typedef unsigned long ErrorCode;

class ProcessRunner
{
public:
  enum class STATE { UNKNOWN = -1, FAILED = 0, STARTED, RUNNING, KILLED };

  ProcessRunner(bool readStream = false);
  ~ProcessRunner();

  // Runs the specified executable in separate process
  bool Run(const std::string& path, const std::list<std::string>& args);
  // Force kills the process
  bool Kill();

  // Wait process for specific condition
  bool WaitForProcessOutput(
    std::function<bool(const std::string)> cond,
    unsigned int timoutSec);

  // Checks if the process has started
  bool HasStarted(unsigned int waitSec);
  // Checks if the process has stopped
  bool HasStopped(unsigned int waitSec);

  // Get the error code
  ErrorCode GetErrorCode() const;

  // Platform specific helpers
  static ProcInfo Launch(const std::string& path,
                        const std::list<std::string>& args,
                        bool readStream,
                        ErrorCode& m_errcode);
  static bool     IsRunning(ProcInfo pinfo, unsigned int waitTimeSec = 0);
  static bool     Terminate(ProcInfo pinfo);

protected:
  ProcInfo              m_pinfo;
  ErrorCode             m_errcode;
  ProcessRunner::STATE  m_state;
  bool                  m_readStream;
  StreamReader*         m_pstreamReader;

  // Checks if the process is running
  bool IsRunning();
};

#endif  /* #ifndef PROCESS_H */
