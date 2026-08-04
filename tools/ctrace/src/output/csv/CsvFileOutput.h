/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_OUTPUT_CSV_CSVFILEOUTPUT_H
#define CTRACE_SRC_OUTPUT_CSV_CSVFILEOUTPUT_H

#include "TraceEvent.h"
#include "TraceSelection.h"
#include "TraceOutput.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <ostream>
#include <string>

/** @brief Writes selected trace events directly to a CSV file. */
class CsvFileOutput final : public TraceOutput {
public:
  /** @brief Owns one CSV stream and provides its explicit close operation. */
  class Stream {
  public:
    /** @brief Allows destruction through the stream interface. */
    virtual ~Stream() = default;

    /** @brief Returns the stream receiving CSV rows. */
    virtual std::ostream& output() = 0;
    /** @brief Flushes and closes the stream. */
    virtual void close() = 0;
  };

  /**
   * @brief Creates an output stream for a validated CSV target path.
   * @param outputFile Final CSV path.
   * @return Owned, writable stream abstraction.
   */
  using StreamFactory = std::function<std::unique_ptr<Stream>(const std::filesystem::path&)>;

  /**
   * @brief Creates a CSV output for one target file.
   * @param outputFile Final CSV path.
   * @param selection Optional event and stream filters.
   */
  explicit CsvFileOutput(std::filesystem::path outputFile, TraceSelection selection = {});
  /**
   * @brief Creates a CSV output using an injected stream factory.
   * @param outputFile Final CSV path.
   * @param selection Optional event and stream filters.
   * @param streamFactory Factory used to open the target stream.
   */
  CsvFileOutput(std::filesystem::path outputFile, TraceSelection selection, StreamFactory streamFactory);
  /** @brief Closes an active stream without throwing. */
  ~CsvFileOutput() override;

  /** @brief Creates the target file and writes its header. */
  void start() override;
  /** @brief Flushes and closes the completed CSV file. */
  void stop() override;
  /** @brief Closes and removes an incomplete CSV file. */
  void abort() override;
  /**
   * @brief Writes one selected event as a CSV row.
   * @param event Event evaluated against the configured selection.
   */
  void writeEvent(const TraceEvent& event) override;
  /** @brief Returns the CSV backend name. */
  std::string_view backendName() const noexcept override;
  /** @brief Returns the CSV target file path. */
  std::string targetPath() const override;

private:
  std::filesystem::path m_outputFile;
  TraceSelection m_selection;
  StreamFactory m_streamFactory;
  std::unique_ptr<Stream> m_stream;
  bool m_active = false;
};

#endif  // CTRACE_SRC_OUTPUT_CSV_CSVFILEOUTPUT_H
