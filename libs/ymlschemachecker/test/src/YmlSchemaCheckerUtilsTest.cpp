/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "YmlSchemaCheckerUtils.h"
#include "YmlSchemaChkTestEnv.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using nlohmann::json;

class YmlSchemaCheckerUtilsTest : public ::testing::Test {
public:
  YmlSchemaCheckerUtilsTest() {}
  virtual ~YmlSchemaCheckerUtilsTest() {}
};

struct SuffixTestCase {
  std::string input;
  bool expectSuccess;
  json expectedValue;
};

TEST(YmlSchemaCheckerUtilsTest, ConvertSuffixedHexString_HexAndDecimal) {
  std::vector<SuffixTestCase> inputs = {
    // No suffix
    {"1234", true, 1234},
    {"0x1324", true, 4900},

    // Unsigned (U)
    {"1234U", true, 1234},
    {"0x1234U", true, 4660},

    // Long (L)
    {"1234L", true, 1234},
    {"0x1234L", true, 4660},

    // Unsigned Long (UL)
    {"1234UL", true, 1234},
    {"0x1234UL", true, 4660},

    // Long Unsigned (LU)
    {"1234LU", true, 1234},
    {"0x1234LU", true, 4660},

    // Long Long (LL)
    {"1234LL", true, 1234},
    {"0x1234LL", true, 4660},

    // Unsigned Long Long (ULL)
    {"1234ULL", true, 1234},
    {"0x1234ULL", true, 4660},

    // Long Long Unsigned (LLU)
    {"1234LLU", true, 1234},
    {"0x1234LLU", true, 4660},

    // 32-bit boundaries
    {"0x7FFFFFFF", true, 2147483647},
    {"0x7FFFFFFFU", true, 2147483647},
    {"0x80000000", true, 2147483648ULL},
    {"0x80000000U", true, 2147483648ULL},

    // 64-bit boundaries
    {"0xFFFFFFFFFFFFFFFFULL", true, 18446744073709551615ULL},
    // This value exceeds uint64_t, so expect failure and string fallback
    {"0x10000000000000000ULL", false, "0x10000000000000000ULL"},

    // Signed decimal values
    {"-1234", true, -1234},
    {"-1234L", true, -1234},
    {"-1234UL", true, -1234},
    {"-1234LL", true, -1234},

    // Leading zeros
    {"0001234U", true, 1234},
    {"0x00001234UL", true, 4660},

    // Invalid hex digits
    {"0x12G4U", false, "0x12G4U"},
    {"0xZZZZUL", false, "0xZZZZUL"},

    // Invalid suffix combinations
    {"1234UU", false, "1234UU"},
    {"1234ULU", false, "1234ULU"},
    {"1234LUL", false, "1234LUL"},
    {"1234XYZ", false, "1234XYZ"},

    // Suffix without a number
    {"U", false, "U"},
    {"0xUL", false, "0xUL"},

    // Mixed or malformed tokens
    {"0x1234_UL", false, "0x1234_UL"},
    {"1234-UL", false, "1234-UL"},
    {"0x1234+U", false, "0x1234+U"},

    // Float-like (not integers)
    {"12.34U", false, "12.34U"},
    {"0x12.34UL", false, "0x12.34UL"},

    // Embedded / symbolic text
    {"value_0x1234UL", false, "value_0x1234UL"},

    // Duplicate invalid-value tests (explicit)
    {"0x12G4U", false, "0x12G4U"},
    {"1234ULU", false, "1234ULU"}
  };

  for (const auto& testInput : inputs) {
    json j = testInput.input;
    EXPECT_EQ(YmlSchemaCheckerUtils::ConvertSuffixedHexString(j), testInput.expectSuccess) << "Input: " << testInput.input;
    EXPECT_EQ(j, testInput.expectedValue) << "Input: " << testInput.input;
  }
}

TEST(YmlSchemaCheckerUtilsTest, SanitizeJsonForNumericSuffixes_Recursive) {
  json j = {
    {"hex", "0x10UL"},
    {"dec", "1234U"},
    {"arr", {"0x2A", "-99L", "notnum"}},
    {"nested", {{"val", "0xFfU"}}}
  };

  YmlSchemaCheckerUtils::SanitizeJsonForNumericSuffixes(j);
  EXPECT_EQ(j["hex"], 16);
  EXPECT_EQ(j["dec"], 1234);
  EXPECT_EQ(j["arr"][0], 42);
  EXPECT_EQ(j["arr"][1], -99);
  EXPECT_EQ(j["arr"][2], "notnum");
  EXPECT_EQ(j["nested"]["val"], 255);
}
