/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteGenerator.cpp
* @brief CMSIS RTE Data Model
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "RteGenerator.h"

#include "RteFile.h"
#include "RteTarget.h"
#include "RteProject.h"

#include "RteFsUtils.h"
#include "XMLTree.h"

#include "CrossPlatformUtils.h"

using namespace std;

RteGenerator::RteGenerator(RteItem* parent, bool bExternal) :
  RteItem(parent),
  m_pDeviceAttributes(nullptr),
  m_files(nullptr),
  m_bExternal(bExternal)
{
  RteGenerator::Clear();
}


RteGenerator::~RteGenerator()
{
  RteGenerator::Clear();
}

void RteGenerator::Clear()
{
  m_files = 0;
  RteItem::Clear();
}

bool RteGenerator::HasExe() const
{
  return !GetCommand().empty();
}

bool RteGenerator::HasWeb() const
{
  RteItem* item = GetItemByTag("web");
  return item != NULL;
}


string RteGenerator::GetGeneratorGroupName() const
{
  string generatorGroup(":");
  generatorGroup += GetName();
  if(!IsExternal()) {
    generatorGroup += ":Common Sources";
  }
  return generatorGroup;
}

const string& RteGenerator::GetGpdsc() const
{
  RteItem* item = GetItemByTag("gpdsc");
  if (item)
    return item->GetName();

  return EMPTY_STRING;
}

const std::string& RteGenerator::GetWorkingDir() const
{
  if(IsExternal()) {
    return GetPathAttribute();
  }
  return GetItemValue("workingDir");
}

RteItem* RteGenerator::GetArgumentsItem(const string& type) const
{
  if (type.empty() || type == "exe") {
    RteItem* exe = GetItemByTag("exe");
    if (exe != NULL) {
      return exe;
    }
  } else {
    return GetItemByTag(type);
  }

  RteItem* args = GetItemByTag("arguments");
  return args;
}

const string RteGenerator::GetCommand(const std::string& hostType) const
{
  if(IsExternal()) {
    return GetRunAttribute();
  }

  RteItem* exe = GetItemByTag("exe");
  if (exe != NULL) {
    for (auto cmd : exe->GetChildren()) {
      if (cmd->GetTag() != "command" || !cmd->MatchesHost(hostType)) {
        continue;
      }
      const string& key = cmd->GetAttribute("key");
      string keyValue;
      if (!key.empty()) {
        keyValue = RteUtils::BackSlashesToSlashes(CrossPlatformUtils::GetRegistryString(key));
        if (RteUtils::ExtractFileName(keyValue) == cmd->GetText()) {
          return keyValue; // key already contains full path to the executable
        }
      }
      if (!keyValue.empty()) {
        // treat as a path, append '/' if needed
        if (keyValue.back() != '/') {
          keyValue += '/';
        }
      }
      return keyValue + cmd->GetText();
    }
    return EMPTY_STRING;
  }

  return GetItemValue("command");
}

string RteGenerator::GetExecutable(RteTarget* target, const std::string& hostType) const
{
  string cmd = GetCommand(hostType);
  if (cmd.empty())
    return cmd;

  if (cmd.find("http:") == 0 || cmd.find("https:") == 0) // a URL?
    return EMPTY_STRING; // return empty string here , GetExpandedWebLine() will return URL then

  if(target && IsExternal()) {
    cmd = target->ExpandAccessSequences(cmd);
  }else {
    cmd = ExpandString(cmd);
  }

  if (RteFsUtils::IsRelative(cmd)) {
     cmd = RteFsUtils::MakePathCanonical(GetAbsolutePackagePath() + cmd);
  }

  bool bWindows = "win" == (hostType.empty() ? CrossPlatformUtils::GetHostType() : hostType);

  // check if generator executable has an extension
  if(bWindows && RteUtils::ExtractFileExtension(RteUtils::ExtractFileName(cmd)).empty()) {
    // try to find file with correct extension
    for(auto ext : {".com", ".exe", ".bat"}) {
      string file = cmd + ext;
      if(RteFsUtils::Exists(file)) {
        return file;
      }
    }
  }
  return cmd;
}


vector<pair<string, string> > RteGenerator::GetExpandedArguments(RteTarget* target, const string& hostType, bool dryRun) const
{
  vector<pair<string, string> > args;
  if(IsExternal()) {
  // add cbuild-gen-idx.yml
    string idxFile = target->ExpandAccessSequences("$SolutionDir$/$TargetType$.cbuild-gen-idx.yml");
    args.push_back({RteUtils::EMPTY_STRING, idxFile});
    return args;
  }

  RteItem* argsItem = GetArgumentsItem("exe");
  if (argsItem) {
    for (auto arg : argsItem->GetChildren()) {
      if (arg->GetTag() != "argument" || !arg->MatchesHost(hostType))
        continue;
      if (!dryRun && arg->GetAttribute("mode") == "dry-run")
        continue;
      args.push_back({arg->GetAttribute("switch"), ExpandString(arg->GetText())});
    }
  }
  return args;
}

string RteGenerator::GetExpandedCommandLine(RteTarget* target, const string& hostType, bool dryRun) const
{
  string fullCmd = RteUtils::AddQuotesIfSpace(GetExecutable(target, hostType));
  const vector<pair<string, string> > args = GetExpandedArguments(target, hostType, dryRun);
  for (size_t i = 0; i < args.size(); i++) {
    fullCmd += ' ' + RteUtils::AddQuotesIfSpace(args[i].first + args[i].second);
  }
  return fullCmd;
}

string RteGenerator::GetExpandedWebLine(RteTarget* target) const
{
  RteItem* item = GetItemByTag("web");
  if (item == NULL) {
    const string& cmd = GetItemValue("command");
    if (cmd.find("http:") == 0 || cmd.find("https:") == 0)
      return cmd; // workaround for backward compatibility
    return EMPTY_STRING;
  }

  string url = item->GetURL();
  if (url.empty())
    return url;

  RteItem* argsItem = GetArgumentsItem("web");
  if (argsItem) {
    char delimiter = '?'; // first delimiter after URL base
    for (auto arg : argsItem->GetChildren()) {
      if (arg->GetTag() != "argument")
        continue;
      if (!arg->MatchesHost()) // should always match, added just for consistency
        continue;
      url += delimiter;
      const string& key = arg->GetAttribute("switch");
      if (!key.empty()) { // actually schema defines switch as required
        url += key;
        url += "=";
      }
      string value = ExpandString(arg->GetText());
      url += value;
      delimiter = '&'; // delimiter between arguments
    }
  }
  return url;
}


string RteGenerator::GetExpandedGpdsc(RteTarget* target, const string& genDir) const
{
  string gpdsc;
  if(IsExternal()) {
    gpdsc = target->GetProject()->GetName() + ".cgen.yml";
  } else {
    gpdsc = GetGpdsc();
    if(gpdsc.empty()) {
      gpdsc = target->GetProject()->GetName() + ".gpdsc";
    } else {
      gpdsc = ExpandString(gpdsc);
    }
  }

  if (!genDir.empty() && fs::path(gpdsc).is_absolute()) {
    error_code ec;
    gpdsc = fs::relative(gpdsc, GetExpandedWorkingDir(target), ec).generic_string();
  }

  if (fs::path(gpdsc).is_relative()) {
    gpdsc = GetExpandedWorkingDir(target, genDir) + gpdsc;
  }

  return RteFsUtils::MakePathCanonical(gpdsc);
}

string RteGenerator::GetExpandedWorkingDir(RteTarget* target, const string& genDir) const
{
  string wd = genDir.empty() ? ExpandString(GetWorkingDir(), IsExternal(), target) : genDir;
  fs::path path(wd);
  if (wd.empty() || (path.is_relative() && !IsExternal())) {
    // use project directory
    wd = target->GetProject()->GetProjectPath() + wd;
  }
  if (!wd.empty() && wd[wd.length() - 1] != '/')
    wd += "/";
  return wd;
}

void RteGenerator::Construct()
{
  RteItem::Construct();
  if (m_files) {
    m_files->AddAttribute("name", GetGeneratorGroupName());
  }
}

RteItem* RteGenerator::CreateItem(const std::string& tag)
{
  if (tag == "project_files") {
    if (!m_files) {
      m_files = new RteFileContainer(this);
      m_files->AddAttribute("name", GetGeneratorGroupName());
      return m_files;
    }
  } else if (tag == "select") {
    m_pDeviceAttributes = new RteItem(this);
    return m_pDeviceAttributes;
  } else if (tag == "arguments" || tag == "exe" || tag == "web" || tag == "eclipse") {
    return new RteItem(this);
  }
  return RteItem::CreateItem(tag);
}


bool RteGenerator::IsDryRunCapable(const std::string& hostType) const
{
  RteItem* argsItem = GetArgumentsItem("exe");
  if (argsItem) {
    for (auto arg : argsItem->GetChildren()) {
      if (arg->GetTag() != "argument" || !arg->MatchesHost(hostType))
        continue;
      if (arg->GetAttribute("mode") == "dry-run")
        return true;
    }
  }
  return false;
}

RteGeneratorContainer::RteGeneratorContainer(RteItem* parent) :
  RteItem(parent)
{
}

RteGenerator* RteGeneratorContainer::GetGenerator(const string& id) const
{
  return dynamic_cast<RteGenerator*>(GetItem(id));
}

RteItem* RteGeneratorContainer::CreateItem(const std::string& tag)
{
  if (tag == "generator") {
    return new RteGenerator(this);
  }
  return RteItem::CreateItem(tag);
}

// End of RteGenerator.cpp
