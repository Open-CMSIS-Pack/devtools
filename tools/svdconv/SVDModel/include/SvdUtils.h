/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SvdUtils_H
#define SvdUtils_H

#include <cstdint>
#include <string>
#include <list>
#include <set>
#include <map>

#include "SvdTypes.h"


// Only available with SVDConv!
#define BITRANGE(msb, lsb, addWidth)    std::make_pair("BITRANGE",  SvdUtils::CreateFieldRange(msb, lsb, addWidth))
#define BITRANGE2(msb, lsb, addWidth)   std::make_pair("BITRANGE2", SvdUtils::CreateFieldRange(msb, lsb, addWidth))
#define ADDR(addr)                      std::make_pair("ADDR",      SvdUtils::CreateAddress(addr, (uint32_t)-1))
#define ADDR2(addr)                     std::make_pair("ADDR2",     SvdUtils::CreateAddress(addr, (uint32_t)-1))
#define ADDRSIZE(addr, size)            std::make_pair("ADDRSIZE",  SvdUtils::CreateAddress(addr, size))
#define ADDRSIZE2(addr, size)           std::make_pair("ADDRSIZE2", SvdUtils::CreateAddress(addr, size))
#define HEXNUM(num)                     std::make_pair("HEXNUM",    SvdUtils::CreateHexNum (num))
#define HEXNUM2(num)                    std::make_pair("HEXNUM2",   SvdUtils::CreateHexNum (num))
#define LINE2(num)                      std::make_pair("LINE",      SvdUtils::CreateLineNum (num))


// Configuration
#define MAX_BITWIDTH_FOR_COMBO        6


class SvdUtils {
  private:
    SvdUtils(){};
  public:

  static const std::string EMPTY_STRING;


  static std::string              SpacesToUnderscore              (const std::string& s);
  static std::string              SlashesToBackSlashes            (const std::string& fileName);
  static SvdTypes::Expression     ParseExpression                 (const std::string &expr, std::string &name, uint32_t &insertPos);

  static bool                     TrimWhitespace                  (std::string &name);
  static bool                     ToUpper                         (std::string &text);
  static bool                     ToLower                         (std::string &text);

  static bool                     ConvertNumber                   (const std::string &text, bool& value);
  static bool                     ConvertNumber                   (const std::string &text, std::set<uint64_t>& numbers);
  static bool                     ConvertNumber                   (const std::string &text, uint64_t& number, uint32_t base);
  static bool                     ConvertNumber                   (const std::string &text, uint64_t& number);
  static bool                     ConvertNumber                   (const std::string &text, int32_t& number);
  static bool                     ConvertNumber                   (const std::string &text, uint32_t& number);
  static bool                     ConvertNumberXBin               (const std::string &text, std::set<uint32_t>& numbers);

  static bool                     ConvertBitRange                 (const std::string &text, uint32_t& m_offset, uint32_t& m_size);
  static bool                     DoConvertNumberXBin             (const std::string &text, uint32_t value, std::set<uint32_t>& numbers);
  static bool                     ConvertCpuRevision              (const std::string &text, uint32_t& rev);
  static bool                     ConvertProtectionStringType     (const std::string &text, SvdTypes::ProtectionType& protection, uint32_t lineNo);
  static bool                     ConvertSauProtectionStringType  (const std::string &text, SvdTypes::ProtectionType& protection, uint32_t lineNo);
  static bool                     ConvertSauAccessType            (const std::string &text, SvdTypes::SauAccessType& accessType,  uint32_t lineNo);

  static bool                     ConvertAccess                   (const std::string &text, SvdTypes::Access& acc, uint32_t lineNo);
  static bool                     ConvertAddrBlockUsage           (const std::string &text, SvdTypes::AddrBlockUsage& usage, uint32_t lineNo);
  static bool                     ConvertEnumUsage                (const std::string &text, SvdTypes::EnumUsage& usage, uint32_t lineNo);
  static bool                     ConvertCpuType                  (const std::string &text, SvdTypes::CpuType& cpuType);
  static bool                     ConvertCpuEndian                (const std::string &text, SvdTypes::Endian& endian, uint32_t lineNo);
  static bool                     ConvertCExpression              (const std::string &text, std::string& expression);
  static bool                     ConvertModifiedWriteValues      (const std::string &text, SvdTypes::ModifiedWriteValue& val, uint32_t lineNo);
  static bool                     ConvertReadAction               (const std::string &text, SvdTypes::ReadAction& act, uint32_t lineNo);
  static bool                     ConvertDataType                 (const std::string &text, std::string& dataType, uint32_t lineNo);

  static bool                     ConvertDerivedNameHirachy       (const std::string &name, std::list<std::string>& searchName);

  static const std::string&       GetDataTypeString               (uint32_t idx);

  static bool                     CheckParseError                 (const std::string& tag, const std::string& value, uint32_t lineNo);

  static std::string              CheckNameCCompliant             (const std::string& value, uint32_t lineNo = (uint32_t)-1);
  static bool                     CheckNameBrackets               (const std::string& value, uint32_t lineNo = (uint32_t)-1);
  static std::string              CheckDescription                (const std::string& value, uint32_t lineNo = (uint32_t)-1);
  static std::string              CheckTextGeneric                (const std::string& value, uint32_t lineNo = (uint32_t)-1);
  static std::string              CheckTextGeneric_SfrCC2         (const std::string& value, uint32_t lineNo = (uint32_t)-1);
  static bool                     IsSkipKeyword                   (const std::string& value);
  static std::string              FindGroupName                   (const std::string& periName);

  static bool                     FormatText                      (std::map<uint32_t, std::string>& formatedText, const std::string& text, uint32_t width);

  static SvdTypes::Access         CalcAccessResult                (SvdTypes::Access accField, SvdTypes::Access accReg);
  static bool                     IsMatchAccess                   (SvdTypes::Access accField, SvdTypes::Access accReg);

  // Message output helpers
  static std::string              CreateFieldRange                (int32_t msb, int32_t lsb, bool addWidth = false);
  static std::string              CreateAddress                   (uint32_t addr, uint32_t size = (uint32_t)-1);
  static std::string              CreateHexNum                    (uint64_t num);
  static std::string              CreateHexNum                    (uint32_t num, uint32_t minWidth);

  static std::string              CreateDecNum                    (int64_t num);
  static std::string              CreateLineNum                   (uint32_t num);

  static int32_t                  AlnumCmp                        (const char* str1, const char* str2, bool cs = true);
  static int32_t                  AlnumCmp                        (const std::string& a, const std::string& b, bool cs = true);


  class StringAlnumLess {
  public:
    bool operator()(const std::string& a, const std::string& b) const { return SvdUtils::AlnumCmp(a, b) < 0 ;}
  };

private:
  static const std::string dataType_str[];
  static const std::string groupNames[];
  static const std::map<std::string, SvdTypes::CpuType> cpuTranslation;
};

#endif // SvdUtils_H
