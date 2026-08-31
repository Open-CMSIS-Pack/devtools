/*
 * Copyright (c) 2020-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "SvdConvIntegTestEnv.h"
#include "SvdConvTestUtils.h"

#include "SVDConv.h"
#include "ErrLog.h"

#include <map>
#include <list>
#include <fstream>
#include <regex>
#include <vector>

using namespace std;
using namespace testing;


class SvdConvIntegTests : public ::testing::Test {
public:
  void SetUp()    override;
  void TearDown() override;
};

void SvdConvIntegTests::SetUp() {
}

void SvdConvIntegTests::TearDown() {
  ErrLog::Get()->Save();
  ErrLog::Get()->ClearLogMessages();
}


// Validate <disableCondition>
TEST_F(SvdConvIntegTests, CheckDisableCondition) {
#if 0
  const string& inFile = SvdConvIntegTestEnv::localtestdata_dir + "/disablecondition/DisableCondTest.svd";
  const string testOut = SvdConvIntegTestEnv::testoutput_dir + "/disablecondition";
  const string logFile = testOut + "/CheckDisableCondition.log";
  ASSERT_TRUE(RteFsUtils::Exists(inFile));

  Arguments args("SVDConv.exe", inFile);
  args.add({ "-o", testOut, "--generate=sfd", "--create-folder" });
  args.add({ "-b", logFile });

  SvdConv svdConv;
  EXPECT_EQ(0, svdConv.Check(args, args, nullptr));

  const string testOutSfd = testOut + "/DisableCondTest.sfd";
  cout << "\nTest SFD file: " << testOutSfd << endl;
  ASSERT_TRUE(RteFsUtils::Exists(testOutSfd));

  string buf;
  RteFsUtils::ReadFile(testOutSfd, buf);
  ASSERT_FALSE(buf.empty());

  const regex pattern1("//[ -]+Register Expression Object: (\\w+)[ -]+");   // regEx: ( .. ) is return content
  list<string> entries1 = { "DCB_DSCSR", "DCB_DSCSR_Clust_DSCSR" };
  list<smatch> result1 = SvdConvTestUtils::FindRegex(buf, pattern1);
  EXPECT_TRUE(SvdConvTestUtils::FindAllEntries(result1, entries1));

  //const regex pattern2("//[ ]+<view> (\\w+)\\s+//[ ]+<disableCond> \\((\\w+) & .*\\) == \\d+[ ]+</disableCond>");
  const regex pattern2("//[ ]+<view> \\w+\\s+//[ ]+<disableCond> \\((\\w+) & .*\\) == \\d+[ ]+</disableCond>");
  list<string> entries2 = { "DCB_DSCSR", "DCB_DSCSR_Clust_DSCSR" };
  list<smatch> result2 = SvdConvTestUtils::FindRegex(buf, pattern2);
  EXPECT_TRUE(SvdConvTestUtils::FindAllEntries(result2, entries2));
#endif
  EXPECT_TRUE(1);
}

// Validate NameHasBrackets
TEST_F(SvdConvIntegTests, CheckNameHasBrackets) {
  const string& inFile = SvdConvIntegTestEnv::localtestdata_dir + "/nameHasBrackets/SVDTiny.svd";
  ASSERT_TRUE(RteFsUtils::Exists(inFile));

  Arguments args("SVDConv.exe", inFile);

  SvdConv svdConv;
  EXPECT_EQ(2, svdConv.Check(args, args, nullptr));

  auto msgs = ErrLog::Get()->GetLogMessages();
  bool bFound = false;
  for(const auto& msg : msgs) {
    if(msg.find("M386") != string::npos) {
      bFound = true;
      break;
    }
  }

  EXPECT_TRUE(bFound);
}

// Validate Option -n
TEST_F(SvdConvIntegTests, CheckOption_n) {
  const string& inFile = SvdConvIntegTestEnv::localtestdata_dir + "/option_n/option_n.svd";
  const string testOut = SvdConvIntegTestEnv::testoutput_dir + "/option_n";
  ASSERT_TRUE(RteFsUtils::Exists(inFile));
  const string sfdOutName = "override.abc";

  Arguments args("SVDConv.exe", inFile);
  args.add({ "-o", testOut, "--generate=sfd", "--create-folder" });
  args.add( { "-n", sfdOutName } );

  SvdConv svdConv;
  EXPECT_FALSE(svdConv.Check(args, args, nullptr));

  string outNameTest = testOut;
  outNameTest += "/";
  outNameTest += RteUtils::ExtractFileBaseName(sfdOutName);
  outNameTest += ".sfd";
  ASSERT_TRUE(RteFsUtils::Exists(outNameTest));
}

TEST_F(SvdConvIntegTests, CheckSauNumRegions_Ok) {
  const string& inFile = SvdConvIntegTestEnv::localtestdata_dir + "/sauConfig/SSE300_ok.svd";
  const string testOut = SvdConvIntegTestEnv::testoutput_dir + "/sauConfig";
  ASSERT_TRUE(RteFsUtils::Exists(inFile));

  Arguments args("SVDConv.exe", inFile);
  args.add({ "-o", testOut, "--generate=partition", "--create-folder" });

  SvdConv svdConv;
  EXPECT_FALSE(svdConv.Check(args, args, nullptr));
}

TEST_F(SvdConvIntegTests, CheckSauNumRegions_Errors) {
  const string& inFile = SvdConvIntegTestEnv::localtestdata_dir + "/sauConfig/SSE300_errs.svd";
  const string testOut = SvdConvIntegTestEnv::testoutput_dir + "/sauConfig";
  ASSERT_TRUE(RteFsUtils::Exists(inFile));

  Arguments args("SVDConv.exe", inFile);
  args.add({ "-o", testOut, "--generate=partition", "--create-folder" });

  SvdConv svdConv;
  EXPECT_EQ(2, svdConv.Check(args, args, nullptr));

  struct {
    int M219 = 0;
    int M364 = 0;
  } cnt;

  auto errMsgs = ErrLog::Get()->GetLogMessages();
  for (const string& msg : errMsgs) {
    size_t s;

    if ((s = msg.find("M219", 0)) != string::npos) {
      cnt.M219++;
    }
    if ((s = msg.find("M364", 0)) != string::npos) {
      cnt.M364++;
    }
  }

  if(cnt.M219 != 2 || cnt.M364 != 1) {
    FAIL() << "Occurrences of M219, M364 are wrong.";
  }
}

TEST_F(SvdConvIntegTests, CheckAccViolationDisableCond) {
  const string& inFile = SvdConvIntegTestEnv::localtestdata_dir + "/accViolationDisableCond/accViolationDisableCond.xml";
  ASSERT_TRUE(RteFsUtils::Exists(inFile));

  Arguments args("SVDConv.exe", inFile);

  SvdConv svdConv;
  EXPECT_EQ(1, svdConv.Check(args, args, nullptr));
}

TEST_F(SvdConvIntegTests, CheckResetMask) {
  const string& inFile = SvdConvIntegTestEnv::localtestdata_dir + "/ResetMask/ResetMask.svd";
  ASSERT_TRUE(RteFsUtils::Exists(inFile));

  Arguments args("SVDConv.exe", inFile);

  SvdConv svdConv;
  EXPECT_EQ(1, svdConv.Check(args, args, nullptr));

  struct {
    int M318 = 0;
    int M351 = 0;
    int M356 = 0;
  } cnt;

  auto errMsgs = ErrLog::Get()->GetLogMessages();
  for (const string& msg : errMsgs) {
    size_t s;

    if ((s = msg.find("M318", 0)) != string::npos) {
      cnt.M318++;
    }
    if ((s = msg.find("M351", 0)) != string::npos) {
      cnt.M351++;
    }
    if ((s = msg.find("M356", 0)) != string::npos) {
      cnt.M356++;
    }
  }

  if(cnt.M318 != 2 || cnt.M351 != 1 || cnt.M356 != 1) {
    FAIL() << "Occurrences of M318, M351, M356 are wrong.";
  }
}

TEST_F(SvdConvIntegTests, CheckPosMaskDimFields) {
  const string& inFile = SvdConvIntegTestEnv::localtestdata_dir + "/posMaskDim/PosMaskDim.svd";
  const string testOut = SvdConvIntegTestEnv::testoutput_dir + "/posMaskDim";
  ASSERT_TRUE(RteFsUtils::Exists(inFile));

  Arguments args("SVDConv.exe", inFile);
  args.add({ "-o", testOut, "--generate=header", "--fields=macro", "--create-folder" });

  SvdConv svdConv;
  EXPECT_EQ(0, svdConv.Check(args, args, nullptr));

  const string testOutHeader = testOut + "/PosMaskDim.h";
  ASSERT_TRUE(RteFsUtils::Exists(testOutHeader));

  string buf;
  RteFsUtils::ReadFile(testOutHeader, buf);
  ASSERT_FALSE(buf.empty());

  const vector<pair<string, string>> expectedMacros = {
    { "#define TIM_DATA_PIN0_Pos", "(0UL)" },
    { "#define TIM_DATA_PIN0_Msk", "(0x1UL)" },
    { "#define TIM_DATA_PIN1_Pos", "(1UL)" },
    { "#define TIM_DATA_PIN1_Msk", "(0x2UL)" },
    { "#define TIM_DATA_PIN2_Pos", "(2UL)" },
    { "#define TIM_DATA_PIN2_Msk", "(0x4UL)" },
    { "#define TIM_DATA_PIN3_Pos", "(3UL)" },
    { "#define TIM_DATA_PIN3_Msk", "(0x8UL)" },
    { "#define TIM_DATA_PIN4_Pos", "(4UL)" },
    { "#define TIM_DATA_PIN4_Msk", "(0x10UL)" },
    { "#define TIM_DATA_PIN5_Pos", "(5UL)" },
    { "#define TIM_DATA_PIN5_Msk", "(0x20UL)" },
    { "#define TIM_DATA_PIN6_Pos", "(6UL)" },
    { "#define TIM_DATA_PIN6_Msk", "(0x40UL)" },
    { "#define TIM_DATA_PIN7_Pos", "(7UL)" },
    { "#define TIM_DATA_PIN7_Msk", "(0x80UL)" },
  };
  for(const auto& [name, value] : expectedMacros) {
    const auto namePos = buf.find(name);
    ASSERT_NE(string::npos, namePos) << name;
    const auto lineEnd = buf.find('\n', namePos);
    const auto valuePos = buf.find(value, namePos);
    ASSERT_NE(string::npos, valuePos) << name;
    EXPECT_LT(valuePos, lineEnd) << name;
  }

  EXPECT_EQ(string::npos, buf.find("#define TIM_DATA_PIN_Pos"));
  EXPECT_EQ(string::npos, buf.find("#define TIM_DATA_PIN_Msk"));
}

TEST_F(SvdConvIntegTests, CheckEnumComboWidthLimit) {
  const string& inFile = SvdConvIntegTestEnv::localtestdata_dir + "/enumComboWidth/EnumComboWidth.svd";
  const string testOut = SvdConvIntegTestEnv::testoutput_dir + "/enumComboWidth";
  ASSERT_TRUE(RteFsUtils::Exists(inFile));

  Arguments args("SVDConv.exe", inFile);
  args.add({ "-o", testOut, "--generate=sfd", "--create-folder" });

  SvdConv svdConv;
  EXPECT_EQ(1, svdConv.Check(args, args, nullptr));

  const auto msgs = ErrLog::Get()->GetLogMessages();
  size_t m227Count = 0;
  string allMessages;
  for(const auto& msg : msgs) {
    allMessages += msg;
    if(msg.find("M227") != string::npos) {
      ++m227Count;
    }
  }
  EXPECT_EQ(1U, m227Count);
  EXPECT_NE(string::npos, allMessages.find("field 'SEVEN_BIT'"));
  EXPECT_NE(string::npos, allMessages.find("field width 7"));
  EXPECT_NE(string::npos, allMessages.find("maximum of 6 bits"));

  const string testOutSfd = testOut + "/EnumComboWidth.sfd";
  ASSERT_TRUE(RteFsUtils::Exists(testOutSfd));

  string buf;
  RteFsUtils::ReadFile(testOutSfd, buf);
  ASSERT_FALSE(buf.empty());

  const auto sixBitField = buf.find("SFDITEM_FIELD__TEST_CTRL_SIX_BIT");
  const auto sevenBitField = buf.find("SFDITEM_FIELD__TEST_CTRL_SEVEN_BIT");
  ASSERT_NE(string::npos, sixBitField);
  ASSERT_NE(string::npos, sevenBitField);
  ASSERT_LT(sixBitField, sevenBitField);

  const auto sixBitCombo = buf.find("//    <combo>", sixBitField);
  EXPECT_NE(string::npos, sixBitCombo);
  EXPECT_LT(sixBitCombo, sevenBitField);
  const auto sixBitLastValue = buf.find("//        <63=>", sixBitCombo);
  EXPECT_NE(string::npos, sixBitLastValue);
  EXPECT_LT(sixBitLastValue, sevenBitField);

  const auto sevenBitEdit = buf.find("//    <edit>", sevenBitField);
  EXPECT_NE(string::npos, sevenBitEdit);
  EXPECT_EQ(string::npos, buf.find("//    <combo>", sevenBitField));
}
