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

#include <regex>

using namespace std;

static constexpr const char* HIGHER_OR_EQUAL_OPERATOR = ">=";
static constexpr const char* TILDE_OPERATOR = "~";
static constexpr const char* CARET_OPERATOR = "^";

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
  if (typeString == RteConstants::OUTPUT_TYPE_BIN) {
    type.bin.on = true;
  } else if (typeString == RteConstants::OUTPUT_TYPE_ELF) {
    type.elf.on = true;
  } else if (typeString == RteConstants::OUTPUT_TYPE_HEX) {
    type.hex.on = true;
  } else if (typeString == RteConstants::OUTPUT_TYPE_LIB) {
    type.lib.on = true;
  } else if (typeString == RteConstants::OUTPUT_TYPE_CMSE) {
    type.cmse.on = true;
  } else if (typeString == RteConstants::OUTPUT_TYPE_MAP) {
    type.map.on = true;
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
      CollectionUtils::PushBackUniquely(selectedContexts, "");
    }
    else {
      // select all contexts
      for (const auto& context : allContexts) {
        CollectionUtils::PushBackUniquely(selectedContexts, context);
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
        CollectionUtils::PushBackUniquely(selectedContexts, context);
        });
    }
  }

  if (unmatchedFilters.size() > 0) {
    error.m_errMsg = "no matching context found for option:\n";
    for (const auto& filter : unmatchedFilters) {
      error.m_errMsg += "  --context " + filter + "\n";
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
        CollectionUtils::PushBackUniquely(selectedContexts, context);
        continue;
      }

      // match contexts
      ContextName contextItem;
      ProjMgrUtils::ParseContextEntry(context, contextItem);
      contextPattern = (inputContext.project != RteUtils::EMPTY_STRING ? inputContext.project : "*");
      contextPattern += "." + (inputContext.build != RteUtils::EMPTY_STRING ? inputContext.build : "*");
      contextPattern += "+" + (inputContext.target != RteUtils::EMPTY_STRING ? inputContext.target : "*");
      const string fullContextItem = contextItem.project + "." + contextItem.build + "+" + contextItem.target;
      if (WildCards::Match(fullContextItem, contextPattern)) {
        CollectionUtils::PushBackUniquely(selectedContexts, context);
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

SemVer ProjMgrUtils::GetSemVer(const std::string version) {
  static const regex regEx = regex("(\\d+).(\\d+).(\\d+).*");
  smatch sm;
  regex_match(version, sm, regEx);
  SemVer semVer = {};
  if (sm.size() == 4) {
    semVer.major = stoi(sm[1]);
    semVer.minor = stoi(sm[2]);
    semVer.patch = stoi(sm[3]);
  }
  return semVer;
}

string ProjMgrUtils::ConvertToVersionRange(const string& version) {
  string versionRange = version;
  if (!versionRange.empty()) {
    if (versionRange.find(HIGHER_OR_EQUAL_OPERATOR) != string::npos) {
      // Minimum version
      versionRange = RteUtils::RemovePrefixByString(versionRange, HIGHER_OR_EQUAL_OPERATOR);
    } else if (versionRange.find(TILDE_OPERATOR) != string::npos) {
      // Equivalent version
      versionRange = RteUtils::RemovePrefixByString(versionRange, TILDE_OPERATOR);
      SemVer semVer = GetSemVer(versionRange);
      versionRange = versionRange + ":" + to_string(semVer.major) + "." + to_string(semVer.minor + 1) + ".0-0";
    } else if (versionRange.find(CARET_OPERATOR) != string::npos) {
      // Compatible version
      versionRange = RteUtils::RemovePrefixByString(versionRange, CARET_OPERATOR);
      SemVer semVer = GetSemVer(versionRange);
      versionRange = versionRange + ":" + to_string(semVer.major + 1) + ".0.0-0";
    } else {
      // Fixed version
      versionRange = versionRange + ":" + versionRange;
    }
  }
  return versionRange;
}

StrMap ProjMgrUtils::CreateIOSequenceMap(const vector<ExecutesItem>& executes) {
  StrMap IOSeqMap = {
    {"input", "${INPUT}"},
    {"output", "${OUTPUT}"}
  };
  vector<int> inputSizes, outputSizes;
  for (auto& item : executes) {
    inputSizes.push_back(item.input.size());
    outputSizes.push_back(item.output.size());
  }
  if (inputSizes.size() > 0) {
    for (int i = 0; i < *max_element(inputSizes.begin(), inputSizes.end()); i++) {
      IOSeqMap["input(" + to_string(i) + ")"] = "${INPUT_" + to_string(i) + "}";
    }
  }
  if (outputSizes.size() > 0) {
    for (int i = 0; i < *max_element(outputSizes.begin(), outputSizes.end()); i++) {
      IOSeqMap["output(" + to_string(i) + ")"] = "${OUTPUT_" + to_string(i) + "}";
    }
  }
  return IOSeqMap;
}

string ProjMgrUtils::ReplaceDelimiters(const string input) {
  regex regEx = regex("::|:|&|@>=|@|\\.|/| ");
  return(regex_replace(input, regEx, "_"));
}

const string ProjMgrUtils::FindReferencedContext(const string& currentContext, const string& refContext, const StrVec& selectedContexts) {
  if (refContext.empty()) {
    return currentContext;
  }
  ContextName currentContextName, refContextName;
  ProjMgrUtils::ParseContextEntry(currentContext, currentContextName);
  ProjMgrUtils::ParseContextEntry(refContext, refContextName);
  string refContextFound;
  if (refContextName.project.empty()) {
    refContextName.project = currentContextName.project;
  }
  if (refContextName.target.empty()) {
    refContextName.target = currentContextName.target;
  }
  for (const auto& selectedContext : selectedContexts) {
    ContextName selectedContextName;
    ProjMgrUtils::ParseContextEntry(selectedContext, selectedContextName);
    if ((refContextName.project != selectedContextName.project) ||
      (refContextName.target != selectedContextName.target) ||
      (!refContextName.build.empty() && refContextName.build != selectedContextName.build)) {
      // skip incompatible contexts
      continue;
    }
    refContextFound = selectedContext;
    if ((!refContextName.build.empty() && refContextName.build == selectedContextName.build) ||
      (refContextName.build.empty() && currentContextName.build == selectedContextName.build)) {
      // best match is found, don't search further
      break;
    }
  }
  return refContextFound;
}

bool ProjMgrUtils::HasAccessSequence(const string value) {
  return regex_match(value, regex(".*\\$.*\\$.*"));
}

const string ProjMgrUtils::FormatPath(const string& original, const string& directory, bool useAbsolutePaths) {
  if (original.find("http") == 0) {
    return original;
  }
  string path = RteFsUtils::MakePathCanonical(original);
  RteFsUtils::NormalizePath(path);
  if (!useAbsolutePaths) {
    string packRoot = ProjMgrKernel::Get()->GetCmsisPackRoot();
    size_t index = path.find(packRoot);
    if (index != string::npos) {
      path.replace(index, packRoot.length(), "${CMSIS_PACK_ROOT}");
    } else {
      string compilerRoot;
      ProjMgrUtils::GetCompilerRoot(compilerRoot);
      index = path.find(compilerRoot);
      if (!compilerRoot.empty() && index != string::npos) {
        path.replace(index, compilerRoot.length(), "${CMSIS_COMPILER_ROOT}");
      } else {
        error_code ec;
        const string relative = fs::relative(path, directory, ec).generic_string();
        if (!ec && !relative.empty()) {
          path = relative;
        }
      }
    }
  }
  return path;
}
