/*
 * Copyright (c) 2020-2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgrYamlParser.h"
#include "ProjMgrLogger.h"
#include "ProjMgrUtils.h"
#include "ProjMgrYamlSchemaChecker.h"

#include "RteFsUtils.h"
#include <regex>
#include <string>

using namespace std;

ProjMgrYamlParser::ProjMgrYamlParser(void) {
  // Reserved
}

ProjMgrYamlParser::~ProjMgrYamlParser(void) {
  // Reserved
}

bool ProjMgrYamlParser::ParseCdefault(const string& input,
  CdefaultItem& cdefault, bool checkSchema) {
  try {
    // Validate file schema
    if (checkSchema &&
      !ProjMgrYamlSchemaChecker().Validate(input)) {
      return false;
    }

    cdefault.path = RteFsUtils::MakePathCanonical(input);

    const YAML::Node& root = YAML::LoadFile(input);
    if (!ValidateCdefault(input, root)) {
      return false;
    }

    const YAML::Node& defaultNode = root[YAML_DEFAULT];
    ParseString(defaultNode, YAML_COMPILER, cdefault.compiler);
    if (!cdefault.compiler.empty()) {
      ProjMgrLogger::Get().Warn("'compiler' setting in cdefault.yml will be deprecated in CMSIS-Toolbox 3.0.0");
    }
    ParseSelectableCompilers(defaultNode, cdefault.selectableCompilers);
    ParseMisc(defaultNode, cdefault.misc);
  }
  catch (YAML::Exception& e) {
    ProjMgrLogger::Get().Error(e.msg, "", input, e.mark.line + 1, e.mark.column + 1);
    return false;
  }
  return true;
}

bool ProjMgrYamlParser::ParseCsolution(const string& input,
  CsolutionItem& csolution, bool checkSchema, bool frozenPacks) {

  string cbuildPackFile = RteUtils::ExtractPrefix(input, ".csolution.yml") + ".cbuild-pack.yml";
  if (fs::exists(cbuildPackFile)) {
    if (!ParseCbuildPack(cbuildPackFile, csolution.cbuildPack, checkSchema)) {
      return false;
    }
  } else if (frozenPacks) {
    ProjMgrLogger::Get().Error("file is missing and required due to use of --frozen-packs option", "", cbuildPackFile);
    return false;
  }

  try {
    // Validate file schema
    if (checkSchema &&
      !ProjMgrYamlSchemaChecker().Validate(input)) {
      return false;
    }

    csolution.path = RteFsUtils::MakePathCanonical(input);
    csolution.directory = RteFsUtils::ParentPath(csolution.path);
    csolution.name = fs::path(input).stem().stem().generic_string();

    const YAML::Node& root = YAML::LoadFile(input);
    if (!ValidateCsolution(input, root)) {
      return false;
    }

    const YAML::Node& solutionNode = root[YAML_SOLUTION];
    ParseString(solutionNode, YAML_DESCRIPTION, csolution.description);
    ParseString(solutionNode, YAML_CREATED_FOR, csolution.createdFor);
    if (!ParseContexts(solutionNode, csolution)) {
      return false;
    }

    if (!ParseTargetTypes(solutionNode, csolution.path, csolution.targetTypes)) {
      ProjMgrLogger::Get().Error("target-types not found", "", input);
      return false;
    }
    if (!ParseBuildTypes(solutionNode, csolution.path, csolution.buildTypes)) {
      return false;
    }
    ParseOutputDirs(solutionNode, csolution.path, csolution.directories);
    if (!ParseTargetType(solutionNode, csolution.path, csolution.target)) {
      return false;
    }
    ParseSelectableCompilers(solutionNode, csolution.selectableCompilers);
    ParsePacks(solutionNode, csolution.path, csolution.packs);
    csolution.enableCdefault = solutionNode[YAML_CDEFAULT].IsDefined();
    ParseGenerators(solutionNode, csolution.path, csolution.generators);
    ParseExecutes(solutionNode, csolution.path, csolution.executes);

  } catch (YAML::Exception& e) {
   ProjMgrLogger::Get().Error(e.msg, "", input, e.mark.line + 1, e.mark.column + 1);
    return false;
  }
  return true;
}

bool ProjMgrYamlParser::ParseCbuildPack(const string& input,
  CbuildPackItem& cbuildPack, bool checkSchema) {
  try {
    // Validate file schema
    if (checkSchema &&
      !ProjMgrYamlSchemaChecker().Validate(input)) {
      return false;
    }

    cbuildPack.path = RteFsUtils::MakePathCanonical(input);
    cbuildPack.directory = RteFsUtils::ParentPath(cbuildPack.path);
    cbuildPack.name = fs::path(input).stem().stem().stem().generic_string();

    const YAML::Node& root = YAML::LoadFile(input);
    if (!ValidateCbuildPack(input, root)) {
      return false;
    }

    const YAML::Node& cbuildPackNode = root[YAML_CBUILD_PACK];

    ParseResolvedPacks(cbuildPackNode, cbuildPack.packs);

  } catch (YAML::Exception& e) {
    ProjMgrLogger::Get().Error(e.msg, "", input, e.mark.line + 1, e.mark.column + 1);
    return false;
  }
  return true;
}

bool ProjMgrYamlParser::ParseCproject(const string& input,
  CsolutionItem& csolution, map<string, CprojectItem>& cprojects,
  bool single, bool checkSchema) {
  CprojectItem cproject;
  try {
    // Validate file schema
    if (checkSchema &&
      !ProjMgrYamlSchemaChecker().Validate(input)) {
      return false;
    }

    const YAML::Node& root = YAML::LoadFile(input);
    if (!ValidateCproject(input, root)) {
      return false;
    }

    cproject.path = RteFsUtils::MakePathCanonical(input);
    cproject.directory = RteFsUtils::ParentPath(cproject.path);
    cproject.name = fs::path(input).stem().stem().generic_string();

    const YAML::Node& projectNode = root[YAML_PROJECT];

    ParseOutput(projectNode, cproject.path, cproject.output);

    ParseTargetType(projectNode, cproject.path, cproject.target);

    ParsePacks(projectNode, cproject.path, cproject.packs);

    if (!ParseComponents(projectNode, cproject.path, cproject.components)) {
      return false;
    }
    if (!ParseGroups(projectNode, cproject.path, cproject.groups)) {
      return false;
    }
    if (!ParseLayers(projectNode, cproject.path, cproject.clayers)) {
      return false;
    }
    if (!ParseSetups(projectNode, cproject.path, cproject.setups)) {
      return false;
    }

    ParseConnections(projectNode, cproject.connections);

    ParseLinker(projectNode, cproject.path, cproject.linker);

    ParseGenerators(projectNode, cproject.path, cproject.generators);

    ParseRte(projectNode, cproject.rteBaseDir);

    ParseExecutes(projectNode, cproject.path, cproject.executes);

  } catch (YAML::Exception& e) {
    ProjMgrLogger::Get().Error(e.msg, "", input, e.mark.line + 1, e.mark.column + 1);
    return false;
  }

  cprojects[input] = cproject;

  if (single) {
    csolution.directory = cproject.directory;
    csolution.contexts.push_back({ input });
  }
  return true;
}

bool ProjMgrYamlParser::ParseClayer(const string& input,
  map<string, ClayerItem>& clayers, bool checkSchema) {
  if (clayers.find(input) != clayers.end()) {
    // already parsed
    return true;
  }
  ClayerItem clayer;
  try {
    // Validate file schema
    if (checkSchema) {
      if (!ProjMgrYamlSchemaChecker().Validate(input)) {
        return false;
      }
    }

    const YAML::Node& root = YAML::LoadFile(input);
    const bool cgen = fs::path(input).stem().extension().generic_string() == ".cgen";
    if (!cgen && !ValidateClayer(input, root)) {
      return false;
    }

    clayer.path = RteFsUtils::MakePathCanonical(input);
    clayer.directory = RteFsUtils::ParentPath(clayer.path);
    clayer.name = fs::path(input).stem().stem().generic_string();

    const YAML::Node& layerNode = root[cgen ? YAML_GENERATOR_IMPORT : YAML_LAYER];
    map<const string, string&> projectChildren = {
      {YAML_DESCRIPTION, clayer.description},
      {YAML_FORBOARD, clayer.forBoard},
      {YAML_FORDEVICE, clayer.forDevice},
      {YAML_TYPE, clayer.type},
    };
    for (const auto& item : projectChildren) {
      ParseString(layerNode, item.first, item.second);
    }

    ParseTargetType(layerNode, clayer.path, clayer.target);

    ParsePacks(layerNode, clayer.path, clayer.packs);

    if (!ParseComponents(layerNode, clayer.path, clayer.components)) {
      return false;
    }
    if (!ParseGroups(layerNode, clayer.path, clayer.groups)) {
      return false;
    }

    ParseConnections(layerNode, clayer.connections);

    ParseLinker(layerNode, clayer.path, clayer.linker);

    ParseGenerators(layerNode, clayer.path, clayer.generators);
  }
  catch (YAML::Exception& e) {
    ProjMgrLogger::Get().Error(e.msg, "", input, e.mark.line + 1, e.mark.column + 1);
    return false;
  }
  clayers[input] = clayer;
  return true;
}

bool ProjMgrYamlParser::ParseCbuildSet(const string& input, CbuildSetItem& cbuildSet, bool checkSchema) {
  // Validate file schema
  if (!ProjMgrYamlSchemaChecker().Validate(input)) {
    return false;
  }

  try {
    const YAML::Node& root = YAML::LoadFile(input);
    if (checkSchema && !ValidateCbuildSet(input, root)) {
      return false;
    }

    const YAML::Node& cbuildSetNode = root[YAML_CBUILD_SET];
    ParseString(cbuildSetNode, YAML_GENERATED_BY, cbuildSet.generatedBy);
    if (cbuildSetNode[YAML_CONTEXTS].IsDefined()) {
      const YAML::Node& contextsNode = cbuildSetNode[YAML_CONTEXTS];
      for (const auto& contextEntry : contextsNode) {
        string contextName;
        ParseString(contextEntry, YAML_CONTEXT, contextName);
        cbuildSet.contexts.push_back(contextName);
      }
    }
    ParseString(cbuildSetNode, YAML_COMPILER, cbuildSet.compiler);
  }
  catch (YAML::Exception& e) {
    ProjMgrLogger::Get().Error(e.msg, "", input, e.mark.line + 1, e.mark.column + 1);
    return false;
  }
  return true;
}

bool ProjMgrYamlParser::ParseDebugAdapters(const string& input, DebugAdaptersItem& adapters, bool checkSchema) {
  try {
    // Validate file schema
    if (checkSchema &&
      !ProjMgrYamlSchemaChecker().Validate(input)) {
      return false;
    }
    const YAML::Node& root = YAML::LoadFile(input);
    const string debugAdaptersPath = RteFsUtils::MakePathCanonical(input);

    const YAML::Node& adaptersNode = root[YAML_DEBUG_ADAPTERS];
    for (const auto& adaptersEntry : adaptersNode) {
      DebugAdapterItem adapter;
      ParseString(adaptersEntry, YAML_NAME, adapter.name);
      ParseVector(adaptersEntry, YAML_ALIAS_NAME, adapter.alias);
      ParseString(adaptersEntry, YAML_TEMPLATE, adapter.templateFile);
      ParseDebugDefaults(adaptersEntry, debugAdaptersPath, adapter.defaults);
      adapters.push_back(adapter);
    }
  }
  catch (YAML::Exception& e) {
    ProjMgrLogger::Get().Error(e.msg, "", input, e.mark.line + 1, e.mark.column + 1);
    return false;
  }
  return true;
}

// EnsurePortability checks the presence of backslash, case inconsistency and absolute path
void ProjMgrYamlParser::EnsurePortability(const string& file, const YAML::Mark& mark, const string& key, string& value) {
  if (value.find('\\') != string::npos) {
    ProjMgrLogger::Get().Warn("'" + value + "' contains non-portable backslash, use forward slash instead", "", file, mark.line + 1, mark.column + 1);
  }
  if (!value.empty()) {
    if (fs::path(value).is_absolute()) {
      ProjMgrLogger::Get().Warn("absolute path '" + value + "' is not portable, use relative path instead", "", file, mark.line + 1, mark.column + 1);
    } else {
      const string parentDir = RteFsUtils::ParentPath(file);
      const string original = RteFsUtils::LexicallyNormal(fs::path(parentDir).append(value).generic_string());
      if (RteFsUtils::Exists(original)) {
        error_code ec;
        string canonical = fs::canonical(original, ec).generic_string();
        if (!canonical.empty() && (original != canonical)) {
          ProjMgrLogger::Get().Warn("'" + value + "' has case inconsistency, use '" +
            RteFsUtils::RelativePath(canonical, parentDir) + "' instead", "", file, mark.line + 1, mark.column + 1);
        }
      }
    }
  }
}

void ProjMgrYamlParser::ParsePortablePath(const YAML::Node& parent, const string& file, const string& key, string& value) {
  ParseString(parent, key, value);
  YAML::Mark mark = parent[key].IsDefined() ? parent[key].Mark() : YAML::Mark();
  EnsurePortability(file, mark, key, value);
}

void ProjMgrYamlParser::ParsePortablePaths(const YAML::Node& parent, const string& file, const string& key, vector<string>& value) {
  ParseVector(parent, key, value);
  auto node = parent[key].begin();
  for (auto& item : value) {
    YAML::Mark mark = (*node++).Mark();
    EnsurePortability(file, mark, key, item);
  }
}

void ProjMgrYamlParser::ParseBoolean(const YAML::Node& parent, const string& key, bool& value, bool def) {
  if (parent[key].IsDefined() && (parent[key].Type() != YAML::NodeType::Null)) {
    value = parent[key].as<bool>();
  } else {
    value = def;
  }
}

void ProjMgrYamlParser::ParseString(const YAML::Node& parent, const string& key, string& value) {
  if (parent[key].IsDefined()) {
    value = parent[key].as<string>();
    if (parent[key].Type() == YAML::NodeType::Null) {
      value = "";
    }
  }
}

void ProjMgrYamlParser::ParseNumber(const YAML::Node& parent, const string& file, const string& key, string& value) {
  if (parent[key].IsDefined()) {
    value = parent[key].as<string>();
    if (!regex_match(value, regex("^(0x[0-9a-fA-F]+)$|^([0-9]*)$"))) {
      YAML::Mark mark = parent[key].Mark();
      ProjMgrLogger::Get().Warn("number must be integer or hexadecimal prefixed by 0x", "", file, mark.line + 1, mark.column + 1);
    }
  }
}

void ProjMgrYamlParser::ParseInt(const YAML::Node& parent, const string& key, int& value) {
  if (parent[key].IsDefined()) {
    value = parent[key].as<int>();
    if (parent[key].Type() == YAML::NodeType::Null) {
      value = 0;
    }
  }
}

void ProjMgrYamlParser::ParseVector(const YAML::Node& parent, const string& key, vector<string>& value) {
  if (parent[key].IsDefined() && parent[key].IsSequence()) {
    value = parent[key].as<vector<string>>();
  }
}

bool ProjMgrYamlParser::ParseDefine(const YAML::Node& defineNode, vector<string>& define) {
  auto ValidateDefineStr = [&](const std::string& defineValue) -> bool {
    if (defineValue.empty()) {
      return true;
    }

    const std::string escapedQuote = "\\\"";
    int numQuotes = RteUtils::CountDelimiters(defineValue, "\"");
    if (numQuotes == 0) {
      return true;
    }

    bool isValid = true;
    string errStr;
    if (numQuotes == 2) {
      if (defineValue.front() == '"') {
        isValid = (defineValue.back() == '"');
      }
      else if (defineValue.substr(0, 2) == escapedQuote) {
        isValid = (defineValue.substr(defineValue.size() - 2, 2) == escapedQuote);
      }
      else {
        isValid = false;
      }
    }
    else {
      isValid = false;
    }

    if (!isValid) {
      ProjMgrLogger::Get().Error("invalid define: " + defineValue + ", improper quotes");
    }

    return isValid;
  };

  bool success = true;
  if (defineNode.IsDefined() && defineNode.IsSequence()) {
    for (const auto& item : defineNode) {
      // accept map elements <string, string> or a string
      if (item.IsDefined()) {
        if (item.IsMap()) {
          const auto& elements = item.as<map<string, string>>();
          for (auto element : elements) {
            if (YAML::IsNullString(element.second)) {
              element.second = "";
            }
            success = ValidateDefineStr(element.second);
            if (success) {
              define.push_back(element.first + "=" + element.second);
            }
          }
        }
        else {
          success = ValidateDefineStr(item.as<string>());
          if (success) {
            define.push_back(item.as<string>());
          }
        }
      }
    }
  }
  return success;
}

void ProjMgrYamlParser::ParseVectorOfStringPairs(const YAML::Node& parent, const string& key, vector<pair<string, string>>& value) {
  if (parent[key].IsDefined() && parent[key].IsSequence()) {
    for (const auto& item : parent[key]) {
      // accept map elements <string, string> or a string
      if (item.IsDefined()) {
        if (item.IsMap()) {
          const auto& elements = item.as<map<string, string>>();
          for (auto element : elements) {
            // skip parsing variable named "copied-from"
            // This is a special case used by the CMSIS csolution extension
            // and may involve redefinitions
            if (element.first != "copied-from") {
              if (YAML::IsNullString(element.second)) {
                element.second = "";
              }
              value.push_back(element);
            }
          }
        } else {
          value.push_back(make_pair(item.as<string>(), ""));
        }
      }
    }
  }
}

void ProjMgrYamlParser::ParseVectorOrString(const YAML::Node& parent, const string& key, vector<string>& value) {
  ParseVector(parent, key, value);
  if (value.empty()) {
    string strValue;
    ParseString(parent, key, strValue);
    if (!strValue.empty()) {
      value.push_back({ strValue });
    }
  }
}

void ProjMgrYamlParser::ParseProcessor(const YAML::Node& parent, ProcessorItem& processor) {
  if (parent[YAML_PROCESSOR].IsDefined()) {
    const YAML::Node& processorNode = parent[YAML_PROCESSOR];
    map<const string, string&> processorChildren = {
      {YAML_TRUSTZONE, processor.trustzone},
      {YAML_FPU, processor.fpu},
      {YAML_DSP, processor.dsp},
      {YAML_MVE, processor.mve},
      {YAML_ENDIAN, processor.endian},
      {YAML_BRANCH_PROTECTION, processor.branchProtection},
    };
    for (const auto& item : processorChildren) {
      ParseString(processorNode, item.first, item.second);
    }
  }
}

bool ProjMgrYamlParser::ParseComponents(const YAML::Node& parent, const string& file, vector<ComponentItem>& components) {
  if (parent[YAML_COMPONENTS].IsDefined()) {
    const YAML::Node& componentsNode = parent[YAML_COMPONENTS];
    for (const auto& componentEntry : componentsNode) {
      ComponentItem componentItem;
      if (!ParseTypeFilter(componentEntry, componentItem.type)) {
        return false;
      }
      ParseString(componentEntry, YAML_COMPONENT, componentItem.component);
      ParseString(componentEntry, YAML_CONDITION, componentItem.condition);
      ParseString(componentEntry, YAML_FROM_PACK, componentItem.fromPack);
      if (!ParseBuildType(componentEntry, file, componentItem.build)) {
        return false;
      }
      ParseInt(componentEntry, YAML_INSTANCES, componentItem.instances);
      components.push_back(componentItem);
    }
  }
  return true;
}

bool ProjMgrYamlParser::ParseSelectableCompilers(const YAML::Node& parent, vector<string>& selectableCompilers) {
  if (parent[YAML_SELECT_COMPILER].IsDefined()) {
    const YAML::Node& selectCompilerNode = parent[YAML_SELECT_COMPILER];
    for (const auto& compilerEntry : selectCompilerNode) {
      string compiler;
      ParseString(compilerEntry, YAML_COMPILER, compiler);
      CollectionUtils::PushBackUniquely(selectableCompilers, compiler);
    }
  }
  return true;
}

void ProjMgrYamlParser::ParseOutput(const YAML::Node& parent, const string& file, OutputItem& output) {
  if (parent[YAML_OUTPUT].IsDefined()) {
    const YAML::Node& outputNode = parent[YAML_OUTPUT];
    ParsePortablePath(outputNode, file, YAML_BASE_NAME, output.baseName);
    ParseVectorOrString(outputNode, YAML_TYPE, output.type);
  }
}

void ProjMgrYamlParser::ParseGenerators(const YAML::Node& parent, const string& file, GeneratorsItem& generators) {
  if (parent[YAML_GENERATORS].IsDefined()) {
    const YAML::Node& generatorsNode = parent[YAML_GENERATORS];
    ParsePortablePath(generatorsNode, file, YAML_BASE_DIR, generators.baseDir);
    if (generatorsNode[YAML_OPTIONS].IsDefined()) {
      const YAML::Node& optionsNode = generatorsNode[YAML_OPTIONS];
      for (const auto& optionsEntry : optionsNode) {
        GeneratorOptionsItem options;
        ParseString(optionsEntry, YAML_GENERATOR, options.id);
        ParsePortablePath(optionsEntry, file, YAML_PATH, options.path);
        ParseString(optionsEntry, YAML_NAME, options.name);
        ParseString(optionsEntry, YAML_MAP, options.map);
        generators.options[options.id] = options;
      }
    }
  }
}

void ProjMgrYamlParser::ParseExecutes(const YAML::Node& parent, const string& file, std::vector<ExecutesItem>& executes) {
  if (parent[YAML_EXECUTES].IsDefined()) {
    const YAML::Node& executesNode = parent[YAML_EXECUTES];
    for (const auto& executesEntry : executesNode) {
      ExecutesItem executesItem;
      ParseString(executesEntry, YAML_EXECUTE, executesItem.execute);
      ParseString(executesEntry, YAML_RUN, executesItem.run);
      executesItem.always = executesEntry[YAML_ALWAYS].IsDefined();
      ParsePortablePaths(executesEntry, file, YAML_INPUT, executesItem.input);
      ParsePortablePaths(executesEntry, file, YAML_OUTPUT, executesItem.output);
      ParseTypeFilter(executesEntry, executesItem.typeFilter);
      executes.push_back(executesItem);
    }
  }
}

void ProjMgrYamlParser::ParseTelnet(const YAML::Node& parent, const std::string& file, std::vector<TelnetItem>& telnet) {
  if (parent[YAML_TELNET].IsDefined()) {
    const YAML::Node& telnetNode = parent[YAML_TELNET];
    for (const auto& telnetEntry : telnetNode) {
      TelnetItem telnetItem;
      ParseString(telnetEntry, YAML_MODE, telnetItem.mode);
      ParseString(telnetEntry, YAML_PORT, telnetItem.port);
      ParseString(telnetEntry, YAML_FILE, telnetItem.file);
      ParseString(telnetEntry, YAML_PNAME, telnetItem.pname);
      telnet.push_back(telnetItem);
    }
  }
}

void ProjMgrYamlParser::ParseDebugger(const YAML::Node& parent, const string& file, DebuggerItem& debugger) {
  if (parent[YAML_DEBUGGER].IsDefined()) {
    const YAML::Node& debuggerNode = parent[YAML_DEBUGGER];
    ParseString(debuggerNode, YAML_NAME, debugger.name);
    ParseString(debuggerNode, YAML_PROTOCOL, debugger.protocol);
    ParseNumber(debuggerNode, file, YAML_CLOCK, debugger.clock);
    ParsePortablePath(debuggerNode, file, YAML_DBGCONF, debugger.dbgconf);
    ParseString(debuggerNode, YAML_START_PNAME, debugger.startPname);
    ParseTelnet(debuggerNode, file, debugger.telnet);
    ParseCustom(debuggerNode, { YAML_NAME, YAML_PROTOCOL, YAML_CLOCK, YAML_DBGCONF, YAML_START_PNAME, YAML_TELNET }, debugger.custom);
  }
}

void ProjMgrYamlParser::ParseRte(const YAML::Node& parent, string& rteBaseDir) {
  if (parent[YAML_RTE].IsDefined()) {
    const YAML::Node& rteNode = parent[YAML_RTE];
    ParseString(rteNode, YAML_BASE_DIR, rteBaseDir);
  }
}

void ProjMgrYamlParser::ParseDebugDefaults(const YAML::Node& parent, const string& file, DebugAdapterDefaultsItem& defaults) {
  if (parent[YAML_DEFAULTS].IsDefined()) {
    const YAML::Node& defaultsNode = parent[YAML_DEFAULTS];
    if (defaultsNode[YAML_GDBSERVER].IsDefined()) {
      const YAML::Node& gdbserverNode = defaultsNode[YAML_GDBSERVER];
      ParseString(gdbserverNode, YAML_PORT, defaults.gdbserver.port);
      defaults.gdbserver.active = gdbserverNode[YAML_ACTIVE].IsDefined();
    }
    if (defaultsNode[YAML_TELNET].IsDefined()) {
      const YAML::Node& telnetNode = defaultsNode[YAML_TELNET];
      ParseString(telnetNode, YAML_PORT, defaults.telnet.port);
      ParseString(telnetNode, YAML_MODE, defaults.telnet.mode);
      defaults.telnet.active = telnetNode[YAML_ACTIVE].IsDefined();
    }
    ParseString(defaultsNode, YAML_PROTOCOL, defaults.protocol);
    ParseNumber(defaultsNode, file, YAML_CLOCK, defaults.clock);
    ParseCustom(defaultsNode, { YAML_GDBSERVER, YAML_TELNET, YAML_PROTOCOL, YAML_CLOCK }, defaults.custom);
  }
}

bool ProjMgrYamlParser::ParseTypeFilter(const YAML::Node& parent, TypeFilter& type) {
  vector<string> include, exclude;
  ParseVectorOrString(parent, YAML_FORCONTEXT, include);
  ParseVectorOrString(parent, YAML_NOTFORCONTEXT, exclude);
  if (!ParseTypePair(include, type.include) ||
      !ParseTypePair(exclude, type.exclude)) {
    return false;
  }
  return true;
}

bool ProjMgrYamlParser::ParseTypePair(vector<string>& vec, vector<TypePair>& typeVec) {
  bool valid = true;
  for (const auto& item : vec) {
    TypePair typePair;
    if (!GetTypes(item, typePair.build, typePair.target, typePair.pattern)) {
      valid = false;
    }
    typeVec.push_back(typePair);
  }
  return valid;
}

bool ProjMgrYamlParser::GetTypes(const string& type, string& buildType, string& targetType, string& pattern) {
  size_t buildDelimiter = type.find(".");
  size_t targetDelimiter = type.find("+");
  size_t regexDelimiter = type.find_first_of("^\\");
  if (((buildDelimiter > 0) && (targetDelimiter > 0) && (regexDelimiter > 0))) {
    ProjMgrLogger::Get().Error("type '" + type + "': it must start with '.' or '+' for normal strings and with '^' or '\\' for regex patterns");
    return false;
  }
  if (regexDelimiter == 0) {
    try {
      regex r = regex(type);
    } catch (const std::regex_error& e) {
      ProjMgrLogger::Get().Error("invalid pattern '" + type + "': " + e.what());
      return false;
    };
    pattern = type;
    return true;
  }
  if (targetDelimiter > buildDelimiter) {
    if (targetDelimiter < string::npos) {
      targetType = type.substr(targetDelimiter + 1, string::npos);
      targetDelimiter--;
    }
    buildType = type.substr(buildDelimiter + 1, targetDelimiter);
  }
  else if (buildDelimiter > targetDelimiter) {
    if (buildDelimiter < string::npos) {
      buildType = type.substr(buildDelimiter + 1, string::npos);
      buildDelimiter--;
    }
    targetType = type.substr(targetDelimiter + 1, buildDelimiter);
  }
  return true;
}

void ProjMgrYamlParser::ParseMisc(const YAML::Node& parent, vector<MiscItem>& misc) {
  if (parent[YAML_MISC].IsDefined()) {
    const YAML::Node& miscNode = parent[YAML_MISC];
    for (const auto& miscEntry : miscNode) {
      MiscItem miscItem;
      ParseString(miscEntry, YAML_FORCOMPILER, miscItem.forCompiler);
      ParseVector(miscEntry, YAML_MISC_C, miscItem.c);
      ParseVector(miscEntry, YAML_MISC_CPP, miscItem.cpp);
      ParseVector(miscEntry, YAML_MISC_C_CPP, miscItem.c_cpp);
      ParseVector(miscEntry, YAML_MISC_ASM, miscItem.as);
      ParseVector(miscEntry, YAML_MISC_LINK, miscItem.link);
      ParseVector(miscEntry, YAML_MISC_LINK_C, miscItem.link_c);
      ParseVector(miscEntry, YAML_MISC_LINK_CPP, miscItem.link_cpp);
      ParseVector(miscEntry, YAML_MISC_LIB, miscItem.lib);
      ParseVector(miscEntry, YAML_MISC_LIBRARY, miscItem.library);
      misc.push_back(miscItem);
    }
  }
}

void ProjMgrYamlParser::ParseResolvedPacks(const YAML::Node& parent, vector<ResolvedPackItem>& packs) {
  if (parent[YAML_RESOLVED_PACKS].IsDefined()) {
    const YAML::Node& packsNode = parent[YAML_RESOLVED_PACKS];
    for (const auto& packEntry : packsNode) {
      ResolvedPackItem packItem;
      ParseString(packEntry, YAML_RESOLVED_PACK, packItem.pack);
      ParseVector(packEntry, YAML_SELECTED_BY_PACK, packItem.selectedByPack);
      // Accept "selected-by" for backward compatibility
      if (packItem.selectedByPack.empty()) {
        ParseVector(packEntry, YAML_SELECTED_BY, packItem.selectedByPack);
      }
      packs.push_back(packItem);
    }
  }
}

void ProjMgrYamlParser::ParsePacks(const YAML::Node& parent, const string& file, vector<PackItem>& packs) {
  if (parent[YAML_PACKS].IsDefined()) {
    const YAML::Node& packNode = parent[YAML_PACKS];
    for (const auto& packEntry : packNode) {
      PackItem packItem;
      packItem.origin = file;
      ParseString(packEntry, YAML_PACK, packItem.pack);
      ParsePortablePath(packEntry, file, YAML_PATH, packItem.path);
      ParseTypeFilter(packEntry, packItem.type);
      packs.push_back(packItem);
    }
  }
}

bool ProjMgrYamlParser::ParseFiles(const YAML::Node& parent, const string& file, vector<FileNode>& files) {
  if (parent[YAML_FILES].IsDefined()) {
    const YAML::Node& filesNode = parent[YAML_FILES];
    for (const auto& fileEntry : filesNode) {
      FileNode fileItem;
      if (!ParseTypeFilter(fileEntry, fileItem.type)) {
        return false;
      }
      ParsePortablePath(fileEntry, file, YAML_FILE, fileItem.file);
      ParseVectorOrString(fileEntry, YAML_FORCOMPILER, fileItem.forCompiler);
      ParseString(fileEntry, YAML_CATEGORY, fileItem.category);
      ParseString(fileEntry, YAML_LINK, fileItem.link);
      if (!ParseBuildType(fileEntry, file, fileItem.build)) {
        return false;
      }
      fileItem.mark = YamlMark{ file, fileEntry.Mark().line + 1, fileEntry.Mark().column + 1 };
      files.push_back(fileItem);
    }
  }
  return true;
}

bool ProjMgrYamlParser::ParseGroups(const YAML::Node& parent, const string& file, vector<GroupNode>& groups) {
  if (parent[YAML_GROUPS].IsDefined()) {
    const YAML::Node& groupsNode = parent[YAML_GROUPS];
    for (const auto& groupEntry : groupsNode) {
      GroupNode groupItem;
      if (!ParseTypeFilter(groupEntry, groupItem.type)) {
        return false;
      }
      if (!ParseFiles(groupEntry, file, groupItem.files)) {
        return false;
      }
      ParseString(groupEntry, YAML_GROUP, groupItem.group);
      ParseVectorOrString(groupEntry, YAML_FORCOMPILER, groupItem.forCompiler);
      if (!ParseBuildType(groupEntry, file, groupItem.build)) {
        return false;
      }
      ParseGroups(groupEntry, file, groupItem.groups);
      groups.push_back(groupItem);
    }
  }
  return true;
}

bool ProjMgrYamlParser::ParseLayers(const YAML::Node& parent, const string& file, vector<LayerItem>& layers) {
  if (parent[YAML_LAYERS].IsDefined()) {
    const YAML::Node& layersNode = parent[YAML_LAYERS];
    for (const auto& layerEntry : layersNode) {
      LayerItem layerItem;
      if (!ParseTypeFilter(layerEntry, layerItem.typeFilter)) {
        return false;
      }
      ParsePortablePath(layerEntry, file, YAML_LAYER, layerItem.layer);
      ParseString(layerEntry, YAML_TYPE, layerItem.type);
      ParseBoolean(layerEntry, YAML_OPTIONAL, layerItem.optional, true);
      layers.push_back(layerItem);
    }
  }
  return true;
}

bool ProjMgrYamlParser::ParseSetups(const YAML::Node& parent, const string& file, vector<SetupItem>& setups) {
  if (parent[YAML_SETUPS].IsDefined()) {
    const YAML::Node& setupsNode = parent[YAML_SETUPS];
    for (const auto& setupEntry : setupsNode) {
      SetupItem setupItem;
      if (!ParseTypeFilter(setupEntry, setupItem.type)) {
        return false;
      }
      ParseString(setupEntry, YAML_SETUP, setupItem.description);
      ParseVectorOrString(setupEntry, YAML_FORCOMPILER, setupItem.forCompiler);
      if (!ParseBuildType(setupEntry, file, setupItem.build)) {
        return false;
      }
      ParseOutput(setupEntry, file, setupItem.output);
      ParseLinker(setupEntry, file, setupItem.linker);
      ParseProcessor(setupEntry, setupItem.build.processor);
      setups.push_back(setupItem);
    }
  }
  return true;
}

bool ProjMgrYamlParser::ParseContexts(const YAML::Node& parent, CsolutionItem& csolution) {
  if (parent[YAML_PROJECTS].IsDefined()) {
    const YAML::Node& projectsNode = parent[YAML_PROJECTS];
    for (const auto& projectsEntry : projectsNode) {
      ContextDesc descriptor;
      if (!ParseTypeFilter(projectsEntry, descriptor.type)) {
        return false;
      }
      ParsePortablePath(projectsEntry, csolution.path, YAML_PROJECT, descriptor.cproject);
      if (!descriptor.cproject.empty()) {
        descriptor.cproject = RteFsUtils::RelativePath(fs::path(csolution.directory).append(descriptor.cproject).generic_string(), csolution.directory);
        CollectionUtils::PushBackUniquely(csolution.cprojects, descriptor.cproject);
      }
      if (projectsEntry[YAML_WEST].IsDefined()) {
        const auto& westEntry = projectsEntry[YAML_WEST];
        WestDesc west;
        ParsePortablePath(westEntry, csolution.path, YAML_APP_PATH, west.app);
        ParseString(westEntry, YAML_PROJECT_ID, west.projectId);
        ParseString(westEntry, YAML_BOARD, west.board);
        ParseString(westEntry, YAML_DEVICE, west.device);
        ParseDefine(westEntry[YAML_WEST_DEFS], west.westDefs);
        ParseVectorOrString(westEntry, YAML_WEST_OPT, west.westOpt);
        if (west.projectId.empty()) {
          west.projectId = fs::path(west.app).filename().generic_string();
        }
        descriptor.west = west;
        CollectionUtils::PushBackUniquely(csolution.westApps, west.projectId);
      }
      csolution.contexts.push_back(descriptor);
    }
  }
  return true;
}

bool ProjMgrYamlParser::ParseBuildTypes(const YAML::Node& parent, const string& file, BuildTypes& buildTypes) {
  std::vector<std::string> invalidBuildTypes;
  if (parent[YAML_BUILDTYPES].IsDefined()) {
    const YAML::Node& buildTypesNode = parent[YAML_BUILDTYPES];
    for (const auto& typeEntry : buildTypesNode) {
      string typeItem;
      BuildType build;
      ParseString(typeEntry, YAML_TYPE, typeItem);
      if (RteUtils::CountDelimiters(typeItem, ".") > 0 || RteUtils::CountDelimiters(typeItem, "+") > 0) {
        invalidBuildTypes.push_back(typeItem);
        continue;
      }
      if (!ParseBuildType(typeEntry, file, build)) {
        return false;
      }
      buildTypes.push_back({ typeItem, build });
    }
  }
  if (invalidBuildTypes.size() > 0) {
    string errMsg = "invalid build type(s):\n";
    for (const auto& buildType : invalidBuildTypes) {
      errMsg += "  " + buildType + "\n";
    }
    ProjMgrLogger::Get().Error(errMsg);
    return false;
  }
  return true;
}

void ProjMgrYamlParser::ParseOutputDirs(const YAML::Node& parent, const std::string& file, struct DirectoriesItem& directories) {
  if (parent[YAML_OUTPUTDIRS].IsDefined()) {
    const YAML::Node& outputDirsNode = parent[YAML_OUTPUTDIRS];
    map<const string, string&> outputDirsChildren = {
      {YAML_OUTPUT_CPRJDIR, directories.cprj},
      {YAML_OUTPUT_INTDIR, directories.intdir},
      {YAML_OUTPUT_OUTDIR, directories.outdir},
      {YAML_OUTPUT_TMPDIR, directories.tmpdir},
    };
    for (const auto& item : outputDirsChildren) {
      ParsePortablePath(outputDirsNode, file, item.first, item.second);
    }
  }
}

void ProjMgrYamlParser::ParseConnections(const YAML::Node& parent, vector<ConnectItem>& connects) {
  if (parent[YAML_CONNECTIONS].IsDefined()) {
    const YAML::Node& connectsNode = parent[YAML_CONNECTIONS];
    for (const auto& connectEntry : connectsNode) {
      ConnectItem connectItem;
      ParseString(connectEntry, YAML_CONNECT, connectItem.connect);
      ParseString(connectEntry, YAML_SET, connectItem.set);
      ParseString(connectEntry, YAML_INFO, connectItem.info);
      map<const string, vector<pair<string, string>>&> connectConsumesProvides = {
        {YAML_PROVIDES, connectItem.provides},
        {YAML_CONSUMES, connectItem.consumes},
      };
      for (const auto& item : connectConsumesProvides) {
        ParseVectorOfStringPairs(connectEntry, item.first, item.second);
      }
      connects.push_back(connectItem);
    }
  }
}

bool ProjMgrYamlParser::ParseLinker(const YAML::Node& parent, const string& file, vector<LinkerItem>& linker) {
  if (parent[YAML_LINKER].IsDefined()) {
    const YAML::Node& linkerNode = parent[YAML_LINKER];
    for (const auto& linkerEntry : linkerNode) {
      LinkerItem linkerItem;
      if (!ParseTypeFilter(linkerEntry, linkerItem.typeFilter)) {
        return false;
      }
      linkerItem.autoGen = linkerEntry[YAML_AUTO].IsDefined();
      if (!ParseDefine(linkerEntry[YAML_DEFINE], linkerItem.defines)) {
        return false;
      }
      ParseVectorOrString(linkerEntry, YAML_FORCOMPILER, linkerItem.forCompiler);
      ParsePortablePath(linkerEntry, file, YAML_REGIONS, linkerItem.regions);
      ParsePortablePath(linkerEntry, file, YAML_SCRIPT, linkerItem.script);
      linker.push_back(linkerItem);
    }
  }
  return true;
}

bool ProjMgrYamlParser::ParseTargetTypes(const YAML::Node& parent, const string& file, TargetTypes& targetTypes) {
  std::vector<std::string> invalidTargetTypes;
  const YAML::Node& targetTypesNode = parent[YAML_TARGETTYPES];
  for (const auto& typeEntry : targetTypesNode) {
    string typeItem;
    TargetType target;
    ParseString(typeEntry, YAML_TYPE, typeItem);
    if (RteUtils::CountDelimiters(typeItem, ".") > 0 || RteUtils::CountDelimiters(typeItem, "+") > 0) {
      invalidTargetTypes.push_back(typeItem);
      continue;
    }
    ParseTargetType(typeEntry, file, target);
    targetTypes.push_back({ typeItem, target });
  }
  if (invalidTargetTypes.size() > 0) {
    string errMsg = "invalid target type(s):\n";
    for (const auto& targetType : invalidTargetTypes) {
      errMsg += "  " + targetType + "\n";
    }
    ProjMgrLogger::Get().Error(errMsg);
    return false;
  }
  return (targetTypes.size() == 0 ? false : true);
}

bool ProjMgrYamlParser::ParseBuildType(const YAML::Node& parent, const string& file, BuildType& buildType) {
  map<const string, string&> buildChildren = {
    {YAML_COMPILER, buildType.compiler},
    {YAML_OPTIMIZE, buildType.optimize},
    {YAML_DEBUG, buildType.debug},
    {YAML_WARNINGS, buildType.warnings},
    {YAML_LANGUAGE_C, buildType.languageC},
    {YAML_LANGUAGE_CPP, buildType.languageCpp},
  };
  for (const auto& item : buildChildren) {
    ParseString(parent, item.first, item.second);
  }
  buildType.lto = parent[YAML_LINK_TIME_OPTIMIZE].IsDefined();
  ParseProcessor(parent, buildType.processor);
  ParseMisc(parent, buildType.misc);
  if (!ParseDefine(parent[YAML_DEFINE], buildType.defines)) {
    return false;
  }
  if (!ParseDefine(parent[YAML_DEFINE_ASM], buildType.definesAsm)) {
    return false;
  }
  if (!ParseDefine(parent[YAML_WEST_DEFS], buildType.westDefs)) {
    return false;
  }
  ParseVector(parent, YAML_UNDEFINE, buildType.undefines);
  ParsePortablePaths(parent, file, YAML_ADDPATH, buildType.addpaths);
  ParsePortablePaths(parent, file, YAML_ADDPATH_ASM, buildType.addpathsAsm);
  ParseVector(parent, YAML_DELPATH, buildType.delpaths);
  ParseVectorOfStringPairs(parent, YAML_VARIABLES, buildType.variables);

  std::vector<std::string> contextMap, invalidContexts;
  ParseVectorOrString(parent, YAML_CONTEXT_MAP, contextMap);
  for (const auto& contextEntry : contextMap) {
    ContextName context;
    if (!ProjMgrUtils::ParseContextEntry(contextEntry, context)) {
      invalidContexts.push_back(contextEntry);
    }
    buildType.contextMap.push_back(context);
  }

  if (invalidContexts.size() > 0) {
    string errMsg = "context-map specifies invalid context(s):\n";
    for (const auto& context : invalidContexts) {
      errMsg += "  " + context + "\n";
    }
    ProjMgrLogger::Get().Error(errMsg);
    return false;
  }

  return true;
}

bool ProjMgrYamlParser::ParseTargetType(const YAML::Node& parent, const string& file, TargetType& targetType) {
  map<const string, string&> targetChildren = {
    {YAML_BOARD, targetType.board},
    {YAML_DEVICE, targetType.device},
  };
  for (const auto& item : targetChildren) {
    ParseString(parent, item.first, item.second);
  }

  // Throw warning in case board or device name are set in *.cproject.yml or *.clayer.yml
  if (regex_match(file, regex(".*\\.(cproject|clayer)\\.(yml|yaml)"))) {
    for (const auto& [key, value] : targetChildren) {
      if (!value.empty()) {
        if ((key == YAML_DEVICE) && regex_match(value, regex("^:[ -9;-~]+$"))) {
          // processor name in the format :<pname> is accepted
          continue;
        }
        ProjMgrLogger::Get().Warn(string(key == YAML_DEVICE ? "'device: Dname'" : "'board:' node") +
          " is deprecated at this level and accepted in *.csolution.yml only", "", file);
      }
    }
  }

  if (parent[YAML_MEMORY].IsDefined()) {
    const YAML::Node& memoryNode = parent[YAML_MEMORY];
    for (const auto& memoryEntry : memoryNode) {
      MemoryItem memoryItem;
      ParseString(memoryEntry, YAML_NAME, memoryItem.name);
      ParseString(memoryEntry, YAML_ACCESS, memoryItem.access);
      ParseString(memoryEntry, YAML_ALGORITHM, memoryItem.algorithm);
      ParseNumber(memoryEntry, file, YAML_START, memoryItem.start);
      ParseNumber(memoryEntry, file, YAML_SIZE, memoryItem.size);
      targetType.memory.push_back(memoryItem);
    }
  }

  ParseTargetSet(parent, file, targetType.targetSet);

  return ParseBuildType(parent, file, targetType.build);
}

void ProjMgrYamlParser::ParseTargetSet(const YAML::Node& parent, const string& file, std::vector<TargetSetItem>& targetSets) {
  if (parent[YAML_TARGET_SET].IsDefined()) {
    const YAML::Node& targetSetNode = parent[YAML_TARGET_SET];
    for (const auto& targetSetEntry : targetSetNode) {
      TargetSetItem targetSet;
      ParseString(targetSetEntry, YAML_SET, targetSet.set);
      ParseString(targetSetEntry, YAML_INFO, targetSet.info);
      ParseDebugger(targetSetEntry, file, targetSet.debugger);
      ParseImages(targetSetEntry, file, targetSet.images);
      targetSets.push_back(targetSet);
    }
  }
}

void ProjMgrYamlParser::ParseImages(const YAML::Node& parent, const string& file, std::vector<ImageItem>& images) {
  if (parent[YAML_IMAGES].IsDefined()) {
    const YAML::Node& imagesNode = parent[YAML_IMAGES];
    for (const auto& imagesEntry : imagesNode) {
      ImageItem imageItem;
      ParseString(imagesEntry, YAML_PROJECT_CONTEXT, imageItem.context);
      ParsePortablePath(imagesEntry, file, YAML_IMAGE, imageItem.image);
      ParseString(imagesEntry, YAML_INFO, imageItem.info);
      ParseString(imagesEntry, YAML_TYPE, imageItem.type);
      ParseString(imagesEntry, YAML_LOAD, imageItem.load);
      ParseString(imagesEntry, YAML_DEVICE, imageItem.pname);
      imageItem.pname = RteUtils::ExtractSuffix(imageItem.pname);
      ParseNumber(imagesEntry, file, YAML_LOAD_OFFSET, imageItem.offset);
      images.push_back(imageItem);
    }
  }
}

CustomItem ProjMgrYamlParser::GetCustomValue(const YAML::Node& node) {
  CustomItem value;
  if (node.IsScalar()) {
    value.scalar = node.as<string>();
  }
  else if (node.IsSequence()) {
    for (const auto& item : node) {
      value.vec.push_back(GetCustomValue(item));
    }
  }
  else if (node.IsMap()) {
    for (const auto& item : node) {
      value.map.push_back({ item.first.as<string>(), GetCustomValue(item.second) });
    }
  }
  return value;
}

void ProjMgrYamlParser::ParseCustom(const YAML::Node& parent, const vector<string>& skip, CustomItem& custom) {
  for (const auto& node : parent) {
    const auto& key = node.first.as<string>();
    if (find(skip.begin(), skip.end(), key) != skip.end()) {
      continue;
    }
    custom.map.push_back({ key, GetCustomValue(node.second) });
  }
}

// Validation Maps
const set<string> defaultKeys = {
  YAML_COMPILER,
  YAML_SELECT_COMPILER,
  YAML_MISC,
};

const set<string> solutionKeys = {
  YAML_DESCRIPTION,
  YAML_PROJECTS,
  YAML_TARGETTYPES,
  YAML_BUILDTYPES,
  YAML_OUTPUTDIRS,
  YAML_PACKS,
  YAML_PROCESSOR,
  YAML_COMPILER,
  YAML_SELECT_COMPILER,
  YAML_OPTIMIZE,
  YAML_DEBUG,
  YAML_WARNINGS,
  YAML_LANGUAGE_C,
  YAML_LANGUAGE_CPP,
  YAML_LINK_TIME_OPTIMIZE,
  YAML_DEFINE,
  YAML_DEFINE_ASM,
  YAML_UNDEFINE,
  YAML_ADDPATH,
  YAML_ADDPATH_ASM,
  YAML_DELPATH,
  YAML_MISC,
  YAML_VARIABLES,
  YAML_CREATED_BY,
  YAML_CREATED_FOR,
  YAML_CDEFAULT,
  YAML_GENERATORS,
  YAML_EXECUTES,
};

const set<string> projectsKeys = {
  YAML_PROJECT,
  YAML_FORCONTEXT,
  YAML_NOTFORCONTEXT,
  YAML_WEST
};

const set<string> projectKeys = {
  YAML_DESCRIPTION,
  YAML_OUTPUT,
  YAML_PACKS,
  YAML_DEVICE,
  YAML_BOARD,
  YAML_PROCESSOR,
  YAML_COMPILER,
  YAML_OPTIMIZE,
  YAML_DEBUG,
  YAML_WARNINGS,
  YAML_LANGUAGE_C,
  YAML_LANGUAGE_CPP,
  YAML_LINK_TIME_OPTIMIZE,
  YAML_DEFINE,
  YAML_DEFINE_ASM,
  YAML_UNDEFINE,
  YAML_ADDPATH,
  YAML_ADDPATH_ASM,
  YAML_DELPATH,
  YAML_MISC,
  YAML_COMPONENTS,
  YAML_GROUPS,
  YAML_LAYERS,
  YAML_SETUPS,
  YAML_CONNECTIONS,
  YAML_LINKER,
  YAML_GENERATORS,
  YAML_RTE,
  YAML_EXECUTES,
};

const set<string> setupKeys = {
  YAML_SETUP,
  YAML_FORCONTEXT,
  YAML_NOTFORCONTEXT,
  YAML_FORCOMPILER,
  YAML_OUTPUT,
  YAML_OPTIMIZE,
  YAML_DEBUG,
  YAML_WARNINGS,
  YAML_LANGUAGE_C,
  YAML_LANGUAGE_CPP,
  YAML_LINK_TIME_OPTIMIZE,
  YAML_DEFINE,
  YAML_DEFINE_ASM,
  YAML_UNDEFINE,
  YAML_ADDPATH,
  YAML_ADDPATH_ASM,
  YAML_DELPATH,
  YAML_MISC,
  YAML_LINKER,
  YAML_PROCESSOR,
};

const set<string> layerKeys = {
  YAML_DESCRIPTION,
  YAML_FORBOARD,
  YAML_FORDEVICE,
  YAML_TYPE,
  YAML_PACKS,
  YAML_DEVICE,
  YAML_BOARD,
  YAML_PROCESSOR,
  YAML_COMPILER,
  YAML_OPTIMIZE,
  YAML_DEBUG,
  YAML_WARNINGS,
  YAML_LANGUAGE_C,
  YAML_LANGUAGE_CPP,
  YAML_LINK_TIME_OPTIMIZE,
  YAML_DEFINE,
  YAML_DEFINE_ASM,
  YAML_UNDEFINE,
  YAML_ADDPATH,
  YAML_ADDPATH_ASM,
  YAML_DELPATH,
  YAML_MISC,
  YAML_COMPONENTS,
  YAML_GROUPS,
  YAML_CONNECTIONS,
  YAML_LINKER,
  YAML_GENERATORS,
};

const set<string> cbuildSetKeys = {
  YAML_GENERATED_BY,
  YAML_CONTEXTS,
  YAML_COMPILER,
  YAML_ENVIRONMENT,
};

const set<string> cbuildPackKeys = {
  YAML_RESOLVED_PACKS,
};

const set<string> targetTypeKeys = {
  YAML_TYPE,
  YAML_DEVICE,
  YAML_BOARD,
  YAML_MEMORY,
  YAML_PROCESSOR,
  YAML_COMPILER,
  YAML_OPTIMIZE,
  YAML_DEBUG,
  YAML_WARNINGS,
  YAML_LANGUAGE_C,
  YAML_LANGUAGE_CPP,
  YAML_LINK_TIME_OPTIMIZE,
  YAML_DEFINE,
  YAML_DEFINE_ASM,
  YAML_UNDEFINE,
  YAML_ADDPATH,
  YAML_ADDPATH_ASM,
  YAML_DELPATH,
  YAML_MISC,
  YAML_VARIABLES,
  YAML_CONTEXT_MAP,
  YAML_TARGET_SET,
  YAML_WEST_DEFS,
};

const set<string> buildTypeKeys = {
  YAML_TYPE,
  YAML_PROCESSOR,
  YAML_COMPILER,
  YAML_OPTIMIZE,
  YAML_DEBUG,
  YAML_WARNINGS,
  YAML_LANGUAGE_C,
  YAML_LANGUAGE_CPP,
  YAML_LINK_TIME_OPTIMIZE,
  YAML_DEFINE,
  YAML_DEFINE_ASM,
  YAML_UNDEFINE,
  YAML_ADDPATH,
  YAML_ADDPATH_ASM,
  YAML_DELPATH,
  YAML_MISC,
  YAML_VARIABLES,
  YAML_CONTEXT_MAP,
  YAML_WEST_DEFS,
};

const set<string> outputDirsKeys = {
  YAML_OUTPUT_CPRJDIR,
  YAML_OUTPUT_INTDIR,
  YAML_OUTPUT_OUTDIR,
  YAML_OUTPUT_TMPDIR,
};

const set<string> outputKeys = {
  YAML_BASE_NAME,
  YAML_TYPE,
};

const set<string> generatorsKeys = {
  YAML_BASE_DIR,
  YAML_OPTIONS,
  YAML_NAME,
  YAML_MAP,
};

const set<string> rteKeys = {
  YAML_BASE_DIR,
};

const set<string> processorKeys = {
  YAML_TRUSTZONE,
  YAML_FPU,
  YAML_DSP,
  YAML_MVE,
  YAML_ENDIAN,
  YAML_BRANCH_PROTECTION,
};

const set<string> miscKeys = {
  YAML_FORCOMPILER,
  YAML_MISC_C,
  YAML_MISC_CPP,
  YAML_MISC_C_CPP,
  YAML_MISC_ASM,
  YAML_MISC_LINK,
  YAML_MISC_LINK_C,
  YAML_MISC_LINK_CPP,
  YAML_MISC_LIB,
  YAML_MISC_LIBRARY,
};

const set<string> packsKeys = {
  YAML_PACK,
  YAML_PATH,
  YAML_FORCONTEXT,
  YAML_NOTFORCONTEXT,
};

const set<string> resolvedPacksKeys = {
  YAML_RESOLVED_PACK,
  YAML_SELECTED_BY_PACK,
  YAML_SELECTED_BY,
};

const set<string> componentsKeys = {
  YAML_COMPONENT,
  YAML_CONDITION,
  YAML_FROM_PACK,
  YAML_FORCONTEXT,
  YAML_NOTFORCONTEXT,
  YAML_COMPILER,
  YAML_OPTIMIZE,
  YAML_DEBUG,
  YAML_WARNINGS,
  YAML_LANGUAGE_C,
  YAML_LANGUAGE_CPP,
  YAML_LINK_TIME_OPTIMIZE,
  YAML_DEFINE,
  YAML_DEFINE_ASM,
  YAML_UNDEFINE,
  YAML_ADDPATH,
  YAML_ADDPATH_ASM,
  YAML_DELPATH,
  YAML_MISC,
  YAML_INSTANCES,
};

const set<string> connectionsKeys = {
  YAML_CONNECT,
  YAML_SET,
  YAML_INFO,
  YAML_PROVIDES,
  YAML_CONSUMES,
};

const set<string> linkerKeys = {
  YAML_AUTO,
  YAML_REGIONS,
  YAML_SCRIPT,
  YAML_DEFINE,
  YAML_FORCOMPILER,
  YAML_FORCONTEXT,
  YAML_NOTFORCONTEXT,
};

const set<string> layersKeys = {
  YAML_LAYER,
  YAML_TYPE,
  YAML_OPTIONAL,
  YAML_FORCONTEXT,
  YAML_NOTFORCONTEXT,
};

const set<string> groupsKeys = {
  YAML_GROUP,
  YAML_FORCONTEXT,
  YAML_NOTFORCONTEXT,
  YAML_FORCOMPILER,
  YAML_GROUPS,
  YAML_FILES,
  YAML_COMPILER,
  YAML_OPTIMIZE,
  YAML_DEBUG,
  YAML_WARNINGS,
  YAML_LANGUAGE_C,
  YAML_LANGUAGE_CPP,
  YAML_LINK_TIME_OPTIMIZE,
  YAML_DEFINE,
  YAML_DEFINE_ASM,
  YAML_UNDEFINE,
  YAML_ADDPATH,
  YAML_ADDPATH_ASM,
  YAML_DELPATH,
  YAML_MISC,
};

const set<string> filesKeys = {
  YAML_FILE,
  YAML_FORCONTEXT,
  YAML_NOTFORCONTEXT,
  YAML_FORCOMPILER,
  YAML_CATEGORY,
  YAML_COMPILER,
  YAML_OPTIMIZE,
  YAML_DEBUG,
  YAML_WARNINGS,
  YAML_LANGUAGE_C,
  YAML_LANGUAGE_CPP,
  YAML_LINK_TIME_OPTIMIZE,
  YAML_LINK,
  YAML_DEFINE,
  YAML_DEFINE_ASM,
  YAML_UNDEFINE,
  YAML_ADDPATH,
  YAML_ADDPATH_ASM,
  YAML_DELPATH,
  YAML_MISC,
};

const set<string> executesKeys = {
  YAML_EXECUTE,
  YAML_RUN,
  YAML_ALWAYS,
  YAML_INPUT,
  YAML_OUTPUT,
  YAML_FORCONTEXT,
  YAML_NOTFORCONTEXT,
};

const set<string> westKeys = {
  YAML_PROJECT_ID,
  YAML_APP_PATH,
  YAML_BOARD,
  YAML_DEVICE,
  YAML_WEST_DEFS,
  YAML_WEST_OPT
};

const map<string, set<string>> sequences = {
  {YAML_PROJECTS, projectsKeys},
  {YAML_TARGETTYPES, targetTypeKeys},
  {YAML_BUILDTYPES, buildTypeKeys},
  {YAML_MISC, miscKeys},
  {YAML_PACKS, packsKeys},
  {YAML_RESOLVED_PACKS, resolvedPacksKeys},
  {YAML_COMPONENTS, componentsKeys},
  {YAML_CONNECTIONS, connectionsKeys},
  {YAML_LAYERS, layersKeys},
  {YAML_GROUPS, groupsKeys},
  {YAML_FILES, filesKeys},
  {YAML_SETUPS, setupKeys},
  {YAML_LINKER, linkerKeys},
  {YAML_EXECUTES, executesKeys},
  {YAML_INPUT, {}},
  {YAML_OUTPUT, {}},
};

const map<string, set<string>> mappings = {
  {YAML_PROCESSOR, processorKeys},
  {YAML_OUTPUTDIRS, outputDirsKeys},
  {YAML_OUTPUT, outputKeys},
  {YAML_GENERATORS, generatorsKeys},
  {YAML_RTE, rteKeys},
  {YAML_WEST, westKeys},
};

bool ProjMgrYamlParser::ValidateCdefault(const string& input, const YAML::Node& root) {
  const set<string> rootKeys = {
    YAML_DEFAULT,
  };
  if (!ValidateKeys(input, root, rootKeys)) {
    return false;
  }
  const YAML::Node& defaultNode = root[YAML_DEFAULT];
  if (!ValidateKeys(input, defaultNode, defaultKeys)) {
    return false;
  }
  return true;
}

bool ProjMgrYamlParser::ValidateCsolution(const string& input, const YAML::Node& root) {
  const set<string> rootKeys = {
    YAML_SOLUTION,
  };
  if (!ValidateKeys(input, root, rootKeys)) {
    return false;
  }
  const YAML::Node& solutionNode = root[YAML_SOLUTION];
  if (!ValidateKeys(input, solutionNode, solutionKeys)) {
    return false;
  }
  return true;
}

bool ProjMgrYamlParser::ValidateCproject(const string& input, const YAML::Node& root) {
  const set<string> rootKeys = {
    YAML_PROJECT,
  };
  if (!ValidateKeys(input, root, rootKeys)) {
    return false;
  }
  const YAML::Node& projectNode = root[YAML_PROJECT];
  if (!ValidateKeys(input, projectNode, projectKeys)) {
    return false;
  }
  return true;
}

bool ProjMgrYamlParser::ValidateClayer(const string& input, const YAML::Node& root) {
  const set<string> rootKeys = {
    YAML_LAYER,
  };
  if (!ValidateKeys(input, root, rootKeys)) {
    return false;
  }
  const YAML::Node& layerNode = root[YAML_LAYER];
  if (!ValidateKeys(input, layerNode, layerKeys)) {
    return false;
  }
  return true;
}

bool ProjMgrYamlParser::ValidateCbuildPack(const string& input, const YAML::Node& root) {
  const set<string> rootKeys = {
    YAML_CBUILD_PACK,
  };
  if (!ValidateKeys(input, root, rootKeys)) {
    return false;
  }
  const YAML::Node& cbuildPackNode = root[YAML_CBUILD_PACK];
  if (!ValidateKeys(input, cbuildPackNode, cbuildPackKeys)) {
    return false;
  }
  return true;
}

bool ProjMgrYamlParser::ValidateCbuildSet(const string& input, const YAML::Node& root) {
  const set<string> rootKeys = {
    YAML_CBUILD_SET,
  };
  if (!ValidateKeys(input, root, rootKeys)) {
    return false;
  }
  const YAML::Node& contextSetNode = root[YAML_CBUILD_SET];
  if (!ValidateKeys(input, contextSetNode, cbuildSetKeys)) {
    return false;
  }
  return true;
}

bool ProjMgrYamlParser::ValidateKeys(const string& input, const YAML::Node& parent, const set<string>& keys) {
  bool valid = true;
  for (const auto& item : parent) {
    const string& key = item.first.as<string>();
    if (keys.find(item.first.as<string>()) == keys.end()) {
      ProjMgrLogger::Get().Warn("key '" + item.first.as<string>() + "' was not recognized", "", input, item.first.Mark().line + 1, item.first.Mark().column + 1);
    }
    if (item.second.IsSequence()) {
      if (!ValidateSequence(input, item.second, key)) {
        valid = false;
      }
    } else if ((sequences.find(key) != sequences.end()) && (mappings.find(key) == mappings.end())) {
      ProjMgrLogger::Get().Error("node '" + item.first.as<string>() + "' shall contain sequence elements", "", input, item.first.Mark().line + 1, item.first.Mark().column + 1);
      valid = false;
    }
    if (item.second.IsMap()) {
      if (!ValidateMapping(input, item.second, key)) {
        valid = false;
      }
    } else if ((mappings.find(key) != mappings.end()) && (sequences.find(key) == sequences.end())) {
      ProjMgrLogger::Get().Error("node '" + item.first.as<string>() + "' shall contain mapping elements", "", input, item.first.Mark().line + 1, item.first.Mark().column + 1);
      valid = false;
    }
  }
  return valid;
}

bool ProjMgrYamlParser::ValidateSequence(const string& input, const YAML::Node& parent, const string& key) {
  bool valid = true;
  if (sequences.find(key) != sequences.end()) {
    set<string> keys = sequences.at(key);
    for (const YAML::Node& entry : parent) {
      if (!ValidateKeys(input, entry, keys)) {
        valid = false;
      }
    }
  }
  return valid;
}

bool ProjMgrYamlParser::ValidateMapping(const string& input, const YAML::Node& parent, const string& key) {
  bool valid = true;
  if (mappings.find(key) != mappings.end()) {
    set<string> keys = mappings.at(key);
    if (!ValidateKeys(input, parent, keys)) {
      valid = false;
    }
  }
  return valid;
}
