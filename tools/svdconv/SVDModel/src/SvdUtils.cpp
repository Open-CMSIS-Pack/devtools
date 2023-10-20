/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "SvdUtils.h"
#include "ErrLog.h"
#include "SvdItem.h"

#include <algorithm>    // std::count
#include <inttypes.h>
#include <sstream>
#include <iomanip>

using namespace std;

const string SvdUtils::EMPTY_STRING("");

const string SvdUtils::dataType_str[] = {
/*  0 */  "uint8_t",    // unsigned byte            width =  8
/*  1 */  "uint8_t",    // unsigned byte            width =  8
/*  2 */  "uint16_t",   // unsigned half word       width = 16
/*  3 */  "uint32_t",   // unsigned half word       width = 24
/*  4 */  "uint32_t",   // unsigned word            width = 32
/*  5 */  "uint64_t",   // unsigned double word     width = 64
/*  6 */  "uint64_t",   // unsigned double word     width = 64
/*  7 */  "uint64_t",   // unsigned double word     width = 64
/*  8 */  "uint64_t",   // unsigned double word     width = 64
/*  9 */  "int8_t",     // signed byte
/* 10 */  "int16_t",    // signed half word
/* 11 */  "int32_t",    // signed world
/* 12 */  "int64_t",    // signed double word
/* 13 */  "uint8_t*",   // pointer to unsigned byte
/* 14 */  "uint16_t*",  // pointer to unsigned half word
/* 15 */  "uint32_t*",  // pointer to unsigned word
/* 16 */  "uint64_t*",  // pointer to unsigned double word
/* 17 */  "int8_t*",    // pointer to signed byte
/* 18 */  "int16_t*",   // pointer to signed half word
/* 19 */  "int32_t*",   // pointer to signed world
/* 20 */  "int64_t*",   // pointer to signed double word
/* 21 */  "",           // end
};

const string SvdUtils::groupNames[] = {
  "PIO",
  "GPIO",
  "Port",
  "Timer",
  "SPI",
  "ADC",
  "DAC",
  "SSP",
  "SPI",
  ""
};

#define CPUTYPE(enumType, name) { name, SvdTypes::CpuType::enumType },
const map<string, SvdTypes::CpuType> SvdUtils::cpuTranslation = {
  #include "EnumStringTables.h"
};
#undef CPUTYPE


bool SvdUtils::TrimWhitespace(string &name)
{
  if(name.empty()) {
    return false;
  }

  // Trim front
  auto from = (size_t) 0;
  for(const auto c : name) {
    if(c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      from++;
    }
    else {
      break;
    }
  }

  // Trim back
  auto to = name.length();
  const string::reverse_iterator rend = name.rend();
  for(string::reverse_iterator it = name.rbegin(); it != rend; ++it) {
    if(*it == ' ' || *it == '\t' || *it == '\r' || *it == '\n') {
      to--;
    }
    else break;
  }

  if(from) {
    name.erase(0, from);
  }

  if(to < name.length()) {
    name.erase(to, name.length());
  }

  return true;
}


string SvdUtils::SpacesToUnderscore(const string& s)
{
  string res = s;
  const auto len = res.length();
  for(string::size_type pos = 0; pos < len; pos++) {
    char ch = res[pos];
    if(ch == ' ')
      res[pos] = '_';
  }
  return res;
}

string SvdUtils::SlashesToBackSlashes(const string& fileName)
{
  string name = fileName;
  string::size_type pos = 0;
  for(pos = name.find('/'); pos != string::npos; pos = name.find('/', pos)){
    name[pos] = '\\';
  }
  return name;
}

SvdTypes::Expression SvdUtils::ParseExpression(const string &expr, string &name, uint32_t &insertPos)
{
  if(expr.empty()) {
    return SvdTypes::Expression::NONE;
  }

  SvdTypes::Expression exp = SvdTypes::Expression::NONE;
  string::size_type posFound = -1;

  string::size_type pos = expr.find("%");
  if(pos != string::npos) {
    pos = expr.find("[%s]");
    if(pos != string::npos) {
      exp = SvdTypes::Expression::ARRAY;
      if(pos != expr.length() - 4) {
        exp = SvdTypes::Expression::NONE; //Expression::ARRAYINVALID;
      }
      posFound = pos;
    }
    else {
      pos = expr.find("%s");
      if(pos != string::npos) {
        exp = SvdTypes::Expression::EXTEND;
        string::size_type pos2 = expr.find_first_of("[]");
        if(pos2 != string::npos) {
          exp = SvdTypes::Expression::NONE; //Expression::INVALID;
          pos = pos2;
        }
        else {
          posFound = pos;
        }
      }
      else {
        string::size_type pos2 = expr.find_first_of("[%s]");
        if(pos2 != string::npos) {
          exp = SvdTypes::Expression::NONE; //Expression::INVALID;
          pos = pos2;
        }
      }
    }
  }

  switch(exp) {
    default:
    case SvdTypes::Expression::NONE:
      insertPos = -1;
      break;
    case SvdTypes::Expression::EXTEND:
    case SvdTypes::Expression::ARRAY:
      insertPos = posFound;
      break;
#if 0
    case SvdTypes::Expression::INVALID:
      name = expr;
      do {
        pos = name.find_first_of("[%s]");
        if(pos != string::npos) {
          name.erase(pos, pos);
        }
      } while(pos != string::npos);
      break;
#endif
  }

  name = expr;
  pos = name.find("[%s]");
  if(pos != string::npos) {
    name.erase(pos, 4);
  }

  pos = name.find("%s");
  if(pos != string::npos) {
    name.erase(pos, 2);
  }

  return exp;
}


bool SvdUtils::ToLower(string &text)
{
  std::transform(text.begin(), text.end(), text.begin(),
    [](unsigned char c){ return std::tolower(c); });

  return true;
}

bool SvdUtils::ToUpper(string &text)
{
  std::transform(text.begin(), text.end(), text.begin(),
    [](unsigned char c){ return std::toupper(c); });

  return true;
}

bool SvdUtils::ConvertNumber (const string &text, bool &number)
{
  if(text.length() > 1) {
    string t = text;
    ToLower(t);
    if (t == "true" ) {
      number = true;
      return true;
    }
    else if (t == "false") {
      number = false;
      return true;
    }
  }
  else if (text == "1") {
    number = true;
    return true;
  }
  else if (text == "0") {
    number = false;
    return true;
  }

  return false;
}

bool SvdUtils::ConvertCpuRevision(const string &text, uint32_t &rev)
{
  if(text.empty()) {
    return false;
  }

  uint32_t revMajor=0, revMinor=0;
  string verStr = text;
  SvdUtils::ToUpper(verStr);

  if(verStr[0] != 'R') {
    return false;
  }

  std::size_t pos = verStr.find('P');
  if(pos == string::npos) {
    return false;
  }

  string major = verStr.substr(0, pos);
  string minor = verStr.substr(pos);

  if(major.length() < 2 || minor.length() < 2) {
    return false;
  }

  if(ConvertNumber(&major[1], revMajor) == false) {
    return false;
  }

  if(ConvertNumber(&minor[1], revMinor) == false) {
    return false;
  }

  if(revMajor > 255 || revMinor > 255) {
    return false;
  }

  rev = (revMajor << 8) | (revMinor << 0);

  return true;
}

// Convert Protection: s, n, p
bool SvdUtils::ConvertProtectionStringType(const string &text, SvdTypes::ProtectionType &protection, uint32_t lineNo)
{
  if(text.empty()) {
    return false;
  }

  string t = text;
  ToLower(t);

  if(t == "s") {
    protection = SvdTypes::ProtectionType::SECURE;
    if(text != "s") {   // Upper/lower case mismatch
      LogMsg("M225", NAME(text), NAME2("s"), lineNo);
      return false;
    }
  }
  else if(t == "n") {
    protection = SvdTypes::ProtectionType::NONSECURE;
    if(text != "n") {   // Upper/lower case mismatch
      LogMsg("M225", NAME(text), NAME2("n"), lineNo);
      return false;
    }
  }
  else if(t == "p") {
    protection = SvdTypes::ProtectionType::PRIVILEGED;
    if(text != "p") {   // Upper/lower case mismatch
      LogMsg("M225", NAME(text), NAME2("p"), lineNo);
      return false;
    }
  }
  else {
    return false;
  }

  return true;
}

// Secure: s, n
bool SvdUtils::ConvertSauProtectionStringType(const string &text, SvdTypes::ProtectionType &protection, uint32_t lineNo)
{
  if(text.empty()) {
    return false;
  }

  string t = text;
  ToLower(t);

  if(t == "s") {
    protection = SvdTypes::ProtectionType::SECURE;
    if(text != "s") {
      LogMsg("M225", NAME(text), NAME2("s"), lineNo);
      return false;
    }
  }
  else if(t == "n") {
    protection = SvdTypes::ProtectionType::NONSECURE;
    if(text != "n") {
      LogMsg("M225", NAME(text), NAME2("n"), lineNo);
      return false;
    }
  }
  else {
    return false;   // unknown
  }

  return true;
}

bool SvdUtils::ConvertSauAccessType(const string &text, SvdTypes::SauAccessType &accessType, uint32_t lineNo)
{
  if(text.empty()) {
    return false;
  }

  string t = text;
  ToLower(t);

  if(t == "c") {
    accessType = SvdTypes::SauAccessType::SECURE;
    if(text != "c") {
      LogMsg("M225", NAME(text), NAME2("c"), lineNo);
      return false;
    }
  }
  else if(t == "n") {
    accessType = SvdTypes::SauAccessType::NONSECURE;
    if(text != "n") {
      LogMsg("M225", NAME(text), NAME2("n"), lineNo);
      return false;
    }
 }

  return true;
}

bool SvdUtils::ConvertNumber(const string &text, uint64_t &number, uint32_t base)
{
  if(text.empty()) {
    return false;
  }

  number = 0;

  for(const auto c : text) {
    number *= base;

    switch(base) {
      case 2:
        if(c == '0' || c == '1') {
          number += c - '0';
        }
        else {
          return false;
        }
        break;
      case 10:
        if(c >= '0' && c <= '9') {
          number += c - '0';
        }
        else {
          return false;
        }
        break;
      case 16:
        if(c >= '0' && c <= '9') {
          number += c - '0';
        }
        else if(c >= 'a' && c <= 'f') {
          number += c - 'a' + 10;
        }
        else {
          return false;
        }
        break;
      default:
        return false;
    }
  }

  return true;
}

bool SvdUtils::ConvertNumber(const string &text, set<uint64_t> &numbers)
{
  if(text.empty()) {
    return false;
  }

  uint64_t num = 0;

  if(!ConvertNumber(text, num)) {
    return false;
  }

  numbers.insert(num);

  return true;
}

bool SvdUtils::ConvertNumber(const string &text, uint32_t &number)
{
  uint64_t num = 0;
  bool ret = ConvertNumber(text, num);
  number = (uint32_t)num;

  return ret;
}

bool SvdUtils::ConvertNumber (const string &text, int32_t &number)
{
  uint64_t num = 0;
  bool ret = ConvertNumber(text, num);
  number = (uint32_t)num;

  return ret;
}

bool SvdUtils::ConvertNumber (const string &text, uint64_t &number)
{
  if(text.empty()) {
    return false;
  }

  uint64_t num=0;
  string t = text;

  ToLower(t);

  if(t == "true") {
    number = 1;
    return true;
  }
  if(t == "false") {
    number = 0;
    return true;
  }

  if(t.length() >= 2 && !t.compare(0, 2, "0x")) {    // Hex Number
    if(!ConvertNumber(&t[2], num, 16)) {
      return false;
    }

    number = num;
    return true;
  }

  if(t.length() >= 2 && !t.compare(0, 2, "0b")) {    // Hex Number
    if(t.find_first_of("x", 2) != string::npos) {    // XBin Number
      return false;
    }

    // Bin Number
    if(!ConvertNumber(&t[2], num, 2)) {
      return false;
    }

    number = num;
    return true;
  }

  if(t.length() >= 1 && !t.compare(0, 1, "#")) {    // Bin Number
    if(t.find_first_of("x", 1) != string::npos) {    // XBin Number
      return false;
    }

    // Bin Number
    if(!ConvertNumber(&t[1], num, 2)) {
      return false;
    }

    number = num;
    return true;
  }

  if(!ConvertNumber(t, num, 10)) {   // Dec Number
    return false;
  }

  number = num;

  return true;
}

bool SvdUtils::ConvertNumberXBin(const string &text, set<uint32_t> &numbers)
{
  if(text.empty()) {
    return false;
  }

  string t = text;

  ToLower(t);
  numbers.clear();

  size_t cnt = count(t.begin(), t.end(), 'x');
  if(cnt > 8) {
    LogMsg("M379", NAME(text));
    return false;
  }

  uint32_t num;
  if(ConvertNumber(text, num) != false) {
    numbers.insert(num);
    return true;
  }

  if(t.length() >= 2 && !t.compare(0, 2, "0b")) {    // Hex Number
    uint32_t value = 0;
    const string txt = &t[2];
    if(!DoConvertNumberXBin(txt, value, numbers)) {
      return false;
    }
  }
  else if(t.length() >= 1 && !t.compare(0, 1, "#")) {    // Hex Number
    uint32_t value = 0;
    const string txt = &t[1];
    if(!DoConvertNumberXBin(txt, value, numbers)) {
      return false;
    }
  }
  else {
    return false;
  }

  return true;
}


bool SvdUtils::DoConvertNumberXBin(const string &text, uint32_t value, set<uint32_t> &numbers)
{
  if(text.empty()) {
    return false;
  }

  for(auto it = text.begin(); it != text.end(); it++) {
    const char c = *it;
    value *= 2;

    if(c == '0' || c == '1') {
      value |= c - '0';

      if(it+1 == text.end()) {
        numbers.insert(value);
      }
    }
    else if(c == 'x') {
      it++;
      if(it == text.end()) {
        numbers.insert(value);
        value |= 1;
        numbers.insert(value);
        return true;
      }

      if(!DoConvertNumberXBin(&(*it), value, numbers)) {
        return false;
      }

      value |= 1;
      if(!DoConvertNumberXBin(&(*it), value, numbers)) {
        return false;
      }

      return true;    // exit loop after recursive calls
    }
  }

  return true;
}

bool SvdUtils::ConvertBitRange(const string &text, uint32_t &msb, uint32_t &lsb)
{
  if(text.empty()) {
    return false;
  }

  string t = text;
  string::size_type pos;

  // find one occurrence of '['
  pos = t.find('[');
  if(pos == string::npos) {
    return false;
  }
  t.erase(pos, 1);

  pos = t.find('[');
  if(pos != string::npos) {
    return false;
  }

  // find one occurrence of ']'
  pos = t.find(']');
  if(pos == string::npos) {
    return false;
  }
  t.erase(pos, 1);

  pos = t.find(']');
  if(pos != string::npos) {
    return false;
  }

  // find ':'
  pos = t.find(':');
  if(pos == string::npos) {
    return false;
  }

  string s_msb = t.substr(0,     pos       );
  string s_lsb = t.substr(pos+1, t.length());

  if(s_msb.empty() || s_lsb.empty()) {
    return false;
  }

  if(!ConvertNumber(s_msb, msb)) {
    return false;
  }

  if(!ConvertNumber(s_lsb, lsb)) {
    return false;
  }

  return true;
}

bool SvdUtils::ConvertAccess (const string &text, SvdTypes::Access &acc, uint32_t lineNo)
{
  if(text.empty()) {
    return false;
  }

  string t = text;
  ToLower(t);

  uint32_t cnt=1;
  do {
    const auto& accStr = SvdTypes::GetAccessType((SvdTypes::Access)cnt);
    if(accStr.empty()) {
      break;
    }

    string accStrLower = accStr;
    ToLower(accStrLower);

    if(t == accStrLower) {
      acc = (SvdTypes::Access)cnt;
      if(text != accStr) {
        LogMsg("M225", NAME(text), NAME2(accStr), lineNo);
      }
      return true;
    }
  } while(++cnt < (uint32_t)SvdTypes::Access::END);

  // Deprecated
  if(t == "read") {
    acc = SvdTypes::Access::READONLY;
    LogMsg("M224", NAME(text), NAME2("read-only"), lineNo);
    return true;
  }
  else if(t == "write") {
    acc = SvdTypes::Access::WRITEONLY;
    LogMsg("M224", NAME(text), NAME2("write-only"), lineNo);
    return true;
  }

  acc = SvdTypes::Access::UNDEF;

  return false;
}

bool SvdUtils::ConvertAddrBlockUsage (const string &text, SvdTypes::AddrBlockUsage &usage, uint32_t lineNo)
{
  if(text.empty()) {
    return false;
  }

  string t = text;
  ToLower(t);

  if(t == "registers") {
    usage = SvdTypes::AddrBlockUsage::REGISTERS;
    if(text != "registers") {
      LogMsg("M225", NAME(text), NAME2("registers"), lineNo);
    }
    return true;
  } else if(t == "buffer") {
    usage = SvdTypes::AddrBlockUsage::BUFFER;
    if(text != "buffer") {
      LogMsg("M225", NAME(text), NAME2("buffer"), lineNo);
    }
    return true;
  } else if(t == "reserved") {
    usage = SvdTypes::AddrBlockUsage::RESERVED;
    if(text != "reserved") {
      LogMsg("M225", NAME(text), NAME2("reserved"), lineNo);
    }
    return true;
  }
  else {
    LogMsg("M212", NAME(text));
  }

  usage = SvdTypes::AddrBlockUsage::UNDEF;

  return false;
}

bool SvdUtils::ConvertEnumUsage (const string &text, SvdTypes::EnumUsage &usage, uint32_t lineNo)
{
  if(text.empty()) {
    return false;
  }

  string t = text;
  ToLower(t);

  if(t == "read") {
    usage = SvdTypes::EnumUsage::READ;
    if(text != "read") {
      LogMsg("M225", NAME(text), NAME2("read"), lineNo);
    }
    return true;
  }
  else if(t == "write") {
    usage = SvdTypes::EnumUsage::WRITE;
    if(text != "write") {
      LogMsg("M225", NAME(text), NAME2("write"), lineNo);
    }
    return true;
  }
  else if(t == "read-write") {
    usage = SvdTypes::EnumUsage::READWRITE;
    if(text != "read-write") {
      LogMsg("M225", NAME(text), NAME2("read-write"), lineNo);
    }
    return true;
  }

  usage = SvdTypes::EnumUsage::UNDEF;

  return false;
}

bool SvdUtils::ConvertCpuType (const string &text, SvdTypes::CpuType &cpuType)
{
  if(text.empty()) {
    return false;
  }

  string t = text;
  ToUpper(t);

  auto it = cpuTranslation.find(t);
  if(it == cpuTranslation.end()) {
    cpuType = SvdTypes::CpuType::UNDEF;
    return false;
  }

  cpuType = it->second;
  if(cpuType == SvdTypes::CpuType::CM0P) {
    cpuType = SvdTypes::CpuType::CM0PLUS;
  }

  return true;
}

bool SvdUtils::ConvertCpuEndian (const string &text, SvdTypes::Endian &endian, uint32_t lineNo)
{
  if(text.empty()) {
    return false;
  }

  string t = text;
  ToLower(t);

  if(t == "little") {
    endian = SvdTypes::Endian::LITTLE;
    if(text != "little") {
      LogMsg("M225", NAME(text), NAME2("little"), lineNo);
    }
    return true;
  }
  else if(t == "big") {
    endian = SvdTypes::Endian::BIG;
    if(text != "big") {
      LogMsg("M225", NAME(text), NAME2("big"), lineNo);
    }
    return true;
  }
  else if(t == "selectable") {
    endian = SvdTypes::Endian::SELECTABLE;
    if(text != "selectable") {
      LogMsg("M225", NAME(text), NAME2("selectable"), lineNo);
    }
    return true;
  }
  else if(t == "other") {
    endian = SvdTypes::Endian::OTHER;
    if(text != "other") {
      LogMsg("M225", NAME(text), NAME2("other"), lineNo);
    }
    return true;
  }

  endian = SvdTypes::Endian::OTHER;

  return false;
}

bool SvdUtils::ConvertCExpression(const string &text, string &expression)
{
  if(text.empty()) {
    return false;
  }

  expression = text;    // done later

  return true;
}

bool SvdUtils::ConvertDataType(const string &text, string &dataType, uint32_t lineNo)
{
  if(text.empty()) {
    return false;
  }

  string t = text;
  ToLower(t);
  TrimWhitespace(t);

  bool found = false;
  uint32_t cnt=1;
  string tp;
  do {
    tp = GetDataTypeString(cnt++);
    if(tp == t) {
      found = true;
      break;
    }
  } while(!tp.empty());

  if(found) {
    dataType = t;
    return true;
  }

  return false;
}

const string& SvdUtils::GetDataTypeString(uint32_t idx)
{
  return dataType_str[idx];
}

bool SvdUtils::ConvertModifiedWriteValues(const string &text, SvdTypes::ModifiedWriteValue &val, uint32_t lineNo)
{
  if(text.empty()) {
    return false;
  }

  string t = text;
  ToLower(t);

  if(t == "onetoclear") {
    val = SvdTypes::ModifiedWriteValue::ONETOCLEAR;
    if(text != "oneToClear") {
      LogMsg("M225", NAME(text), NAME2("oneToClear"), lineNo);
    }
    return true;
  }
  else if(t == "onetoset") {
    val = SvdTypes::ModifiedWriteValue::ONETOSET;
    if(text != "oneToSet") {
      LogMsg("M225", NAME(text), NAME2("oneToSet"), lineNo);
    }
    return true;
  }
  else if(t == "onetotoggle") {
    val = SvdTypes::ModifiedWriteValue::ONETOTOGGLE;
    if(text != "oneToToggle") {
      LogMsg("M225", NAME(text), NAME2("oneToToggle"), lineNo);
    }
    return true;
  }
  else if(t == "zerotoclear") {
    val = SvdTypes::ModifiedWriteValue::ZEROTOCLEAR;
    if(text != "zeroToClear") {
      LogMsg("M225", NAME(text), NAME2("zeroToClear"), lineNo);
    }
    return true;
  }
  else if(t == "zerotoset") {
    val = SvdTypes::ModifiedWriteValue::ZEROTOSET;
    if(text != "zeroToSet") {
      LogMsg("M225", NAME(text), NAME2("zeroToSet"), lineNo);
    }
    return true;
  }
  else if(t == "zerototoggle") {
    val = SvdTypes::ModifiedWriteValue::ZEROTOTOGGLE;
    if(text != "zeroToToggle") {
      LogMsg("M225", NAME(text), NAME2("zeroToToggle"), lineNo);
    }
    return true;
  }
  else if(t == "clear") {
    val = SvdTypes::ModifiedWriteValue::CLEAR;
    if(text != "clear") {
      LogMsg("M225", NAME(text), NAME2("clear"), lineNo);
    }
    return true;
  }
  else if(t == "set") {
    val = SvdTypes::ModifiedWriteValue::SET;
    if(text != "set") {
      LogMsg("M225", NAME(text), NAME2("set"), lineNo);
    }
    return true;
  }
  else if(t == "selectable") {
    val = SvdTypes::ModifiedWriteValue::SET;
    if(text != "selectable") {
      LogMsg("M225", NAME(text), NAME2("selectable"), lineNo);
    }
    return true;
  }
  else if(t == "modify") {
    val = SvdTypes::ModifiedWriteValue::MODIFY;
    if(text != "modify") {
      LogMsg("M225", NAME(text), NAME2("modify"), lineNo);
    }
    return true;
  }

  val = SvdTypes::ModifiedWriteValue::UNDEF;

  return false;
}

bool SvdUtils::ConvertReadAction  (const string &text, SvdTypes::ReadAction &act, uint32_t lineNo)
{
  if(text.empty()) {
    return false;
  }

  string t = text;
  ToLower(t);

  if(t == "clear") {
    act = SvdTypes::ReadAction::CLEAR;
    if(text != "clear") {
      LogMsg("M225", NAME(text), NAME2("clear"), lineNo);
    }
    return true;
  }
  else if(t == "set") {
    act = SvdTypes::ReadAction::SET;
    if(text != "set") {
      LogMsg("M225", NAME(text), NAME2("set"), lineNo);
    }
    return true;
  }
  else if(t == "modify") {
    act = SvdTypes::ReadAction::MODIFY;
    if(text != "modify") {
      LogMsg("M225", NAME(text), NAME2("modify"), lineNo);
    }
    return true;
  }
  else if(t == "modifyexternal") {
    act = SvdTypes::ReadAction::MODIFEXT;
    if(text != "modifyExternal") {
      LogMsg("M225", NAME(text), NAME2("modifyExternal"), lineNo);
    }
    return true;
  }

  act = SvdTypes::ReadAction::UNDEF;

  return false;
}

bool SvdUtils::ConvertDerivedNameHirachy(const string &name, list<string>& searchName)
{
  if(name.empty()) {      // empty string
    searchName.push_back("!ERROR!");
    return true;
  }

  string::size_type pos=0, prevPos=0;
  pos = name.find('.', pos);

  // only one name
  if(pos == string::npos) {
    searchName.push_back(name);
    return true;
  }

  do {
    pos = name.find('.', pos);
    if(pos != string::npos) {
      string text = name.substr(prevPos, pos - prevPos);
      if(!text.empty()) {
        searchName.push_back(text);
      }
      else {
        searchName.push_back("!ERROR!");
      }

      pos++;
      prevPos = pos;
      if(pos >= name.length()) {
        break;
      }
    }
  } while(pos != string::npos);

  if(prevPos <= name.length()) {
    string text = name.substr(prevPos, name.length() - prevPos);
    if(!text.empty()) {
      searchName.push_back(text);
    }
    else {
      searchName.push_back("!ERROR!");
    }
  }

  return true;
}

bool SvdUtils::CheckParseError (const string& tag, const string& value, uint32_t lineNo)
{
  if(value.empty()) {
    LogMsg("M205", TAG(tag), lineNo);
    return true;
  }

  if(tag == "access") {
    LogMsg("M233", TAG(tag), VALUE(value), lineNo);
  }
  else {
    LogMsg("M202", TAG(tag), VALUE(value), lineNo);
  }

  return true;
}

string SvdUtils::CheckNameCCompliant(const string& value, uint32_t lineNo)
{
  string::size_type pos;
  string name = CheckTextGeneric(value, lineNo);    // basic checks

  do {
    pos = name.find_first_of(" \\:;.,#?()+-*/");
    if(pos != string::npos) {
      char c = name[pos];
      string hexStr = SvdUtils::CreateHexNum(c);
      string str;
      str += c;
      LogMsg("M305", NAME(name), VAL("HEX", hexStr), lineNo);
      name[pos] = '_';
    }
  } while(pos != string::npos);


  return name;
}

bool SvdUtils::CheckNameBrackets(const string& value, uint32_t lineNo)
{
  string::size_type pos;
  string name = CheckTextGeneric(value, lineNo);    // basic checks

  pos = name.find_first_of("[]");
  if(pos != string::npos) {
    LogMsg("M386", NAME(name), lineNo);
    return false;
  }

  return true;
}

string SvdUtils::CheckDescription(const string& value, uint32_t lineNo)
{
  string descr = CheckTextGeneric(value, lineNo);    // basic checks
#if 0   // allow % in description
  string::size_type pos;

  do {
    pos = descr.find_first_of("%");
    if(pos != string::npos) {
      char c = descr[pos];
      string hexStr = SvdUtils::CreateHexNum(c);
      string str;
      str += c;
      LogMsg("M216", NAME(descr), VAL("HEX", hexStr), lineNo);
      descr[pos] = '_';
    }
  } while(pos != string::npos);
#endif
  return descr;
}

string SvdUtils::CheckTextGeneric(const string& value, uint32_t lineNo)
{
  string descr;
  unsigned char c=0, cPrev=0, cPrev2=0;

  for(auto it = value.begin(); it != value.end(); it++) {
    cPrev2  = cPrev;
    cPrev   = c;
    c       = *it;
    descr  += c;     // Do not delete or modify any chars! (from JK: 11.01.2018)

    if(c == '%') {
      auto tmpIt = it;
      tmpIt++;
      if(tmpIt != value.end() && *tmpIt == 's') {   // accept %s
        ;
      }
    }

    if(c == ' ' && cPrev == ' ') {      // delete double whitespace
      continue;
    }

    if(c == '\n' || c == '\r' || c == '\t') {   // convert all additional whitespace characters
      if(cPrev == ' ') {
        continue;
      } else {
        c = ' ';
      }
    }

    if(cPrev == '?' && cPrev2 == '?') {
      string str;
      str += c;
      LogMsg("M217", NAME(value), VAL("CHAR", str), lineNo);
      continue;
    }

    if(c < 0x20 || c > 0x7e) {
      string hexStr = SvdUtils::CreateHexNum(c);
      string str;
      str += c;
      LogMsg("M216", NAME(value), VAL("HEX", hexStr), lineNo);
      continue;
    }

    if(c == '?' && cPrev != '?') {
      //descr += c;
      continue;
    }

    switch(c) {
      //case '\\':
      //case '“':
      //case '”':
      case '"': {
        string str;
        str += c;
        string hexStr = SvdUtils::CreateHexNum(c);
        LogMsg("M216", NAME(value), VAL("HEX", hexStr), lineNo);
        } continue;

      default:
        break;
    }

    if(cPrev == '\\') {     // Test on supported ESC sequences
      switch(c) {
        case 'n':
          break;

        default:
          string str;
          str += c;
          LogMsg("M218", NAME(value), VAL("CHAR", str), lineNo);
          continue;
      }
    }
  }

  return descr;
}

string SvdUtils::CheckTextGeneric_SfrCC2(const string& value, uint32_t lineNo)
{
  string descr;
  unsigned char c=0, cPrev=0, cPrev2=0;

  for(auto it = value.begin(); it != value.end(); it++) {
    cPrev2  = cPrev;
    cPrev   = c;
    c       = *it;

    if(c == '%') {
      auto tmpIt = it;
      tmpIt++;
      if(tmpIt != value.end() && *tmpIt == 's') {
        ;
      }
    }

    if(c == ' ' && cPrev == ' ') {      // delete double whitespace
      continue;
    }

    if(c == '\n' || c == '\r' || c == '\t') {   // convert all additional whitespace characters
      if(cPrev == ' ') {
        continue;
      }
      else {
        c = ' ';
      }
    }

    if(cPrev == '?' && cPrev2 == '?') {
      string str;
      str += c;
      LogMsg("M517", NAME(value), VAL("CHAR", str), lineNo);
      continue;
    }

    if(c < 0x20 || c > 0x7e) {
      string hexStr = SvdUtils::CreateHexNum(c);
      string str;
      str += c;
      LogMsg("M516", NAME(value), VAL("HEX", hexStr), lineNo);
      continue;
    }

    if(c == '?' && cPrev != '?') {
      descr += c;
      continue;
    }

    switch(c) {
      //case '\\':
      // FIXME: These characters are already suppressed by statement above!
      // case 0x93: // FIXME: ANSI Windows Codepage 1252 character “
      // case 0x94: // FIXME: ANSI Windows Codepage 1252 character ”
      case '"': {
        descr += "\\\"";
        string hexStr = SvdUtils::CreateHexNum(c);
        LogMsg("M516", NAME(value), VAL("HEX", hexStr), lineNo);
        } continue;

      default:
        break;
    }

    if(cPrev == '\\') {     // Test on supported ESC sequences
      switch(c) {
        case 'n':
          break;

        default:
          // FIXME: The escape sequence is messed up, e.g. "\\rtest" converts to "\\test"
          string str;
          str += c;
          LogMsg("M518", NAME(value), VAL("CHAR", str), lineNo);
          //if(descr.length()) {
          //  descr.erase(descr.length()-1);
          //}
          continue;
      }
    }

    descr += c;
  }

  return descr;
}

string SvdUtils::FindGroupName(const string& periName)
{
  string name;

  // Known Group Names
  uint32_t idx=0;
  while(groupNames[idx] != "") {
    if(periName.find(groupNames[idx], 0) == 0) {
      name = groupNames[idx];
      return name;
    }
    idx++;
  }

  // Search for " -_"
  string::size_type pos;
  pos = periName.find_first_of(" -_", 1);
  if(pos != string::npos) {
    name = periName;
    name.erase(pos, name.length());
    return name;
  }

  // Search for 0-9
  pos = periName.find_first_of("0123456789", 1);
  if(pos != string::npos) {
    name = periName;
    name.erase(pos, name.length());
    return name;
  }

  return periName;
}

bool SvdUtils::IsSkipKeyword(const string& value)
{
  string name = value;
  ToUpper(name);

  if(name.compare("RESERVED") == 0) {
    return true;
  }

  return false;
}

// Message output helpers
string SvdUtils::CreateFieldRange(int32_t msb, int32_t lsb, bool addWidth /*= false*/)
{
  string text;

  text = "[";
  text += SvdUtils::CreateDecNum(msb);
  text += "..";
  text += SvdUtils::CreateDecNum(lsb);

  if(addWidth) {
    text += ":";
    text += SvdUtils::CreateDecNum(msb - lsb +1);
  }

  text += "]";

  return text;
}

string SvdUtils::CreateAddress(uint32_t addr, uint32_t size /* = -1*/)
{
  string text = SvdUtils::CreateHexNum(addr, 8);

  if(size != (uint32_t)-1) {
    text += ":";
    text += SvdUtils::CreateDecNum(size);
  }

  return text;
}

string SvdUtils::CreateHexNum(uint64_t num)
{
  stringstream  ss;
  ss << "0x" << std::uppercase << std::hex << num;

  return ss.str();
}

string SvdUtils::CreateHexNum(uint32_t num, uint32_t minWidth)
{
  stringstream  ss;
  ss << "0x" << std::setw(minWidth) << std::setfill('0') << std::uppercase << std::hex << num;

  return ss.str();
}

string SvdUtils::CreateDecNum(int64_t num)
{
  string text;
  text = to_string(num);

  return text;
}

string SvdUtils::CreateLineNum(uint32_t num)
{
  string text;
  text = "(Line ";
  text += CreateDecNum(num);
  text += ")";

  return text;
}

bool SvdUtils::FormatText(map<uint32_t, string>& formatedText, const string& txt, uint32_t width)
{
  uint32_t i=0;
  bool foundNewLine = false;
  string text = txt;
  string::size_type findPos;
  string::size_type pos = 0;
  string::size_type copyFrom=0, copyCnt=0;

  do {
    pos = text.find_first_of("\r\n");
    if(pos != string::npos) {
      text.erase(pos, 1);
    }
  } while(pos != string::npos);

  do {
    pos = text.find("  ");
    if(pos != string::npos) {
      text.erase(pos, 1);
    }
  } while(pos != string::npos);

  size_t len = text.length();
  pos=0;
  do {
    findPos = text.find("\\n", pos);
    if(findPos != string::npos) {
      copyFrom = pos;
      copyCnt  = findPos - pos;
      foundNewLine = true;
    }
    else {
      copyFrom = pos;
      copyCnt  = width;
    }

    if(copyCnt > width) {
      string::size_type lastPos = copyFrom;
      findPos = copyFrom;

      do {
        findPos = text.find(" ", findPos);
        if(findPos != string::npos) {
          if(findPos > width+copyFrom) {
            foundNewLine = false;
            pos++;
            break;
          }

          lastPos = findPos;
          findPos++;
        }
      } while (findPos != string::npos);

      copyCnt  = lastPos - copyFrom;
    }

    formatedText[i] = text.substr(copyFrom, copyCnt);
    auto firstChr = formatedText[i].begin();
    if(firstChr != formatedText[i].end() && *firstChr == ' ') {
      formatedText[i].erase(0,1);
    }
    i++;
    pos += copyCnt;

    if(foundNewLine) {
      foundNewLine = false;
      pos+=2;
    }
  } while(pos < len);

  return true;
}

bool SvdUtils::IsMatchAccess(SvdTypes::Access accField, SvdTypes::Access accReg)
{
  // Access::UNDEF=0, Access::READONLY=1, Access::WRITEONLY=2, Access::READWRITE=3, Access::WRITEONCE=4, Access::READWRITEONCE=5
  switch(accReg) {
    case SvdTypes::Access::READONLY: {
      switch(accField) {
        case SvdTypes::Access::READONLY:
          return true;

        case SvdTypes::Access::READWRITE:
        case SvdTypes::Access::READWRITEONCE:
        case SvdTypes::Access::WRITEONLY:
        case SvdTypes::Access::WRITEONCE:
        default:
          return false;
      }
    } break;

    case SvdTypes::Access::WRITEONLY: {
      switch(accField) {
        case SvdTypes::Access::WRITEONLY:
        case SvdTypes::Access::WRITEONCE:
          return true;

        case SvdTypes::Access::READONLY:
        case SvdTypes::Access::READWRITE:
        case SvdTypes::Access::READWRITEONCE:
        default:
          return false;
      }
    } break;

    case SvdTypes::Access::WRITEONCE: {
      switch(accField) {
        case SvdTypes::Access::WRITEONLY:
        case SvdTypes::Access::WRITEONCE:
          return true;

        case SvdTypes::Access::READONLY:
        case SvdTypes::Access::READWRITE:
        case SvdTypes::Access::READWRITEONCE:
        default:
          return false;
      }
    } break;

    case SvdTypes::Access::READWRITE: {
      switch(accField) {
        case SvdTypes::Access::READONLY:
        case SvdTypes::Access::WRITEONLY:
        case SvdTypes::Access::WRITEONCE:
        case SvdTypes::Access::READWRITE:
        case SvdTypes::Access::READWRITEONCE:
          return true;

        default:
          return false;
      }
    } break;

    case SvdTypes::Access::READWRITEONCE: {
      switch(accField) {
        case SvdTypes::Access::READONLY:
        case SvdTypes::Access::WRITEONLY:
        case SvdTypes::Access::WRITEONCE:
        case SvdTypes::Access::READWRITE:
        case SvdTypes::Access::READWRITEONCE:
          return true;

        default:
          return false;
      }
    } break;

    default: {
      return false;
    }
  }

  return false;
}

SvdTypes::Access SvdUtils::CalcAccessResult(SvdTypes::Access accField, SvdTypes::Access accReg)
{
  // Access::UNDEF=0, Access::READONLY=1, Access::WRITEONLY=2, Access::READWRITE=3, Access::WRITEONCE=4, Access::READWRITEONCE=5
  switch(accReg) {
    case SvdTypes::Access::READONLY: {
      switch(accField) {
        case SvdTypes::Access::WRITEONLY:
        case SvdTypes::Access::READWRITE:
          return SvdTypes::Access::READWRITE;

        case SvdTypes::Access::WRITEONCE:
        case SvdTypes::Access::READWRITEONCE:
          return SvdTypes::Access::READWRITEONCE;

        case SvdTypes::Access::READONLY:
          return SvdTypes::Access::READONLY;

        default:
          return accReg;
      }
    } break;

    case SvdTypes::Access::WRITEONLY: {
      switch(accField) {
        case SvdTypes::Access::READONLY:
        case SvdTypes::Access::READWRITE:
        case SvdTypes::Access::READWRITEONCE:
          return SvdTypes::Access::READWRITE;

        case SvdTypes::Access::WRITEONLY:
        case SvdTypes::Access::WRITEONCE:
          return SvdTypes::Access::WRITEONLY;

        default:
          return accReg;
      }
    } break;

    case SvdTypes::Access::WRITEONCE: {
      switch(accField) {
        case SvdTypes::Access::READONLY:
        case SvdTypes::Access::READWRITEONCE:
          return SvdTypes::Access::READWRITEONCE;

        case SvdTypes::Access::READWRITE:
          return SvdTypes::Access::READWRITE;

        case SvdTypes::Access::WRITEONLY:
        case SvdTypes::Access::WRITEONCE:
          return SvdTypes::Access::WRITEONLY;

        default:
          return accReg;
      }
    } break;

    case SvdTypes::Access::READWRITE: {
      switch(accField) {
        case SvdTypes::Access::READONLY:
        case SvdTypes::Access::WRITEONLY:
        case SvdTypes::Access::WRITEONCE:
        case SvdTypes::Access::READWRITEONCE:
          return SvdTypes::Access::READWRITE;

        case SvdTypes::Access::READWRITE:
          return SvdTypes::Access::READWRITE;

        default:
          return accReg;
      }
    } break;

    case SvdTypes::Access::READWRITEONCE: {
      switch(accField) {
        case SvdTypes::Access::READONLY:
        case SvdTypes::Access::WRITEONLY:
          return SvdTypes::Access::READWRITE;

        case SvdTypes::Access::WRITEONCE:
        case SvdTypes::Access::READWRITEONCE:
          return SvdTypes::Access::READWRITEONCE;

        case SvdTypes::Access::READWRITE:
          return SvdTypes::Access::READWRITE;

        default:
          return accReg;
      }
    } break;

    default: {
      return accField;
    }
  }

  return SvdTypes::Access::UNDEF;
}

int32_t SvdUtils::AlnumCmp(const char* str1, const char* str2, bool cs)
{
  if (str1 == str2) {
    return 0; // constant case
  }

	if(!str1) {
		return (!str2) ? 0 : -1;
  }
	else if(!str2) {
		return 1;
  }

	while(*str1 && *str2) {
		if(isdigit((unsigned char)*str1) && isdigit((unsigned char)*str2)) {
			int32_t n1 = atoi(str1);
			int32_t n2 = atoi(str2);

			if(n1 > n2) {
				return 1;
      }
			else if(n1 < n2) {
				return -1;
      }

			// skip digits
			while(*str1 && isdigit((unsigned char)*str1)) {
				str1++;
      }

			while(*str2 && isdigit((unsigned char)*str2)) {
				str2++;
      }
		}
		else {
			char c1 = *str1;
			char c2 = *str2;

			if(!cs) {
				c1 = toupper(c1);
				c2 = toupper(c2);
			}

			if(c1 > c2) {
				return 1;
      }
			else if(c1 < c2) {
				return -1;
      }

			str1++;
			str2++;
		}
	}

  return (!*str1 && !*str2) ? 0 : *str1 ? 1 : -1;
}

int32_t SvdUtils::AlnumCmp(const std::string& a, const std::string& b, bool cs)
{
  if(a.empty()) {
	  return (b.empty())? 0 : -1;
  }
  else if(b.empty()) {
	  return 1;
  }

  return AlnumCmp(a.c_str(), b.c_str(), cs);
}
