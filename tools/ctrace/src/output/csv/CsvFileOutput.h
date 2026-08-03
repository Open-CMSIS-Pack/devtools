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
#include <fstream>
#include <string>

class CsvFileOutput final : public TraceOutput {
public:
  explicit CsvFileOutput(std::filesystem::path outputFile, TraceSelection selection = {});
  ~CsvFileOutput() override;

  void start() override;
  void stop() override;
  void abort() override;
  void writeEvent(const TraceEvent& event) override;
  std::string_view backendName() const noexcept override;
  std::string targetPath() const override;

private:
  std::filesystem::path outputFile_;
  TraceSelection selection_;
  std::ofstream stream_;
  bool active_ = false;
};

#endif  // CTRACE_SRC_OUTPUT_CSV_CSVFILEOUTPUT_H
