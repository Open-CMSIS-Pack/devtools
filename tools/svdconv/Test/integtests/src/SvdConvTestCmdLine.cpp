/*
 * Copyright (c) 2020-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "SvdConvIntegTestEnv.h"
#include "SvdConvTestUtils.h"

#include "SVDConv.h"
#include "ParseOptions.h"
#include "ErrLog.h"

#include <map>
#include <list>
#include <fstream>
#include <regex>

using namespace std;
using namespace testing;


class SvdConvIntegTestsCmdParser : public ::testing::Test {
public:
  void SetUp()    override;
  void TearDown() override;
};

void SvdConvIntegTestsCmdParser::SetUp() {
}

void SvdConvIntegTestsCmdParser::TearDown() {
  ErrLog::Get()->Save();
  ErrLog::Get()->ClearLogMessages();
}


TEST_F(SvdConvIntegTestsCmdParser, CheckInputFile) {
  const string& inFile = SvdConvIntegTestEnv::localtestdata_dir + "/cmdlineParser/DisableCondTest.svd";

  Arguments args("SVDConv.exe", inFile);

  SvdOptions svdOptions;
  ParseOptions parseOptions(svdOptions);
  ParseOptions::Result result = parseOptions.Parse(args, args);
  EXPECT_EQ(ParseOptions::Result::Ok, result);

  string checkStr = svdOptions.GetSvdFullpath();
  EXPECT_EQ(inFile, checkStr);
}

TEST_F(SvdConvIntegTestsCmdParser, CheckOutputDirectory) {
  const string& outDirFile = SvdConvIntegTestEnv::localtestdata_dir + "/cmdlineParser/outputDir";

  Arguments args("SVDConv.exe");
  args.add({ "-o", outDirFile });
  args.add({ "--create-folder" });

  SvdOptions svdOptions;
  ParseOptions parseOptions(svdOptions);
  ParseOptions::Result result = parseOptions.Parse(args, args);
  EXPECT_EQ(ParseOptions::Result::Ok, result);

  string checkStr = svdOptions.GetOutputDirectory();
  EXPECT_EQ(outDirFile, checkStr);
}

TEST_F(SvdConvIntegTestsCmdParser, CheckLogfile) {
  const string& inFile = SvdConvIntegTestEnv::localtestdata_dir + "/cmdlineParser/DisableCondTest.svd";
  const string testOut = SvdConvIntegTestEnv::testoutput_dir + "/checkLogfile";
  const string logFile = testOut + "/checkLogfile.log";

  Arguments args("SVDConv.exe", inFile);
  args.add({ "-b", logFile });
  args.add({ "--create-folder" });

  SvdConv svdConv;
  svdConv.Check(args, args, nullptr);

  ErrLog::Get()->Save();
  bool bOk = RteFsUtils::Exists(logFile);
  EXPECT_TRUE(bOk);
}

TEST_F(SvdConvIntegTestsCmdParser, CheckGenerate) {
  Arguments args("SVDConv.exe");
  args.add({ "--generate=header" });
  args.add({ "--generate=partition" });
  args.add({ "--generate=peripheralMap" });
  args.add({ "--generate=registerMap" });
  args.add({ "--generate=fieldMap" });
  args.add({ "--generate=sfd" });
  args.add({ "--generate=sfr" });

  SvdOptions svdOptions;
  ParseOptions parseOptions(svdOptions);
  ParseOptions::Result result = parseOptions.Parse(args, args);
  EXPECT_EQ(ParseOptions::Result::Ok, result);

  EXPECT_TRUE(svdOptions.IsGenerateHeader());
  EXPECT_TRUE(svdOptions.IsGeneratePartition());
  EXPECT_TRUE(svdOptions.IsGenerateMapPeripheral());
  EXPECT_TRUE(svdOptions.IsGenerateMapRegister());
  EXPECT_TRUE(svdOptions.IsGenerateMapField());
  EXPECT_TRUE(svdOptions.IsGenerateSfd());
  EXPECT_TRUE(svdOptions.IsGenerateSfr());
}

#if 0 // template
TEST_F(SvdConvIntegTestsCmdParser, CheckFoo) {
  Arguments args("SVDConv.exe");
  args.add({ "" });

  SvdOptions svdOptions;
  ParseOptions parseOptions(svdOptions);
  ParseOptions::Result result = parseOptions.Parse(args, args);
  EXPECT_EQ(ParseOptions::Result::Ok, result);

  EXPECT_TRUE(svdOptions.IsFoo());
}
#endif

TEST_F(SvdConvIntegTestsCmdParser, CheckFields) {
  Arguments args("SVDConv.exe");
  args.add({ "--fields=macro" });
  args.add({ "--fields=struct" });
  args.add({ "--fields=struct-ansic" });
  args.add({ "--fields=enum" });

  SvdOptions svdOptions;
  ParseOptions parseOptions(svdOptions);
  ParseOptions::Result result = parseOptions.Parse(args, args);
  EXPECT_EQ(ParseOptions::Result::Ok, result);

  EXPECT_TRUE(svdOptions.IsCreateFields());
  EXPECT_TRUE(svdOptions.IsCreateFieldsAnsiC());
  EXPECT_TRUE(svdOptions.IsCreateMacros());
  EXPECT_TRUE(svdOptions.IsCreateEnumValues());
}

TEST_F(SvdConvIntegTestsCmdParser, CheckDebug) {
  Arguments args("SVDConv.exe");
  args.add({ "--debug=struct" });
  args.add({ "--debug=header" });
  args.add({ "--debug=sfd" });

  SvdOptions svdOptions;
  ParseOptions parseOptions(svdOptions);
  ParseOptions::Result result = parseOptions.Parse(args, args);
  EXPECT_EQ(ParseOptions::Result::Ok, result);

  EXPECT_TRUE(svdOptions.IsDebugStruct());
  EXPECT_TRUE(svdOptions.IsDebugHeaderfile());
  EXPECT_TRUE(svdOptions.IsDebugSfd());
}

TEST_F(SvdConvIntegTestsCmdParser, CheckSuppresPath) {
  Arguments args("SVDConv.exe");
  args.add({ "--suppress-path" });

  SvdOptions svdOptions;
  ParseOptions parseOptions(svdOptions);
  ParseOptions::Result result = parseOptions.Parse(args, args);
  EXPECT_EQ(ParseOptions::Result::Ok, result);

  EXPECT_TRUE(svdOptions.IsSuppressPath());
}

TEST_F(SvdConvIntegTestsCmdParser, CheckCreateFolder) {
  Arguments args("SVDConv.exe");
  args.add({ "--create-folder" });

  SvdOptions svdOptions;
  ParseOptions parseOptions(svdOptions);
  ParseOptions::Result result = parseOptions.Parse(args, args);
  EXPECT_EQ(ParseOptions::Result::Ok, result);

  EXPECT_TRUE(svdOptions.IsCreateFolder());
}

TEST_F(SvdConvIntegTestsCmdParser, CheckShowMissingEnums) {
  Arguments args("SVDConv.exe");
  args.add({ "--show-missingEnums" });

  SvdOptions svdOptions;
  ParseOptions parseOptions(svdOptions);
  ParseOptions::Result result = parseOptions.Parse(args, args);
  EXPECT_EQ(ParseOptions::Result::Ok, result);

  EXPECT_TRUE(svdOptions.IsShowMissingEnums());
}

#if 0   // done directly in ErrLog
TEST_F(SvdConvIntegTestsCmdParser, CheckStrict) {
  Arguments args("SVDConv.exe");
  args.add({ "--strict" });

  SvdOptions svdOptions;
  ParseOptions parseOptions(svdOptions);
  ParseOptions::Result result = parseOptions.Parse(args, args);
  EXPECT_EQ(ParseOptions::Result::Ok, result);

  EXPECT_TRUE(svdOptions.IsStrict());
}
#endif

#if 0   // done directly in ErrLog
TEST_F(SvdConvIntegTestsCmdParser, CheckSuppressWarnings) {
  Arguments args("SVDConv.exe");
  args.add({ "--suppress-warnings" });

  SvdOptions svdOptions;
  ParseOptions parseOptions(svdOptions);
  ParseOptions::Result result = parseOptions.Parse(args, args);
  EXPECT_EQ(ParseOptions::Result::Ok, result);

  EXPECT_TRUE(svdOptions.IsSuppressWarnings());
}
#endif

#if 0   // done directly in ErrLog
TEST_F(SvdConvIntegTestsCmdParser, CheckAllowSuppressError) {
  Arguments args("SVDConv.exe");
  args.add({ "--allow-suppress-error" });

  SvdOptions svdOptions;
  ParseOptions parseOptions(svdOptions);
  ParseOptions::Result result = parseOptions.Parse(args, args);
  EXPECT_EQ(ParseOptions::Result::Ok, result);

  EXPECT_TRUE(svdOptions.IsAllowSuppressError());
}
#endif

TEST_F(SvdConvIntegTestsCmdParser, CheckNoCleanup) {
  Arguments args("SVDConv.exe");
  args.add({ "--nocleanup" });

  SvdOptions svdOptions;
  ParseOptions parseOptions(svdOptions);
  ParseOptions::Result result = parseOptions.Parse(args, args);
  EXPECT_EQ(ParseOptions::Result::Ok, result);

  EXPECT_TRUE(svdOptions.IsNoCleanup());
}

TEST_F(SvdConvIntegTestsCmdParser, CheckUnderTest) {
  Arguments args("SVDConv.exe");
  args.add({ "--under-test" });

  SvdOptions svdOptions;
  ParseOptions parseOptions(svdOptions);
  ParseOptions::Result result = parseOptions.Parse(args, args);
  EXPECT_EQ(ParseOptions::Result::Ok, result);

  EXPECT_TRUE(svdOptions.IsUnderTest());
}

#if 0   // done directly in ErrLog
TEST_F(SvdConvIntegTestsCmdParser, CheckQuiet) {
  Arguments args("SVDConv.exe");
  args.add({ "--quiet" });

  SvdOptions svdOptions;
  ParseOptions parseOptions(svdOptions);
  ParseOptions::Result result = parseOptions.Parse(args, args);
  EXPECT_EQ(ParseOptions::Result::Ok, result);

  EXPECT_TRUE(svdOptions.IsQuiet());
}
#endif

TEST_F(SvdConvIntegTestsCmdParser, CheckSfdNameOverride) {
  Arguments args("SVDConv.exe");
  args.add({ "-n", "override.abc" });

  SvdOptions svdOptions;
  ParseOptions parseOptions(svdOptions);
  ParseOptions::Result result = parseOptions.Parse(args, args);
  EXPECT_EQ(ParseOptions::Result::Ok, result);

  const string& name = svdOptions.GetOutFilenameOverride();
  const string ext = RteUtils::ExtractFileExtension(name);

  EXPECT_TRUE(!name.empty());
  EXPECT_TRUE(ext.empty());
}
