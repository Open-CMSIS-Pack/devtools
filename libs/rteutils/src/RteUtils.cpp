/******************************************************************************/
/* RTE  -  CMSIS Run-Time Environment				                          */
/******************************************************************************/
/** @file  RteUtils.cpp
  * @brief CMSIS RTE Data model
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2024 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "RteUtils.h"
#include "RteConstants.h"

#include <cstring>
#include <sstream>
#include <regex>

using namespace std;

const string RteUtils::EMPTY_STRING("");
const string RteUtils::SPACE_STRING(" ");
const string RteUtils::DASH_STRING("-");

const string RteUtils::CRLF_STRING(RteConstants::CRLF);
const string RteUtils::CR_STRING(RteConstants::CR);
const string RteUtils::LF_STRING(RteConstants::LF);

const string RteUtils::ERROR_STRING("<ERROR>");
const string RteUtils::BASE_STRING("base");
const string RteUtils::UPDATE_STRING("update");

const set<string> RteUtils::EMPTY_STRING_SET;
const list<string> RteUtils::EMPTY_STRING_LIST;
const vector<string> RteUtils::EMPTY_STRING_VECTOR;

const char RteUtils::CatalogName[] = "pack.idx";

string RteUtils::GetPrefix(const string& s, char delimiter, bool withDelimiter)
{
  string::size_type pos = s.find_first_of(delimiter);
  if (pos == string::npos)
    return s;

  if (withDelimiter)
    pos++;
  return s.substr(0, pos);
}

string RteUtils::GetSuffix(const string& s, char delimiter, bool withDelimiter)
{
  string::size_type pos = s.find_last_of(delimiter);
  if (pos == string::npos)
    return EMPTY_STRING;

  if (!withDelimiter)
    pos++;
  return s.substr(pos);
}

int RteUtils::GetSuffixAsInt(const string& value, char delimiter)
{
  string s = GetSuffix(value, delimiter);
  if (s.empty())
    return -1;
  return stoi(s);
}

string RteUtils::RemovePrefixByString(const string& s, const char* delimiter)
{
  size_t            len = strlen(delimiter);
  string::size_type pos = s.find(delimiter);
  if (pos == string::npos)
    return s;

  return s.substr(pos + len);
}

string RteUtils::RemoveSuffixByString(const string& s, const char* delimiter)
{
  const char* cpos = strstr(s.c_str(), delimiter);
  const char* pos;
  if (cpos == 0) {
    return EMPTY_STRING;
  }
  do {
    pos = cpos;
    cpos = strstr(cpos + 1, delimiter);
  } while (cpos);
  size_t cnt = pos - s.c_str();
  if(cnt > 0 ) {
    return s.substr(0, cnt);
  }
  return EMPTY_STRING;
}

int RteUtils::CountDelimiters(const string& s, const char* delimiter)
{
  int dlen, count = 0;
  string::size_type pos = 0;
  if (s.empty() || delimiter == 0)
    return 0;
  dlen = (int)strlen(delimiter);
  if (dlen <= 0)
    return 0;
  while ((pos = s.find(delimiter, pos)) != string::npos) {
    count++; pos += dlen;
  }
  return count;
}

int RteUtils::SplitString(list<string>& segments, const string& s, const char delimiter)
{
  size_t startPos = 0;
  size_t endPos = 0;
  while ((endPos = s.find_first_of(delimiter, startPos)) != string::npos) {
    string segment = s.substr(startPos, endPos - startPos);
    segments.push_back(segment);
    startPos = endPos + 1;
  }
  string segment = s.substr(startPos);
  if (!segment.empty())
    segments.push_back(segment); // remaining segment if not empty
  return (int)segments.size();
}

set<string> RteUtils::SplitStringToSet(const string& args, const string& delimiter) {
  set<string> s;
  if(args.empty()) {
    return s;
  }
  if(delimiter.empty()) {
    s.insert(args);
    return s;
  }
  size_t end = 0, start = 0, len = args.length();
  while(end < len) {
    if((end = args.find_first_of(delimiter, start)) == string::npos) end = len;
    s.insert(args.substr(start, end - start));
    start = end + 1;
  }
  return s;
}

bool RteUtils::EqualNoCase(const std::string& a, const std::string& b)
{
  // first compare lengths
  if (a.size() != b.size()) {
    return false;
  }
  return std::equal(a.begin(), a.end(), b.begin(), b.end(),
    [](auto ca, auto cb) { return tolower(ca) == tolower(cb); });
}


bool RteUtils::CheckCMSISName(const string& s)
{
  for (auto ch : s) {
    if (!isalnum(ch) && ch != '-' && ch != '_' && ch != '/') // allowed characters
      return false;
  }
  return true;
}

std::string& RteUtils::ReplaceAll(std::string& str, const string& toReplace, const string& with)
{
  for (size_t pos = str.find(toReplace); pos != string::npos; pos = str.find(toReplace, pos + with.size())) {
    str.replace(pos, toReplace.size(), with);
  }
  return str;
}

string RteUtils::ExpandAccessSequences(const string& src, const StrMap& variables) {
  string ret = src;
  if (regex_match(ret, regex(".*\\$.*\\$.*"))) {
    for (const auto& [varName, replacement] : variables) {
      const string var = "$" + varName + "$";
      size_t index = 0;
      while ((index = ret.find(var)) != string::npos) {
        ret.replace(index, var.length(), replacement);
      }
    }
  }
  return ret;
}

string RteUtils::SpacesToUnderscore(const string& s)
{
  string res = s;
  for (string::size_type pos = 0; pos < res.length(); pos++) {
    char ch = res[pos];
    if (ch == ' ')
      res[pos] = '_';
  }
  return res;
}

string RteUtils::SlashesToOsSlashes(const string& fileName)
{
#if defined(_WIN32)
  return SlashesToBackSlashes(fileName);
#else
  return BackSlashesToSlashes(fileName);
#endif
}

string RteUtils::SlashesToBackSlashes(const string& fileName)
{
  string name = fileName;
  string::size_type pos = 0;
  for (pos = name.find('/'); pos != string::npos; pos = name.find('/', pos)) {
    name[pos] = '\\';
  }
  return name;
}

string RteUtils::BackSlashesToSlashes(const string& fileName)
{
  string name = fileName;
  string::size_type pos = 0;
  for (pos = name.find('\\'); pos != string::npos; pos = name.find('\\', pos)) {
    name[pos] = '/';
  }
  return name;
}

string RteUtils::EnsureCrLf(const string& s)
{
  string tmp;
  string::size_type len = s.length();
  // ensure Windows line endings
  bool cr = false;
  bool lf = false;
  for (string::size_type pos = 0; pos < len; pos++) {
    char ch = s[pos];
    if (ch == '\r') {
      if (cr)              // previous char was also CR ("\r\r" case), add LF
        tmp += '\n';      // prepend LF
      tmp += ch;          // add CR itself
      if (pos + 1 == len)  // line ends with CR,
        tmp += '\n';      // append LF
      cr = true;
      lf = false;
    }
    else if (ch == '\n') {
      if (!cr)            // previous char was not CR
        tmp += '\r';     // prepend it
      lf = true;
      cr = false;
      tmp += ch;
    }
    else {
      if (cr && !lf) { // previous char was CR not followed by LF
        tmp += '\n';
      }
      cr = lf = false;
      tmp += ch;
    }
  }
  return tmp;
}

string RteUtils::EnsureLf(const std::string& s)
{
  string tmp(s);
  ReplaceAll(tmp, RteUtils::CRLF_STRING, RteUtils::LF_STRING);
  return ReplaceAll(tmp, RteUtils::CR_STRING, RteUtils::LF_STRING);
}

string RteUtils::ExpandInstancePlaceholders(const string& s, int count)
{
  static string INSTANCE = "%Instance%";
  static std::string::size_type len = INSTANCE.length();

  if (s.empty() || count == 0 || s.find(INSTANCE) == string::npos)
    return s;
  string result;
  for (int i = 0; i < count; i++) {
    string tmp = s;
    string num = to_string(i);
    for (std::string::size_type pos = tmp.find(INSTANCE); pos != string::npos; pos = tmp.find(INSTANCE)) {
      tmp = tmp.replace(pos, len, num);
    }
    result += tmp;
    if (tmp.rfind("\n") != (tmp.size() - 1))
      result += "\n";
  }
  return result;
}

string RteUtils::ExtractFileName(const string& fileName)
{
  string::size_type pos = fileName.find_last_of("\\/");
  if (pos == string::npos)
    return fileName;

  return fileName.substr(pos + 1);
}

string RteUtils::ExtractFilePath(const string& fileName, bool withTrailingSlash)
{
  string::size_type pos = fileName.find_last_of("\\/");
  if (pos == string::npos)
    return EMPTY_STRING;

  if (withTrailingSlash)
    pos++;
  return fileName.substr(0, pos);
}


string RteUtils::ExtractFileBaseName(const string& fileName)
{
  string name = ExtractFileName(fileName);
  string::size_type pos = name.find_last_of('.');
  if (pos == string::npos)
    return name;
  return name.substr(0, pos);
}

string RteUtils::ExtractFileExtension(const string& fileName, bool withDot)
{
  return GetSuffix(fileName, '.', withDot);
}

string RteUtils::AppendFileVersion(const std::string& fileName, const string& version, const string& versionPrefix)
{
  return fileName + '.' + versionPrefix + '@' + version;
}

string RteUtils::AppendFileBaseVersion(const std::string& fileName, const std::string& version)
{
  return AppendFileVersion(fileName, version, RteUtils::BASE_STRING);
}

string RteUtils::AppendFileUpdateVersion(const std::string& fileName, const std::string& version)
{
  return AppendFileVersion(fileName, version, RteUtils::UPDATE_STRING);
}


string RteUtils::ExtractFirstFileSegments(const string& fileName, int nSegments)
{
  if (nSegments <= 0) {
    return fileName;
  }
  string::size_type pos = 0;
  for (int i = 0; i < nSegments; i++) {
    pos = fileName.find_first_of("\\/", pos + 1);
    if (pos == string::npos)
      break;
  }
  if (pos == string::npos)
    return fileName;
  return fileName.substr(0, pos);
}

string RteUtils::ExtractLastFileSegments(const string& fileName, int nSegments)
{
  if (nSegments <= 0) {
    return fileName;
  }
  string::size_type pos = string::npos;
  for (int i = 0; i < nSegments; i++) {
    pos = fileName.find_last_of("\\/", pos - 1);
    if (pos == string::npos)
      break;
  }
  if (pos == string::npos)
    return fileName;
  return fileName.substr(pos + 1);
}

int  RteUtils::GetFileSegmentCount(const string& fileName) {
  if (fileName.empty())
    return 0;
  string::size_type pos = 0;
  int nSegments = 0;
  while (pos < fileName.size()) {
    nSegments++;
    pos = fileName.find_first_of("\\/", pos);
    if (pos == string::npos)
      break;
    pos++;
  }
  return nSegments;
}


int RteUtils::SegmentedPathCompare(const char* f1, const char* f2)
{
  if (!f1 || !f2)
    return 0;

  int n = 0;
  char* p1 = (char*)f1;
  char* p2 = (char*)f2;
  p1 += strlen(p1) - 1;
  p2 += strlen(p2) - 1;

  for (; p1 != f1 && p1 != f2; p1--, p2--) {
    if (toupper(*p1) != toupper(*p2))
      break;
    if (*p1 == '\\' || *p1 == '/')
      n++;
  }
  return n;
}

const char* RteUtils::GetIndent(unsigned indent)
{
  static char INDENT_BUF[64] = { '\0' };
  if (!INDENT_BUF[0]) {
    memset(INDENT_BUF, ' ', 63);
  }
  if (indent > 63)
    indent = 63;

  return &(INDENT_BUF[63 - indent]);
}

string RteUtils::ToXmlString(const map<string, string>& attributes)
{
  string s;
  for (auto [a, v] : attributes) {
    if (!s.empty())
      s += " ";
    s += a;
    s += "=\"";
    s += v;
    s += "\"";
  }
  return s;
}

string RteUtils::RemoveTrailingBackslash(const string& s) {
  if (s.empty() || (s[s.size() - 1] != '\\' && s[s.size() - 1] != '/')) {
    return s;
  }
  string result = s;
  while (!result.empty() && (result[result.size() - 1] == '\\' || result[result.size() - 1] == '/'))
    result.erase(result.size() - 1, 1);
  return result;
}


string RteUtils::RemoveQuotes(const string& s)
{
  string::size_type pos, pos1;
  pos = s.find("\"");
  // remove quotes if any
  if (pos != string::npos) {
    pos++;
    pos1 = s.find("\"", pos);
    if (pos1 != string::npos)
      return s.substr(pos, pos1 - pos);
  }
  return s;
}

std::string RteUtils::AddQuotesIfSpace(const std::string& s)
{
  static const string& QUOTE = string("\"");
  if (s.find(' ') != string::npos && s.find('\"') == string::npos){
    return QUOTE + s + QUOTE;
  }
  return s;
}

bool RteUtils::HasHexPrefix(const string& s)
{
  return s.length() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X');
}

string::size_type RteUtils::FindFirstDigit(const std::string& s) {
  return s.find_first_of("0123456789");
}

unsigned long RteUtils::ToUL(const string& s) {
  if (s.empty()) {
    return (0x0L);
  }
  if (HasHexPrefix(s)) {
    return stoul(s, 0, 16);                           // use base 16
  }
  return stoul(s, 0); // use auto
}

bool RteUtils::StringToBool(const std::string& value, bool defaultValue)
{
  if (value.empty())
    return defaultValue;
  return (value == "1" || value == "true");
}


int RteUtils::StringToInt(const std::string& value, int defaultValue)
{
  if (value.empty())
    return defaultValue;
  try {
    return std::stoi(value, 0, 0);
  }
  catch (const std::exception&) {
    return defaultValue;
  }
}

unsigned RteUtils::StringToUnsigned(const std::string& value, unsigned defaultValue)
{
  if (value.empty())
    return defaultValue;
  try {
    return std::stoul(value, 0, 0);
  }
  catch (const std::exception&) {
    return defaultValue;
  }
}

unsigned long long RteUtils::StringToULL(const std::string& value, unsigned long long defaultValue)
{
  if (value.empty()) {
    return (defaultValue);
  }
  int base = 10; // use 10 as default, prevent octal numbers
  if (HasHexPrefix(value)) {   // 0xnnnn
    base = 16; // use base 16 since it is a hex value
  }
  try {
    return std::stoull(value, 0, base);
  }
  catch (const std::exception&) {
    return defaultValue;
  }
}

std::string RteUtils::LongToString(long value, int radix)
{
  ostringstream ss;
  if (radix == 16) {
    ss << showbase << hex;
  }
  ss << value;
  return ss.str();
}

std::string RteUtils::Trim(const std::string& str) {
  static const char* whitespace = " \t\r\n";
  const auto begin = str.find_first_not_of(whitespace);
  if (begin == std::string::npos)
    return EMPTY_STRING;

  const auto end = str.find_last_not_of(whitespace);
  const auto range = end - begin + 1;
  return str.substr(begin, range);
}

string RteUtils::ConstructID(const std::vector<std::pair<const char*, const std::string&>>& elements) {
  string id;
  for (const auto& element : elements) {
    if (!element.second.empty()) {
      id += element.first + element.second;
    }
  }
  return id;
}

bool RteUtils::GetAccessSequence(size_t& offset, const string& src, string& sequence, const char start, const char end) {
  size_t delimStart = src.find_first_of(start, offset);
  if (delimStart != string::npos) {
    size_t delimEnd = src.find_first_of(end, ++delimStart);
    if (delimEnd != string::npos) {
      sequence = src.substr(delimStart, delimEnd - delimStart);
      offset = ++delimEnd;
      return true;
    } else {
      return false;
    }
  }
  offset = string::npos;
  return true;
}

void RteUtils::ApplyFilter(const vector<string>& origin, const set<string>& filter, vector<string>& result) {
  result.clear();
  for (const auto& item : origin) {
    bool match = true;
    for (const auto& word : filter) {
      if (word.empty()) {
        continue;
      }
      if (item.find(word) == string::npos) {
        match = false;
        break;
      }
    }
    if (match) {
      CollectionUtils::PushBackUniquely(result, item);
    }
  }
}

string RteUtils::RemoveLeadingSpaces(const string& input) {
  // the regex pattern to match leading spaces after a newline character
  static std::regex pattern(R"((\n)\s+)");

  // Replace leading spaces after newline characters
  // with a single newline character
  std::string result = std::regex_replace(input, pattern, "$1");
  return result;
}

// End of RteUtils.cpp
