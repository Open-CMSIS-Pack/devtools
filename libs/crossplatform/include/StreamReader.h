/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef STREAMREADER_H
#define STREAMREADER_H

#include <chrono>
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <queue>


#ifdef _WIN32
#include <Windows.h>

typedef HANDLE file_desc_t;
typedef HANDLE proc_id_t;
#define INVALID_VALUE INVALID_HANDLE_VALUE
#else
#include<sys/types.h>
#include<unistd.h>

typedef int   file_desc_t;
typedef pid_t proc_id_t;
#define INVALID_VALUE -1
#endif

#define INVALID_FILEDESCRIPTOR INVALID_VALUE
#define INVALID_PID            INVALID_VALUE


struct ProcInfo {
  proc_id_t   pid;
  file_desc_t stdoutfd;
  file_desc_t stderrfd;
};


class StreamReader
{
public:
  static const unsigned int BUFSIZE = 4096;
  typedef std::function<bool(const std::string)> Callback;
  typedef std::function<std::string(file_desc_t)> ReadFunc;

  StreamReader();
  ~StreamReader();

  void        Start(ProcInfo pinfo);
  void        Stop();

  std::string PopItem();
  size_t      GetSize();
  void        Flush(Callback process);

  // platform specific method
  static std::string  AsyncRead(file_desc_t fd);

private:
  void        PushItem(std::string& s);
  void        ThreadFunc(ReadFunc func, file_desc_t fd);

  // Sync members
  std::mutex              m_mutex;
  std::promise<void>      m_prmStop;
  std::future<void>       m_futStop;
  std::thread             m_stdoutReaderThread;
  std::thread             m_stderrReaderThread;

  std::queue<std::string> m_queue;
};

#endif  /* #ifndef STREAMREADER_H */
