/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "SvdConvIntegTestEnv.h"
#include "SvdConvTestUtils.h"
#include "SVDConv.h"
#include "ErrLog.h"

#include <cstdint>
#include <initializer_list>
#include <regex>
#include <string>
#include <utility>

using namespace std;

namespace {

class SvdConvEnumHeaderTests : public testing::Test {
protected:
  void SetUp() override {
    ErrLog::Get()->ClearLogMessages();
  }

  void TearDown() override {
    ErrLog::Get()->Save();
    ErrLog::Get()->ClearLogMessages();
  }

  void GenerateHeader(const string& fixture, const string& name, string& header) {
    const string input = SvdConvIntegTestEnv::localtestdata_dir + "/" + fixture;
    const string output = SvdConvIntegTestEnv::testoutput_dir + "/enumHeader/" + name;
    ASSERT_TRUE(RteFsUtils::Exists(input));
    Arguments args("SVDConv.exe", input);
    args.add({"-o", output, "--generate=header", "--fields=enum", "--create-folder"});
    SvdConv svdConv;
    ASSERT_EQ(0, svdConv.Check(args, args, nullptr));

    for(const auto& message : ErrLog::Get()->GetLogMessages()) {
      EXPECT_EQ(string::npos, message.find("M227")) << message;
    }
    ASSERT_TRUE(RteFsUtils::ReadFile(output + "/" + name + ".h", header));
    ASSERT_FALSE(header.empty());
  }

  static void ExpectEnumeration(const string& header, const string& type,
                                initializer_list<pair<const char*, uint32_t>> values) {
    const regex declaration(R"(typedef\s+enum\s*\{([^}]+)\}\s*)" + type + R"(\s*;)");
    const auto types = SvdConvTestUtils::FindRegex(header, declaration);
    ASSERT_EQ(1U, types.size()) << type;
    const string body = types.front()[1].str();
    for(const auto& [name, expected] : values) {
      SCOPED_TRACE(name);
      const regex assignment(string(R"(\b)") + name + R"(\s*=\s*(0[xX][0-9a-fA-F]+|[0-9]+)[uUlL]*\s*,)");
      const auto entries = SvdConvTestUtils::FindRegex(body, assignment);
      ASSERT_EQ(1U, entries.size());
      EXPECT_EQ(expected, stoul(entries.front()[1].str(), nullptr, 0));
    }
  }
};

TEST_F(SvdConvEnumHeaderTests, FieldEnumsPreserveValuesWithoutBitOffsetShift) {
  string header;
  ASSERT_NO_FATAL_FAILURE(GenerateHeader("enumComboWidth/EnumComboWidth.svd", "EnumComboWidth", header));

  ExpectEnumeration(header, "TEST_CTRL_SIX_BIT_Enum", {{"TEST_CTRL_SIX_BIT_ONE", 1U}});
  // SEVEN_BIT starts at bit 8, but its enum value is the field value, not a register mask.
  ExpectEnumeration(header, "TEST_CTRL_SEVEN_BIT_Enum", {{"TEST_CTRL_SEVEN_BIT_NINE", 9U}});
}

TEST_F(SvdConvEnumHeaderTests, ClusterAndDerivedRegisterEnumsHaveDistinctTypesAndValues) {
  string header;
  ASSERT_NO_FATAL_FAILURE(GenerateHeader("option_n/option_n.svd", "option_n", header));

  ExpectEnumeration(header, "DCB_DSCSR_Clust_DSCSR_CDS_Enum",
    {{"DCB_DSCSR_Clust_DSCSR_CDS_Disable", 0U}, {"DCB_DSCSR_Clust_DSCSR_CDS_Enable", 1U}});
  ExpectEnumeration(header, "DCB_DSCSR_CDS_Enum",
    {{"DCB_DSCSR_CDS_Disable", 0U}, {"DCB_DSCSR_CDS_Enable", 1U}});
}

} // namespace
