/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgrUtils.h"

#include "RteFsUtils.h"
#include "RteItem.h"
#include "RteItemBuilder.h"
#include "XMLTreeSlim.h"
#include "CrossPlatform.h"
#include "CrossPlatformUtils.h"

#include <array>
#include <functional>
#include <regex>

using namespace std;

RtePackage* ProjMgrUtils::ReadGpdscFile(const string& gpdsc, bool& valid) {
  fs::path path(gpdsc);
  error_code ec;
  if (fs::exists(path, ec)) {
    RtePackage* gpdscPack = ProjMgrKernel::Get()->LoadPack(gpdsc, PackageState::PS_GENERATED);
    if (gpdscPack) {
      if (gpdscPack->Validate()) {
        valid = true;
        return gpdscPack;
      } else {
        ProjMgrCallback* callback = ProjMgrKernel::Get()->GetCallback();
        RtePrintErrorVistior visitor(callback);
        gpdscPack->AcceptVisitor(&visitor);
        if (callback->GetErrorMessages().empty()) {
          // validation failed but there are no errors, don't delete gpdscPack
          valid = false;
          return gpdscPack;
        }
      }
      delete gpdscPack;
    }
  }
  valid = false;
  return nullptr;
}

const ProjMgrUtils::Result ProjMgrUtils::ExecCommand(const string& cmd) {
  array<char, 128> buffer;
  string result;
  int ret_code = -1;
  std::function<int(FILE*)> close = _pclose;
  std::function<FILE* (const char*, const char*)> open = _popen;

  auto deleter = [&close, &ret_code](FILE* cmd) { ret_code = close(cmd); };
  {
    const unique_ptr<FILE, decltype(deleter)> pipe(open(cmd.c_str(), "r"), deleter);
    if (pipe) {
      while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
        result += buffer.data();
      }
    }
  }
  return make_pair(result, ret_code);
}

const string ProjMgrUtils::GetCategory(const string& file) {
  static const map<string, vector<string>> CATEGORIES = {
    {"sourceC", {".c", ".C"}},
    {"sourceCpp", {".cpp", ".c++", ".C++", ".cxx", ".cc", ".CC"}},
    {"sourceAsm", {".asm", ".s", ".S"}},
    {"header", {".h", ".hpp"}},
    {"library", {".a", ".lib"}},
    {"object", {".o"}},
    {"linkerScript", {".sct", ".scf", ".ld", ".icf"}},
    {"doc", {".txt", ".md", ".pdf", ".htm", ".html"}},
  };
  fs::path ext((fs::path(file)).extension());
  for (const auto& category : CATEGORIES) {
    if (find(category.second.begin(), category.second.end(), ext) != category.second.end()) {
      return category.first;
    }
  }
  return "other";
}

void ProjMgrUtils::PushBackUniquely(vector<string>& vec, const string& value) {
  if (find(vec.cbegin(), vec.cend(), value) == vec.cend()) {
    vec.push_back(value);
  }
}

void ProjMgrUtils::PushBackUniquely(list<string>& list, const string& value) {
  if (find(list.cbegin(), list.cend(), value) == list.cend()) {
    list.push_back(value);
  }
}

void ProjMgrUtils::PushBackUniquely(StrPairVec& vec, const StrPair& value) {
  for (const auto& item : vec) {
    if ((value.first == item.first) && (value.second == item.second)) {
      return;
    }
  }
  vec.push_back(value);
}

StrVecMap ProjMgrUtils::MergeStrVecMap(const StrVecMap& map1, const StrVecMap& map2) {
  StrVecMap mergedMap(map1);
  mergedMap.insert(map2.begin(), map2.end());
  return mergedMap;
}

int ProjMgrUtils::StringToInt(const string& value) {
  int intValue = 0;
  smatch sm;
  if (regex_match(value, sm, regex("^[\\+]?([0-9]+)$"))) {
    try {
      intValue = stoi(sm[1]);
    }
    catch (exception&) {};
  }
  return intValue;
}


void ProjMgrUtils::ExpandCompilerId(const string& compiler, string& name, string& minVer, string& maxVer) {
  name = RteUtils::GetPrefix(compiler, '@');
  string version = RteUtils::GetSuffix(compiler, '@');
  if (version.empty()) {
    // any version
    minVer = "0.0.0";
  } else {
    if (version.find(">=") != string::npos) {
      // minimum version
      minVer = version.substr(2);
    } else {
      // fixed version
      minVer = maxVer = version;
    }
  }
}

bool ProjMgrUtils::AreCompilersCompatible(const string& first, const string& second) {
  if (!first.empty() && !second.empty()) {
    string firstName, firstMin, firstMax, secondName, secondMin, secondMax;
    ExpandCompilerId(first, firstName, firstMin, firstMax);
    ExpandCompilerId(second, secondName, secondMin, secondMax);
    if ((firstName != secondName) ||
      (!firstMax.empty() && !secondMin.empty() && (VersionCmp::Compare(firstMax, secondMin) < 0)) ||
      (!secondMax.empty() && !firstMin.empty() && (VersionCmp::Compare(secondMax, firstMin) < 0))) {
      return false;
    }
  }
  return true;
}

void ProjMgrUtils::CompilersIntersect(const string& first, const string& second, string& intersection) {
  if ((first.empty() && second.empty()) ||
    !AreCompilersCompatible(first, second)) {
    return;
  }
  string firstName, firstMin, firstMax, secondName, secondMin, secondMax;
  ExpandCompilerId(first, firstName, firstMin, firstMax);
  ExpandCompilerId(second, secondName, secondMin, secondMax);
  // get intersection
  if (firstMax.empty()) {
    firstMax = secondMax;
  }
  if (secondMax.empty()) {
    secondMax = firstMax;
  }
  const string& intersectName = firstName.empty() ? secondName : firstName;
  const string& intersectMin = VersionCmp::Compare(firstMin, secondMin) < 0 ? secondMin : firstMin;
  const string& intersectMax = VersionCmp::Compare(firstMax, secondMax) > 0 ? secondMax : firstMax;
  if (intersectMax.empty()) {
    if (VersionCmp::Compare(intersectMin, "0.0.0") == 0) {
      // any version
      intersection = intersectName;
    } else {
      // minimum version
      intersection = intersectName + "@>=" + intersectMin;
    }
  } else {
    if (intersectMin == intersectMax) {
      // fixed version
      intersection = intersectName + "@" + intersectMin;
    }
  }
}

void ProjMgrUtils::GetCompilerRoot(string& compilerRoot) {
  compilerRoot = CrossPlatformUtils::GetEnv("CMSIS_COMPILER_ROOT");
  if (compilerRoot.empty()) {
    error_code ec;
    string exePath = RteUtils::ExtractFilePath(CrossPlatformUtils::GetExecutablePath(ec), true);
    compilerRoot = fs::path(exePath).parent_path().parent_path().append("etc").generic_string();
    if (!RteFsUtils::Exists(compilerRoot)) {
      compilerRoot.clear();
    }
  }
  if (!compilerRoot.empty()) {
    error_code ec;
    compilerRoot = fs::weakly_canonical(fs::path(compilerRoot), ec).generic_string();
  }
}

bool ProjMgrUtils::ParseContextEntry(const string& contextEntry, ContextName& context) {
  // validate context
  if (RteUtils::CountDelimiters(contextEntry, ".") > 1 ||
    RteUtils::CountDelimiters(contextEntry, "+") > 1) {
    return false;
  }
  vector<pair<const regex, string&>> regexMap = {
    // "project name" may come before dot (.) or plus (+) or alone
    {regex(R"(^(.*?)[.+].*$|^(.*)$)"), context.project},
    // "build type" comes after dot (.) and may be succeeded by plus (+)
    {regex(R"(^.*\.(.*)\+.*$|^.*\.(.*).*$)"), context.build},
    // "target type" comes after plus (+) and may be succeeded by dot (.)
    {regex(R"(^.*\+(.*)\..*$|^.*\+(.*).*$)"), context.target},
  };
  for (auto& [regEx, stringReference] : regexMap) {
    smatch sm;
    regex_match(contextEntry, sm, regEx);
    // for every element 2 capture groups are exclusively possible - see above regex groups between brackets (.*)
    // the stringReference (context.project, context.build or context.target) is set to the "matched" group
    stringReference = sm.size() < 3 ? string() : sm[1].matched ? sm[1] : sm[2].matched ? sm[2] : string();
  }
  return true;
}

void ProjMgrUtils::SetOutputType(const string typeString, OutputTypes& type) {
  if (typeString == OUTPUT_TYPE_BIN) {
    type.bin.on = true;
  } else if (typeString == OUTPUT_TYPE_ELF) {
    type.elf.on = true;
  } else if (typeString == OUTPUT_TYPE_HEX) {
    type.hex.on = true;
  } else if (typeString == OUTPUT_TYPE_LIB) {
    type.lib.on = true;
  } else if (typeString == OUTPUT_TYPE_CMSE) {
    type.cmse.on = true;
  }
}

ProjMgrUtils::Error ProjMgrUtils::GetSelectedContexts(vector<string>& selectedContexts,
  const vector<string>& allContexts, const vector<string>& contextFilters)
{
  Error error;
  vector<string> unmatchedFilters;
  selectedContexts.clear();

  if (contextFilters.empty()) {
    if (allContexts.empty()) {
      // default context
      ProjMgrUtils::PushBackUniquely(selectedContexts, "");
    }
    else {
      // select all contexts
      for (const auto& context : allContexts) {
        ProjMgrUtils::PushBackUniquely(selectedContexts, context);
      }
    }
  }
  else {
    for (const auto& contextFilter : contextFilters) {
      auto filteredContexts = GetFilteredContexts(allContexts, contextFilter);
      if (filteredContexts.size() == 0) {
        unmatchedFilters.push_back(contextFilter);
        continue;
      }

      // append element to the output list
      for_each(filteredContexts.begin(), filteredContexts.end(), [&](const string& context) {
        ProjMgrUtils::PushBackUniquely(selectedContexts, context);
        });
    }
  }

  if (unmatchedFilters.size() > 0) {
    error.m_errMsg = "invalid context name(s). Following context name(s) was not found:\n";
    for (const auto& filter : unmatchedFilters) {
      error.m_errMsg += "  " + filter + "\n";
    }
  }
  return error;
}

ProjMgrUtils::Error ProjMgrUtils::ReplaceContexts(vector<string>& selectedContexts,
  const vector<string>& allContexts, const string& contextReplace)
{
  Error error;
  if (contextReplace == RteUtils::EMPTY_STRING) {
    return error;
  }

  // validate if the replace filter is valid
  auto replaceContexts = GetFilteredContexts(allContexts, contextReplace);
  if (replaceContexts.size() == 0) {
    error.m_errMsg = "invalid context replacement name. \"" + contextReplace + "\" was not found.\n";
    selectedContexts.clear();
    return error;
  }

  // no replacement needed when replacement contexts list
  // is exactly same as selected contexts list
  if (selectedContexts.size() == replaceContexts.size()) {
    set<string> sortedSelectedContexts(selectedContexts.begin(), selectedContexts.end());
    set<string> sortedReplaceContexts(replaceContexts.begin(), replaceContexts.end());
    if (sortedSelectedContexts == sortedReplaceContexts) {
      // no replacement needed
      return error;
    }
  }

  // validate if replace filter requests more contexts than selected contexts
  if (selectedContexts.size() < replaceContexts.size()) {
    error.m_errMsg = "invalid replacement request. Replacement contexts are more than the selected contexts";
    selectedContexts.clear();
    return error;
  }

  ContextName replaceFilter;
  ProjMgrUtils::ParseContextEntry(contextReplace, replaceFilter);

  // validate if the replace filter has contexts which are not matching with selected context list
  bool found = false;
  for (auto& selectedContext : selectedContexts) {
    ContextName context;
    ProjMgrUtils::ParseContextEntry(selectedContext, context);
    if (replaceFilter.project.empty() ||
      WildCards::Match(replaceFilter.project, context.project)) {
      found = true;
      break;
    }
  }
  if (!found) {
    error.m_errMsg = "no suitable replacement found for context replacement \"" + contextReplace + "\"";
    selectedContexts.clear();
    return error;
  }

  if (replaceFilter.project.empty()) {
    replaceFilter.project = "*";
  }

  // get a list of contexts needs to be replaced
  vector<string> filteredContexts;
  ContextName repContextItem, selContextItem;
  for (string& selectedContext : selectedContexts) {
    ProjMgrUtils::ParseContextEntry(selectedContext, selContextItem);
    for (const string& contextItr : replaceContexts) {
      if ((!replaceFilter.project.empty()) && (!WildCards::Match(replaceFilter.project, selContextItem.project))) {
        // if 'project name' does not match the replace filter push it into the results and iterate further
        ProjMgrUtils::PushBackUniquely(filteredContexts, selectedContext);
        continue;
      }

      ProjMgrUtils::ParseContextEntry(contextItr, repContextItem);
      // construct context name for replacement
      string replaceContext = selContextItem.project;
      if (!repContextItem.build.empty()) {
        replaceContext += "." + repContextItem.build;
      }
      replaceContext += "+" + repContextItem.target;

      if (find(allContexts.begin(), allContexts.end(), replaceContext) != allContexts.end()) {
        // if 'replaceContext' is valid push it into the results
        ProjMgrUtils::PushBackUniquely(filteredContexts, replaceContext);
      }
    }
  }

  if (filteredContexts.size() == 0) {
    // no match found
    error.m_errMsg = "no suitable replacements found for context replacement \"" + contextReplace + "\"";
    selectedContexts.clear();
    return error;
  }

  if (selectedContexts.size() != filteredContexts.size()) {
    // incompatible change
    error.m_errMsg = "incompatible replacements found for \"" + contextReplace + "\"";
    selectedContexts.clear();
    return error;
  }

  // update selected contexts with the filtered ones
  selectedContexts = filteredContexts;
  return error;
}

vector<string> ProjMgrUtils::GetFilteredContexts(
  const vector<string>& allContexts, const string& contextFilter)
{
  vector<string> selectedContexts;
  string contextPattern;
  ContextName inputContext;

  if (ProjMgrUtils::ParseContextEntry(contextFilter, inputContext)) {
    for (const auto& context : allContexts) {
      //  add context to output list if exact match
      if (context == contextFilter) {
        ProjMgrUtils::PushBackUniquely(selectedContexts, context);
        continue;
      }

      // match contexts
      ContextName contextItem;
      ProjMgrUtils::ParseContextEntry(context, contextItem);
      contextPattern = (inputContext.project != RteUtils::EMPTY_STRING ? inputContext.project : "*");
      if (contextItem.build != RteUtils::EMPTY_STRING) {
        contextPattern += "." + (inputContext.build != RteUtils::EMPTY_STRING ? inputContext.build : "*");
      }
      contextPattern += "+" + (inputContext.target != RteUtils::EMPTY_STRING ? inputContext.target : "*");
      if (WildCards::Match(context, contextPattern)) {
        ProjMgrUtils::PushBackUniquely(selectedContexts, context);
      }
    }
  }
  return selectedContexts;
}
