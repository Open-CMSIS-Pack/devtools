/*
 * Copyright (c) 2020-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "SvdConvIntegTestEnv.h"
#include "SvdConvTestUtils.h"

#include "SVDConv.h"
#include "SfrccInterface.h"
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

TEST_F(SvdConvIntegTests, CheckSfdGeneration) {
  const string inFile = SvdConvIntegTestEnv::localtestdata_dir + "/option_n/option_n.svd";
  const string testOut = SvdConvIntegTestEnv::testoutput_dir + "/sfdGeneration";
  ASSERT_TRUE(RteFsUtils::Exists(inFile));

  Arguments args("SVDConv.exe", inFile);
  args.add({ "-o", testOut, "--generate=sfd", "--create-folder" });

  SvdConv svdConv;
  ASSERT_EQ(0, svdConv.Check(args, args, nullptr));

  string sfd;
  ASSERT_TRUE(RteFsUtils::ReadFile(testOut + "/option_n.sfd", sfd));
  EXPECT_FALSE(sfd.empty());
}

TEST_F(SvdConvIntegTests, GeneratesMemoryMapsAtRequestedDetailLevel) {
  const string inFile = SvdConvIntegTestEnv::localtestdata_dir + "/option_n/option_n.svd";
  const string testOut = SvdConvIntegTestEnv::testoutput_dir + "/memoryMaps";
  ASSERT_TRUE(RteFsUtils::Exists(inFile));

  struct MapMode {
    const char* option;
    const char* suffix;
    bool registers;
    bool fields;
  };
  const MapMode modes[] = {
    {"peripheralMap", "MapPeripherals", false, false},
    {"registerMap", "MapRegisters", true, false},
    {"fieldMap", "MapFields", true, true},
  };
  for(const auto& mode : modes) {
    SCOPED_TRACE(mode.option);
    Arguments args("SVDConv.exe", inFile);
    args.add({"-o", testOut, string("--generate=") + mode.option, "--create-folder"});
    SvdConv svdConv;
    ASSERT_EQ(0, svdConv.Check(args, args, nullptr));

    string listing;
    ASSERT_TRUE(RteFsUtils::ReadFile(testOut + "/option_n_" + mode.suffix + ".txt", listing));
    EXPECT_NE(string::npos, listing.find("Peripheral Map"));
    for(const auto* address : {"0x40003000", "0x40004000", "0xe000ee08"}) {
      EXPECT_NE(string::npos, listing.find(string("Base Address: ") + address));
    }
    EXPECT_NE(string::npos, listing.find("AddressBlock:"));
    EXPECT_TRUE(regex_search(listing, regex(R"(000\s+---)")));
    EXPECT_TRUE(regex_search(listing, regex(R"(001\s+---)")));
    EXPECT_TRUE(regex_search(listing, regex(R"(002\s+IWDG\s+IWDG Interrupt)")));

    // Derived peripherals and nested clusters must retain their resolved addresses.
    for(const auto* pattern : {
          R"(\bKR\b[^\n]*Address: 0x40003000,[^\n]*Access: wo)",
          R"(\bKR\b[^\n]*Address: 0x40004000,[^\n]*Access: wo)",
          R"(\bPR\b[^\n]*Address: 0x40003004,[^\n]*Offset: 0x00000004,[^\n]*Width: 4,[^\n]*Access: rw)",
          R"(\bDSCSR_Clust\b[^\n]*Address: 0xe000ee08)",
          R"(\bDSCSR\b[^\n]*Address: 0xe000ee08)",
          R"(\bDSCSR\b[^\n]*Address: 0xe000ee10)"}) {
      EXPECT_EQ(mode.registers, regex_search(listing, regex(pattern))) << pattern;
    }
    for(const auto* pattern : {
          R"(\bKEY\b[^\n]*\[15 \.\.\.  0\][^\n]*Bits: 16)",
          R"(\bCDS\b[^\n]*\[16 \.\.\. 16\][^\n]*Bits: 1)"}) {
      EXPECT_EQ(mode.fields, regex_search(listing, regex(pattern))) << pattern;
    }
  }
}

TEST_F(SvdConvIntegTests, ExpandsRegisterAndFieldDimensionsInMemoryMap) {
  const string inFile = SvdConvIntegTestEnv::localtestdata_dir + "/posMaskDim/PosMaskDim.svd";
  const string testOut = SvdConvIntegTestEnv::testoutput_dir + "/dimensionMemoryMap";
  ASSERT_TRUE(RteFsUtils::Exists(inFile));
  Arguments args("SVDConv.exe", inFile);
  args.add({"-o", testOut, "--generate=fieldMap", "--create-folder"});
  SvdConv svdConv;
  ASSERT_EQ(0, svdConv.Check(args, args, nullptr));

  string listing;
  ASSERT_TRUE(RteFsUtils::ReadFile(testOut + "/PosMaskDim_MapFields.txt", listing));
  const regex registers(R"(\bDATA(\d+)\b[^\n]*Address: 0x([0-9a-f]+))");
  unsigned count = 0;
  for(auto it = sregex_iterator(listing.begin(), listing.end(), registers); it != sregex_iterator(); ++it) {
    EXPECT_EQ(count, stoul((*it)[1].str()));
    EXPECT_EQ(0x40000000UL + count * 4U, stoul((*it)[2].str(), nullptr, 16));
    count++;
  }
  EXPECT_EQ(256U, count);
  for(unsigned bit = 0; bit < 8; bit++) {
    SCOPED_TRACE(bit);
    const string value = to_string(bit);
    const regex field("\\bPIN" + value + R"(\b[^\n]*\[ )" + value + R"( \.\.\.  )" + value +
                      R"(\][^\n]*Bits: 1)");
    const auto matches = SvdConvTestUtils::FindRegex(listing, field);
    EXPECT_EQ(256U, matches.size());
  }
}

#ifndef _WIN32
TEST_F(SvdConvIntegTests, CheckSfrUnsupportedPlatform) {
  const string inFile = SvdConvIntegTestEnv::localtestdata_dir + "/option_n/option_n.svd";
  const string testOut = SvdConvIntegTestEnv::testoutput_dir + "/sfrUnsupportedPlatform";
  ASSERT_TRUE(RteFsUtils::Exists(inFile));

  Arguments args("SVDConv.exe", inFile);
  args.add({ "-o", testOut, "--generate=sfr", "--create-folder" });

  SvdConv svdConv;
  EXPECT_EQ(2, svdConv.Check(args, args, nullptr));

  string messages;
  for(const auto& msg : ErrLog::Get()->GetLogMessages()) {
    messages += msg;
  }
  EXPECT_NE(string::npos, messages.find("M133"));
  EXPECT_NE(string::npos, messages.find("SFR generation is only supported on Windows (requires SfrCC2.exe)."));
  EXPECT_FALSE(RteFsUtils::Exists(testOut + "/option_n.sfr"));
}

TEST_F(SvdConvIntegTests, CheckSfrCompileUnsupportedPlatform) {
  SvdConv svdConv; // Initialize the diagnostic message table.
  SfrccInterface sfrcc;
  EXPECT_FALSE(sfrcc.Compile("unused.sfd"));
}
#endif

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

struct VtorHeaderCase {
  const char* name;
  const char* cpu;
  const char* xmlValue;
  const char* expectedValue;
};

class SvdConvVtorHeaderTests : public SvdConvIntegTests, public WithParamInterface<VtorHeaderCase> {
};

TEST_P(SvdConvVtorHeaderTests, CheckVtorPresent) {
  const auto& param = GetParam();
  const string testOut = SvdConvIntegTestEnv::testoutput_dir + "/vtorPresent/" + param.name;
  const string inFile = testOut + "/VtorPresence.svd";
  ASSERT_TRUE(RteFsUtils::CreateDirectories(testOut));

  const string xmlValue = param.xmlValue;
  const string vtorElement = xmlValue.empty() ? "" : "<vtorPresent>" + xmlValue + "</vtorPresent>";
  const string svd = string(R"(<?xml version="1.0" encoding="utf-8"?>
<device schemaVersion="1.3">
  <name>VtorPresence</name>
  <version>1.0</version>
  <description>VTOR header configuration test.</description>
  <cpu>
    <name>)") + param.cpu + R"(</name>
    <revision>r0p0</revision>
    <endian>little</endian>
    <mpuPresent>false</mpuPresent>
    <fpuPresent>false</fpuPresent>
    )" + vtorElement + R"(
    <nvicPrioBits>2</nvicPrioBits>
    <vendorSystickConfig>false</vendorSystickConfig>
  </cpu>
  <addressUnitBits>8</addressUnitBits>
  <width>32</width>
  <size>32</size>
  <access>read-write</access>
  <resetValue>0</resetValue>
  <resetMask>0xFFFFFFFF</resetMask>
  <peripherals>
    <peripheral>
      <name>TEST</name>
      <description>Test peripheral.</description>
      <baseAddress>0x40000000</baseAddress>
      <addressBlock><offset>0</offset><size>4</size><usage>registers</usage></addressBlock>
      <interrupt><name>TEST</name><description>Test interrupt.</description><value>0</value></interrupt>
      <registers>
        <register><name>DATA</name><description>Test data.</description><addressOffset>0</addressOffset></register>
      </registers>
    </peripheral>
  </peripherals>
</device>
)";
  ASSERT_TRUE(RteFsUtils::CreateTextFile(inFile, svd));

  Arguments args("SVDConv.exe", inFile);
  args.add({ "-o", testOut, "--generate=header" });
  SvdConv svdConv;
  ASSERT_EQ(0, svdConv.Check(args, args, nullptr));

  string header;
  ASSERT_TRUE(RteFsUtils::ReadFile(testOut + "/VtorPresence.h", header));
  ASSERT_FALSE(header.empty());
  const string expectedValue = param.expectedValue;
  if(expectedValue.empty()) {
    EXPECT_EQ(string::npos, header.find("__VTOR_PRESENT"));
  }
  else {
    const regex pattern(R"(#define[ \t]+__VTOR_PRESENT[ \t]+([0-9]+)\b)");
    const auto matches = SvdConvTestUtils::FindRegex(header, pattern);
    ASSERT_EQ(1U, matches.size());
    EXPECT_EQ(expectedValue, matches.front()[1].str());
  }
}

INSTANTIATE_TEST_SUITE_P(VtorPresent, SvdConvVtorHeaderTests,
  Values(
    VtorHeaderCase{ "CM33Default",    "CM33",    "",      "1" },
    VtorHeaderCase{ "CM0PLUSDefault", "CM0PLUS", "",      "1" },
    VtorHeaderCase{ "CM23Default",    "CM23",    "",      "1" },
    VtorHeaderCase{ "CM33True",       "CM33",    "true",  "1" },
    VtorHeaderCase{ "CM33False",      "CM33",    "false", "0" },
    VtorHeaderCase{ "CM33One",        "CM33",    "1",     "1" },
    VtorHeaderCase{ "CM33Zero",       "CM33",    "0",     "0" },
    VtorHeaderCase{ "CM0PLUSTrue",    "CM0PLUS", "true",  "1" },
    VtorHeaderCase{ "CM0PLUSFalse",   "CM0PLUS", "false", "0" },
    VtorHeaderCase{ "CM0PLUSOne",     "CM0PLUS", "1",     "1" },
    VtorHeaderCase{ "CM0PLUSZero",    "CM0PLUS", "0",     "0" },
    VtorHeaderCase{ "CM0NoMacro",     "CM0",     "",      ""  },
    VtorHeaderCase{ "CM1NoMacro",     "CM1",     "",      ""  },
    VtorHeaderCase{ "CM3NoMacro",     "CM3",     "",      ""  }
  ),
  [](const TestParamInfo<VtorHeaderCase>& info) { return info.param.name; }
);
