/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ParseOptions.h"
#include "SvdOptions.h"
#include "ErrLog.h"

#include "gtest/gtest.h"
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace std;

namespace {

class ScopedConsoleCapture {
public:
  ScopedConsoleCapture() :
    m_previousOutput(cout.rdbuf(m_output.rdbuf())),
    m_previousError(cerr.rdbuf(m_error.rdbuf())) {
  }

  ~ScopedConsoleCapture() {
    cout.rdbuf(m_previousOutput);
    cerr.rdbuf(m_previousError);
  }

  string Output() const { return m_output.str(); }
  string Error() const { return m_error.str(); }

private:
  ostringstream m_output;
  ostringstream m_error;
  streambuf* m_previousOutput;
  streambuf* m_previousError;
};

class SvdOptionsTest : public testing::Test {
protected:
  void SetUp() override {
    const auto log = ErrLog::Get();
    m_previousLevel = log->GetLevel();
    m_previousQuiet = log->IsQuietMode();
    m_previousStrict = log->IsStrictMode();
    m_previousAllowSuppressError = log->IsAllowSuppressError();
    m_previousErrors = log->GetErrCnt();
    m_previousWarnings = log->GetWarnCnt();
    log->SetLevel(MsgLevel::LEVEL_WARNING3);
    log->SetQuietMode(false);
    log->SetStrictMode(false);
    log->SetAllowSuppressError(false);
    log->ResetMsgCount();
  }

  void TearDown() override {
    const auto log = ErrLog::Get();
    log->SetLevel(m_previousLevel);
    log->SetQuietMode(m_previousQuiet);
    log->SetStrictMode(m_previousStrict);
    log->SetAllowSuppressError(m_previousAllowSuppressError);
    log->ResetMsgCount();
    for(int i = 0; i < m_previousErrors; i++) {
      log->IncErrCnt();
    }
    for(int i = 0; i < m_previousWarnings; i++) {
      log->IncWarnCnt();
    }
  }

  ParseOptions::Result Parse(initializer_list<const char*> options) {
    vector<const char*> args{"SVDConv"};
    args.insert(args.end(), options.begin(), options.end());
    ParseOptions parser(m_options);
    ScopedConsoleCapture capture;
    const auto result = parser.Parse(static_cast<int>(args.size()), args.data());
    m_output = capture.Output();
    m_error = capture.Error();
    return result;
  }

  SvdOptions m_options;
  string m_output;
  string m_error;

private:
  MsgLevel m_previousLevel = MsgLevel::LEVEL_WARNING3;
  bool m_previousQuiet = false;
  bool m_previousStrict = false;
  bool m_previousAllowSuppressError = false;
  int m_previousErrors = 0;
  int m_previousWarnings = 0;
};

TEST_F(SvdOptionsTest, TokenizesOptionFileLinesWithoutFilesystemAccess) {
  const vector<pair<string, vector<string>>> cases{
    {"", {}},
    {" \t\r ", {}},
    {"# comment", {}},
    {"  --generate=header\t--fields=enum  ", {"--generate=header", "--fields=enum"}},
    {"--generate=header # ignored option", {"--generate=header"}},
    {"--generate=header#comment", {"--generate=header"}},
  };
  ParseOptions parser(m_options);
  for(const auto& [line, expected] : cases) {
    SCOPED_TRACE(line);
    vector<string> options;
    EXPECT_EQ(ParseOptions::Result::Ok, parser.ParseOptsFileLine(line, options));
    EXPECT_EQ(expected, options);
  }

  for(const auto* value : {"my header", "my # header"}) {
    SCOPED_TRACE(value);
    vector<string> options;
    const string line = string("-n \"") + value + "\" # comment";
    EXPECT_EQ(ParseOptions::Result::Ok, parser.ParseOptsFileLine(line, options));
    ASSERT_EQ(2U, options.size());
    EXPECT_EQ("-n", options[0]);
    EXPECT_TRUE(options[1] == value || options[1] == string("\"") + value + "\"") << options[1];
  }
}

TEST_F(SvdOptionsTest, AppendsOptionFileTokensToExistingArguments) {
  ParseOptions parser(m_options);
  vector<string> options{"SVDConv", "--generate=header"};
  EXPECT_EQ(ParseOptions::Result::Ok, parser.ParseOptsFileLine("--fields=enum", options));
  EXPECT_EQ((vector<string>{"SVDConv", "--generate=header", "--fields=enum"}), options);
}

TEST_F(SvdOptionsTest, ShowsHelpWithoutStartingConversion) {
  for(const auto& arguments : {initializer_list<const char*>{}, {"--help"}, {"-h"}}) {
    EXPECT_EQ(ParseOptions::Result::ExitNoError, Parse(arguments));
    EXPECT_NE(string::npos, m_output.find("--generate"));
    EXPECT_NE(string::npos, m_output.find("--fields"));
    EXPECT_TRUE(m_error.empty());
    EXPECT_TRUE(m_options.GetSvdFullpath().empty());
  }
}

TEST_F(SvdOptionsTest, ShowsVersionWithoutStartingConversion) {
  const auto version = m_options.GetVersion();
  ASSERT_FALSE(version.empty());
  for(const auto* option : {"--version", "-V"}) {
    SCOPED_TRACE(option);
    EXPECT_EQ(ParseOptions::Result::ExitNoError, Parse({option}));
    EXPECT_NE(string::npos, m_output.find(version));
    EXPECT_TRUE(m_error.empty());
    EXPECT_TRUE(m_options.GetSvdFullpath().empty());
  }
}

TEST_F(SvdOptionsTest, RejectsUnknownOptionsAndMissingOptionValues) {
  for(const auto* option : {"--not-an-option", "--generate", "--fields", "-o"}) {
    SCOPED_TRACE(option);
    EXPECT_EQ(ParseOptions::Result::Error, Parse({option}));
    EXPECT_FALSE(m_error.empty());
    EXPECT_TRUE(m_output.empty());
  }
}

TEST_F(SvdOptionsTest, AppliesExplicitWarningLevels) {
  const vector<pair<const char*, MsgLevel>> cases{
    {"-w0", MsgLevel::LEVEL_ERROR},
    {"-w1", MsgLevel::LEVEL_WARNING},
    {"-w2", MsgLevel::LEVEL_WARNING2},
    {"-w3", MsgLevel::LEVEL_WARNING3},
    {"-wall", MsgLevel::LEVEL_WARNING3},
  };
  for(const auto& [option, expected] : cases) {
    SCOPED_TRACE(option);
    ErrLog::Get()->SetLevel(MsgLevel::LEVEL_PROGRESS);
    EXPECT_EQ(ParseOptions::Result::Ok, Parse({option}));
    EXPECT_EQ(expected, ErrLog::Get()->GetLevel());
    EXPECT_TRUE(m_error.empty());
  }
}

TEST_F(SvdOptionsTest, EnablesVerboseDiagnostics) {
  EXPECT_EQ(ParseOptions::Result::Ok, Parse({"--verbose"}));
  EXPECT_EQ(MsgLevel::LEVEL_PROGRESS, ErrLog::Get()->GetLevel());
}

TEST_F(SvdOptionsTest, SuppressesWarningDiagnostics) {
  EXPECT_EQ(ParseOptions::Result::Ok, Parse({"--suppress-warnings"}));
  EXPECT_EQ(MsgLevel::LEVEL_ERROR, ErrLog::Get()->GetLevel());
}

TEST_F(SvdOptionsTest, EnablesQuietMode) {
  EXPECT_EQ(ParseOptions::Result::Ok, Parse({"--quiet"}));
  EXPECT_TRUE(ErrLog::Get()->IsQuietMode());
}

TEST_F(SvdOptionsTest, EnablesStrictMode) {
  EXPECT_EQ(ParseOptions::Result::Ok, Parse({"--strict"}));
  EXPECT_TRUE(ErrLog::Get()->IsStrictMode());
}

TEST_F(SvdOptionsTest, AllowsErrorSuppressionWithoutChangingSuppressionSets) {
  EXPECT_EQ(ParseOptions::Result::Ok, Parse({"--allow-suppress-error"}));
  EXPECT_TRUE(ErrLog::Get()->IsAllowSuppressError());
}

TEST_F(SvdOptionsTest, RejectsEmptyLogAndDiagnosticSuppressionArguments) {
  EXPECT_FALSE(m_options.SetLogFile(""));
  EXPECT_TRUE(m_options.GetLogPath().empty());
  EXPECT_FALSE(m_options.AddDiagSuppress(""));
}

TEST_F(SvdOptionsTest, DoesNotChangeWarningLevelWhenVerboseIsDisabled) {
  ErrLog::Get()->SetLevel(MsgLevel::LEVEL_WARNING2);
  EXPECT_TRUE(m_options.SetVerbose(false));
  EXPECT_EQ(MsgLevel::LEVEL_WARNING2, ErrLog::Get()->GetLevel());
}

} // namespace
