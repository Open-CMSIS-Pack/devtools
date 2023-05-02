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

ProjMgrUtils::ProjMgrUtils(void) {
  // Reserved
}

ProjMgrUtils::~ProjMgrUtils(void) {
  // Reserved
}

string ProjMgrUtils::GetComponentID(const RteItem* component) {
  if (!component) {
    return RteUtils::EMPTY_STRING;
  }
  const auto& vendor = component->GetVendorString().empty() ? "" : component->GetVendorString() + SUFFIX_CVENDOR;
  const vector<pair<const char*, const string&>> elements = {
    {"",              vendor},
    {"",              component->GetCclassName()},
    {PREFIX_CBUNDLE,  component->GetCbundleName()},
    {PREFIX_CGROUP,   component->GetCgroupName()},
    {PREFIX_CSUB,     component->GetCsubName()},
    {PREFIX_CVARIANT, component->GetCvariantName()},
    {PREFIX_CVERSION, component->GetVersionString()},
  };
  return ConstructID(elements);
}

string ProjMgrUtils::GetConditionID(const RteItem* condition) {
  if (!condition) {
    return RteUtils::EMPTY_STRING;
  }
  return condition->GetTag() + " " + GetComponentID(condition);
}

string ProjMgrUtils::GetComponentAggregateID(const RteItem* component) {
  if (!component) {
    return RteUtils::EMPTY_STRING;
  }
  const auto& vendor = component->GetVendorString().empty() ? "" : component->GetVendorString() + SUFFIX_CVENDOR;
  const vector<pair<const char*, const string&>> elements = {
    {"",              vendor},
    {"",              component->GetCclassName()},
    {PREFIX_CBUNDLE,  component->GetCbundleName()},
    {PREFIX_CGROUP,   component->GetCgroupName()},
    {PREFIX_CSUB,     component->GetCsubName()},
  };
  return ConstructID(elements);
}

string ProjMgrUtils::GetPartialComponentID(const RteItem* component) {
  if (!component) {
    return RteUtils::EMPTY_STRING;
  }
  const vector<pair<const char*, const string&>> elements = {
    {"",              component->GetCclassName()},
    {PREFIX_CBUNDLE,  component->GetCbundleName()},
    {PREFIX_CGROUP,   component->GetCgroupName()},
    {PREFIX_CSUB,     component->GetCsubName()},
    {PREFIX_CVARIANT, component->GetCvariantName()},
  };
  return ConstructID(elements);
}

std::map<std::string, std::string> ProjMgrUtils::ComponentAttributesFromId(const std::string& componentId)
{
  XmlItem attributes;
  string id = componentId;
  if (componentId.find("::") != string::npos) {
    string vendor = RteUtils::RemoveSuffixByString(id, SUFFIX_CVENDOR);
    attributes.AddAttribute("Cvendor", vendor);
    id = RteUtils::RemovePrefixByString(componentId, SUFFIX_CVENDOR);
  }
  attributes.AddAttribute("Cversion", RteUtils::GetSuffix(id, PREFIX_CVERSION_CHAR));
  id = RteUtils::GetPrefix(id, PREFIX_CVERSION_CHAR);
  list<string> segments;
  RteUtils::SplitString(segments, id, ':');
  size_t index = 0;
  for (auto s : segments) {
    switch (index) {
      case 0: // Cclass[&Cbundle]
        attributes.AddAttribute("Cclass", RteUtils::GetPrefix(s, PREFIX_CBUNDLE_CHAR));
        attributes.AddAttribute("Cbundle", RteUtils::GetSuffix(s, PREFIX_CBUNDLE_CHAR), false);
        break;
      case 1: // Cgroup[&Cvariant]
        attributes.AddAttribute("Cgroup", RteUtils::GetPrefix(s, PREFIX_CVARIANT_CHAR));
        attributes.AddAttribute("Cvariant", RteUtils::GetSuffix(s, PREFIX_CVARIANT_CHAR), false);
        break;
      case 2: // Csub
        attributes.AddAttribute("Csub", RteUtils::GetPrefix(s, PREFIX_CVARIANT_CHAR));
        attributes.AddAttribute("Cvariant", RteUtils::GetSuffix(s, PREFIX_CVARIANT_CHAR), false);
        break;
      default:
      break;
    };
    index++;
  }

  return attributes.GetAttributes();
}


string ProjMgrUtils::GetPackageID(const RteItem* pack) {
  if (!pack) {
    return RteUtils::EMPTY_STRING;
  }
  const auto& vendor = pack->GetVendorString().empty() ? "" : pack->GetVendorString() + SUFFIX_PACK_VENDOR;
  const vector<pair<const char*, const string&>> elements = {
    {"",                  vendor},
    {"",                  pack->GetName()},
    {PREFIX_PACK_VERSION, pack->GetVersionString()},
  };
  return ConstructID(elements);
}

string ProjMgrUtils::GetPackageID(const string& packVendor, const string& packName, const string& packVersion) {
  const auto& vendor = packVendor + SUFFIX_PACK_VENDOR;
  const vector<pair<const char*, const string&>> elements = {
    {"",                  vendor},
    {"",                  packName},
    {PREFIX_PACK_VERSION, packVersion},
  };
  return ConstructID(elements);
}

string ProjMgrUtils::ConstructID(const std::vector<std::pair<const char*, const std::string&>>& elements) {
  string id;
  for (const auto& element : elements) {
    if (!element.second.empty()) {
      id += element.first + element.second;
    }
  }
  return id;
}

RtePackage* ProjMgrUtils::ReadGpdscFile(const string& gpdsc) {
  fs::path path(gpdsc);
  error_code ec;
  if (fs::exists(path, ec)) {
    RteItemBuilder rteItemBuilder(nullptr, PackageState::PS_GENERATED);
    XMLTreeSlim tree(&rteItemBuilder);
    tree.Init();
    bool success = tree.AddFileName(gpdsc, true);
    RtePackage* gpdscPack = rteItemBuilder.GetPack();
    if (success && gpdscPack && gpdscPack->Validate()) {
      return gpdscPack;
    } else if (gpdscPack) {
      delete gpdscPack;
    }
  }
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
