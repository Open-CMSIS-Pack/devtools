/******************************************************************************/
/* RTE  -  CMSIS Run-Time Environment				                          */
/******************************************************************************/
/** @file  RteUtils.cpp
  * @brief CMSIS RTE Data model
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "RteUtils.h"

#include <cstring>

using namespace std;

const string RteUtils::EMPTY_STRING("");
const set<string> RteUtils::EMPTY_STRING_SET;
const list<string> RteUtils::EMPTY_STRING_LIST;
const vector<string> RteUtils::EMPTY_STRING_VECTOR;

const string RteUtils::ERROR_STRING("<ERROR>");
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
  for (auto it = s.begin(); it != s.end(); it++) {
    char ch = *it;
    if (!isalnum(ch) && ch != '-' && ch != '_' && ch != '/') // allowed characters
      return false;
  }
  return true;
}

std::string& RteUtils::ReplaceAll(std::string& str, const string& toReplace, const string& with)
{
  for (size_t pos = str.find(toReplace); pos != string::npos; pos = str.find(toReplace, pos + toReplace.size())) {
    str.replace(pos, toReplace.size(), with);
  }
  return str;
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

std::string RteUtils::AppendFileVersion(const std::string& fileName, const string& version, bool bHidden)
{
  if (!bHidden) {
    return fileName + '@' + version;
  }
  string path = ExtractFilePath(fileName, true);
  string name = ExtractFileName(fileName);
  return path + '.' + name + '@' + version;
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
  map<string, string>::const_iterator it;
  for (it = attributes.begin(); it != attributes.end(); it++) {
    if (!s.empty())
      s += " ";
    s += it->first;
    s += "=\"";
    s += it->second;
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

bool RteUtils::HasHexPrefix(const string& s)
{
  return s.length() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X');
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

unsigned long long RteUtils::ToULL(const string& s) {
  if (s.empty()) {
    return (0x0LL);
  }
  if (HasHexPrefix(s)) {
    return stoull(s, 0, 16);                           // use base 16
  }
  return stoull(s, 0); // use auto
}

string RteUtils::GetPackID(const string &path) {
  string res;
  size_t pos = path.find_last_of('/') + 1; // string::npos + 1 = 0 in case of file name without path
  size_t pos2 = path.find_last_of('.');
  if (pos2 != string::npos) {
    res = path.substr(pos, pos2 - pos);   // res := vendor.name.majv.minv.minv2
    if (res.find('.') != string::npos)
      res.replace(res.find('.'), 1, "::");
    if (res.find('.') != string::npos)
      res.replace(res.find('.'), 1, "::");
  }
  return res;
}

string RteUtils::VendorFromPackageId(const std::string &packageId) {
  // assumed packageId format is conform
  return std::string(packageId.substr(0, packageId.find_first_of('.')));
}

string RteUtils::NameFromPackageId(const std::string &packageId) {
  // assumed packageId format is conform
  size_t pos = packageId.find_first_of(".");
  return std::string(packageId.substr(pos + 1, packageId.find('.', pos+1) - pos - 1));
}

// End of RteUtils.cpp
