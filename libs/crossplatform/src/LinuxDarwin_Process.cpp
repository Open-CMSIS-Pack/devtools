/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ProcessRunner.h"

#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#define PIPE_READ 0
#define PIPE_WRITE 1

ProcInfo ProcessRunner::Launch(const std::string& path,
                              const std::list<std::string>& args,
                              bool readStream,
                              ErrorCode& errcode) {
  int stdoutFD[2] = {INVALID_FILEDESCRIPTOR, INVALID_FILEDESCRIPTOR};
  int stderrFD[2] = {INVALID_FILEDESCRIPTOR, INVALID_FILEDESCRIPTOR};

  struct ProcInfo pinfo = {
    INVALID_PID,
    INVALID_FILEDESCRIPTOR,
    INVALID_FILEDESCRIPTOR
  };

  int result;
  std::string cmd, params;
  std::vector<char*> argv;

  argv.push_back(const_cast<char*>(path.data()));
  for (std::string const& arg : args) {
    argv.push_back(const_cast<char*>(arg.data()));
  }
  argv.push_back(NULL);

  if (pipe(stdoutFD) < 0) {
    perror("allocating pipe for child output redirect");
    return pinfo;
  }

  if (pipe(stderrFD) < 0) {
    perror("allocating pipe for child error redirect");
    return pinfo;
  }

  pid_t childPID = fork();

  // Failed to fork child process
  if (childPID < 0) {
    errcode = errno;
    close(stdoutFD[PIPE_READ]);
    close(stdoutFD[PIPE_WRITE]);
    close(stderrFD[PIPE_READ]);
    close(stderrFD[PIPE_WRITE]);
    return pinfo;
  }

  if (childPID == 0) {
    if (readStream) {
      // redirect stdout
      if (dup2(stdoutFD[PIPE_WRITE], STDOUT_FILENO) == -1) {
        exit(errno);
      }

      // redirect stderr
      if (dup2(stderrFD[PIPE_WRITE], STDERR_FILENO) == -1) {
        exit(errno);
      }

      // all these are for use by parent only
      close(stdoutFD[PIPE_READ]);
      close(stdoutFD[PIPE_WRITE]);
      close(stderrFD[PIPE_READ]);
      close(stderrFD[PIPE_WRITE]);
    }
    result = execvp(path.c_str(), argv.data());
    exit(result);
  }

  pinfo.pid      = childPID;
  pinfo.stdoutfd = stdoutFD[PIPE_READ];
  pinfo.stderrfd = stderrFD[PIPE_READ];

  // close unused file descriptors
  close(stdoutFD[PIPE_WRITE]);
  close(stderrFD[PIPE_WRITE]);
  return pinfo;
}

bool ProcessRunner::Terminate(ProcInfo pinfo) {
  int status;

  // Force kill
  close(pinfo.stdoutfd);
  close(pinfo.stderrfd);
  ::kill(pinfo.pid, SIGTERM);
  waitpid(pinfo.pid, &status, 0);
  status = WEXITSTATUS(status);

  return (WIFEXITED(status) || WIFSIGNALED(status)) ? true : false;
}

bool ProcessRunner::IsRunning(ProcInfo pinfo, unsigned int waitTimeSec) {
  int status;
  int w = waitpid(pinfo.pid, &status, WNOHANG);

  return (w == 0) || !(WIFEXITED(status) || WIFSTOPPED(status));
}
