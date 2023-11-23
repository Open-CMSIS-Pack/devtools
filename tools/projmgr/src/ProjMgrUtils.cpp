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

const StrMap ProjMgrUtils::DeviceAttributesKeys = {
  { RTE_DFPU       , YAML_FPU               },
  { RTE_DDSP       , YAML_DSP               },
  { RTE_DMVE       , YAML_MVE               },
  { RTE_DENDIAN    , YAML_ENDIAN            },
  { RTE_DSECURE    , YAML_TRUSTZONE         },
  { RTE_DBRANCHPROT, YAML_BRANCH_PROTECTION },
};

const StrPairVecMap ProjMgrUtils::DeviceAttributesValues = {
  { RTE_DFPU       , {{ RTE_DP_FPU       , YAML_FPU_DP         },
                      { RTE_SP_FPU       , YAML_FPU_SP         },
                      { RTE_NO_FPU       , YAML_OFF            }}},
  { RTE_DDSP       , {{ RTE_DSP          , YAML_ON             },
                      { RTE_NO_DSP       , YAML_OFF            }}},
  { RTE_DMVE       , {{ RTE_FP_MVE       , YAML_MVE_FP         },
                      { RTE_MVE          , YAML_MVE_INT        },
                      { RTE_NO_MVE       , YAML_OFF            }}},
  { RTE_DENDIAN    , {{ RTE_ENDIAN_BIG   , YAML_ENDIAN_BIG     },
                      { RTE_ENDIAN_LITTLE, YAML_ENDIAN_LITTLE  }}},
  { RTE_DSECURE    , {{ RTE_SECURE       , YAML_TZ_SECURE      },
                      { RTE_NON_SECURE   , YAML_TZ_NON_SECURE  },
                      { RTE_TZ_DISABLED  , YAML_OFF            }}},
  { RTE_DBRANCHPROT, {{ RTE_BTI          , YAML_BP_BTI         },
                      { RTE_BTI_SIGNRET  , YAML_BP_BTI_SIGNRET },
                      { RTE_NO_BRANCHPROT, YAML_OFF            }}},
};

const string& ProjMgrUtils::GetDeviceAttribute(const string& key, const string& value) {
  const auto& values = DeviceAttributesValues.at(key);
  for (const auto& [rte, yaml] : values) {
    if (value == rte) {
      return yaml;
    } else if  (value == yaml) {
      return rte;
    }
  }
  return RteUtils::EMPTY_STRING;
}

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
    {"linkerScript", {".sct", ".scf", ".ld", ".icf", ".src"}},
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
    compilerRoot = RteFsUtils::MakePathCanonical(compilerRoot);
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

bool ProjMgrUtils::ConvertToPackInfo(const string& packId, PackInfo& packInfo) {
  string packInfoStr = packId;
  if (packInfoStr.find("::") != string::npos) {
    packInfo.vendor = RteUtils::RemoveSuffixByString(packInfoStr, "::");
    packInfoStr = RteUtils::RemovePrefixByString(packInfoStr, "::");
    packInfo.name   = RteUtils::GetPrefix(packInfoStr, '@');
  } else {
    packInfo.vendor = RteUtils::GetPrefix(packInfoStr, '@');
  }
  packInfo.version = RteUtils::GetSuffix(packInfoStr, '@');

  return true;
}

bool ProjMgrUtils::IsMatchingPackInfo(const PackInfo& exactPackInfo, const PackInfo& packInfoToMatch) {
  // Check if vendor matches
  if (packInfoToMatch.vendor != exactPackInfo.vendor) {
    // Not same vendor
    return false;
  }

  // Check if pack name matches
  if (!packInfoToMatch.name.empty()) {
    if (WildCards::IsWildcardPattern(packInfoToMatch.name)) {
      // Check if filter matches
      if (!WildCards::Match(packInfoToMatch.name, exactPackInfo.name)) {
        // Name filter does not match needle
        return false;
      }
    } else if (packInfoToMatch.name != exactPackInfo.name) {
      // Not same pack name
      return false;
    }
  }

  // Check if version matches
  string reqVersionRange = ConvertToVersionRange(packInfoToMatch.version);
  if (!reqVersionRange.empty() && VersionCmp::RangeCompare(exactPackInfo.version, reqVersionRange) != 0) {
    // Version out of range
    return false;
  }

  // Needle matches this resolved pack
  return true;
}

string ProjMgrUtils::ConvertToVersionRange(const string& version) {
  string versionRange = version;
  if (!versionRange.empty()) {
    if (versionRange.find(">=") != string::npos) {
      versionRange = versionRange.substr(2);
    } else {
      versionRange = versionRange + ":" + versionRange;
    }
  }
  return versionRange;
}
