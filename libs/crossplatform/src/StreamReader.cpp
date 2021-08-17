/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "StreamReader.h"

StreamReader::StreamReader() {
  m_futStop = m_prmStop.get_future();
};

StreamReader::~StreamReader() {
  // wait for thread to finish
  if (m_stdoutReaderThread.joinable()) {
    Stop();
    m_stdoutReaderThread.join();
  }

  if (m_stderrReaderThread.joinable()) {
    m_stderrReaderThread.join();
  }

  // clear the queue
  std::queue<std::string>().swap(m_queue);
};

void StreamReader::Start(ProcInfo pinfo) {
  m_stdoutReaderThread = std::thread([=]() {
    ThreadFunc(StreamReader::AsyncRead, pinfo.stdoutfd);
  });

  m_stderrReaderThread = std::thread([=]() {
    ThreadFunc(StreamReader::AsyncRead, pinfo.stderrfd);
  });
}

void StreamReader::Stop() {
  m_prmStop.set_value();
}

void StreamReader::PushItem(std::string& s) {
  std::unique_lock<std::mutex> locker(m_mutex);
 
  m_queue.push(s);
  locker.unlock();

  s.clear();
}

std::string StreamReader::PopItem() {
  std::unique_lock<std::mutex> locker(m_mutex);
  std::string s;
  if (!m_queue.empty()) {
    s = m_queue.front();
    m_queue.pop();
  }
  locker.unlock();

  return s;
}

size_t StreamReader::GetSize() {
  std::unique_lock<std::mutex> locker(m_mutex, std::defer_lock);

  size_t size = m_queue.size();
  locker.unlock();

  return size;
}

void StreamReader::Flush(Callback process) {
  std::lock_guard<std::mutex> lock(m_mutex);

  while (!m_queue.empty()) {
    process(m_queue.front());
    m_queue.pop();
  }

  std::queue<std::string>().swap(m_queue);
}

void StreamReader::ThreadFunc(ReadFunc func, file_desc_t fd) {
  do {
    std::string item = func(fd);
    if (!item.empty()) {
      PushItem(item);
    }
  }
  while (m_futStop.wait_for(std::chrono::milliseconds(0)) == std::future_status::timeout);
}
