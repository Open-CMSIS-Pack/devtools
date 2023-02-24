/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgrGenerator.h"
#include "ProjMgrUtils.h"
#include "ProductInfo.h"

#include "RteFsUtils.h"
#include "XmlFormatter.h"
#include "XMLTreeSlim.h"

#include <algorithm>
#include <iomanip>
#include <iterator>
#include <fstream>

using namespace std;

static constexpr const char* SCHEMA_FILE = "CPRJ.xsd";    // XML schema file name
static constexpr const char* SCHEMA_VERSION = "1.2.0";    // XML schema version


ProjMgrGenerator::ProjMgrGenerator(void) {
  // Reserved
}

ProjMgrGenerator::~ProjMgrGenerator(void) {
  // Reserved
}

bool ProjMgrGenerator::GenerateCprj(ContextItem& context, const string& filename, bool nonLocked) {
  // Root
  XMLTreeSlim cprjTree = XMLTreeSlim();
  XMLTreeElement* rootElement;
  rootElement = cprjTree.CreateElement("cprj");
  if (!rootElement) {
    return false;
  }

  // Created
  GenerateCprjCreated(rootElement);

  // Info
  GenerateCprjInfo(rootElement, context.description);

  // Create first level elements
  XMLTreeElement* packagesElement = rootElement->CreateElement("packages");
  XMLTreeElement* compilersElement = rootElement->CreateElement("compilers");
  XMLTreeElement* targetElement = rootElement->CreateElement("target");
  XMLTreeElement* componentsElement = rootElement->CreateElement("components");
  XMLTreeElement* filesElement = rootElement->CreateElement("files");

  // Packages
  if (packagesElement) {
    GenerateCprjPackages(packagesElement, context, nonLocked);
  }

  // Compilers
  if (compilersElement) {
    GenerateCprjCompilers(compilersElement, context);
  }

  // Target
  if (targetElement) {
    GenerateCprjTarget(targetElement, context);
  }

  // Components
  if (componentsElement) {
    GenerateCprjComponents(componentsElement, context, nonLocked);
  }

  // Files
  if (filesElement) {
    GenerateCprjGroups(filesElement, context.groups, context.toolchain.name);
  }

  // Remove empty files element
  if (!filesElement->HasChildren()) {
    rootElement->RemoveChild("files", true);
  }

  // Save CPRJ
  if (!WriteXmlFile(filename, &cprjTree)) {
    return false;
  }

  return true;
}

void ProjMgrGenerator::GenerateCprjCreated(XMLTreeElement* element) {
  const string& tool = string(ORIGINAL_FILENAME) + " " + string(VERSION_STRING);
  const string& timestamp = GetLocalTimestamp();
  XMLTreeElement* createdElement = element->CreateElement("created");
  if (createdElement) {
    createdElement->AddAttribute("tool", tool);
    createdElement->AddAttribute("timestamp", string(timestamp));
  }
}

void ProjMgrGenerator::GenerateCprjInfo(XMLTreeElement* element, const string& description) {
  XMLTreeElement* infoElement = element->CreateElement("info");
  if (infoElement) {
    infoElement->AddAttribute("isLayer", "false");
    const string& infoDescription = description.empty() ? "Automatically generated project" : description;
    XMLTreeElement* infoDescriptionElement = infoElement->CreateElement("description");
    if (infoDescriptionElement) {
      infoDescriptionElement->SetText(infoDescription);
    }
  }
}

void ProjMgrGenerator::GenerateCprjPackages(XMLTreeElement* element, const ContextItem& context, bool nonLocked) {
  for (const auto& package : context.packages) {
    XMLTreeElement* packageElement = element->CreateElement("package");
    if (packageElement) {
      packageElement->AddAttribute("name", package.second->GetName());
      packageElement->AddAttribute("vendor", package.second->GetVendorString());
      const string& pdscFile = package.second->GetPackageFileName();
      if (!nonLocked) {
        const string& version = package.second->GetVersionString() + ":" + package.second->GetVersionString();
        packageElement->AddAttribute("version", version);
      }
      if (context.pdscFiles.find(pdscFile) != context.pdscFiles.end()) {
        if (nonLocked) {
          const string& version = context.pdscFiles.at(pdscFile).second;
          if (!version.empty()) {
            packageElement->AddAttribute("version", version);
          }
        }
        const string& packPath = context.pdscFiles.at(pdscFile).first;
        if (!packPath.empty()) {
          error_code ec;
          packageElement->AddAttribute("path", fs::relative(packPath, context.directories.cprj, ec).generic_string());
        }
      }
    }
  }
}

void ProjMgrGenerator::GenerateCprjCompilers(XMLTreeElement* element, const ContextItem& context) {
  XMLTreeElement* compilerElement = element->CreateElement("compiler");
  if (compilerElement) {
    compilerElement->AddAttribute("name", context.toolchain.name);
    // set minimum version according to registered/supported toolchain
    string versionRange = context.toolchain.version;
    // set maximum version according to solution requirements
    string name, minVer, maxVer;
    ProjMgrUtils::ExpandCompilerId(context.compiler, name, minVer, maxVer);
    if (!maxVer.empty()) {
      versionRange += ":" + maxVer;
    }
    compilerElement->AddAttribute("version", versionRange);
  }
}

void ProjMgrGenerator::GenerateCprjTarget(XMLTreeElement* element, const ContextItem& context) {
  constexpr const char* DEVICE_ATTRIBUTES[] = { "Ddsp", "Dendian", "Dfpu", "Dmve", "Dname", "Pname", "Dsecure", "Dtz", "Dvendor" };
  map<string, string> attributes = context.targetAttributes;
  if (attributes["Dendian"] == "Configurable") {
    attributes["Dendian"].erase();
  }
  for (const auto& name : DEVICE_ATTRIBUTES) {
    const string& value = attributes[name];
    SetAttribute(element, name, value);
  }

  // board attributes
  constexpr const char* BOARD_ATTRIBUTES[] = { "Bvendor", "Bname", "Brevision", "Bversion" /*deprecated*/};
  for (const auto& name : BOARD_ATTRIBUTES) {
    const string& value = attributes[name];
    SetAttribute(element, name, value);
  }

  XMLTreeElement* targetOutputElement = element->CreateElement("output");
  if (targetOutputElement) {
    targetOutputElement->AddAttribute("name", context.rteActiveTarget->GetName());
    targetOutputElement->AddAttribute("type", context.outputType);
    targetOutputElement->AddAttribute("intdir", context.directories.intdir);
    targetOutputElement->AddAttribute("outdir", context.directories.outdir);
    targetOutputElement->AddAttribute("rtedir", context.directories.rte);
    for (const auto& [type, file] : context.outputFiles) {
      targetOutputElement->AddAttribute(type, file, false);
    }
  }

  GenerateCprjOptions(element, context.controls.processed);
  GenerateCprjMisc(element, context.controls.processed.misc);
  GenerateCprjLinkerScript(element, context.toolchain.name, context.linkerScript);
  GenerateCprjVector(element, context.controls.processed.defines, "defines");
  GenerateCprjVector(element, context.controls.processed.addpaths, "includes");
}

void ProjMgrGenerator::GenerateCprjComponents(XMLTreeElement* element, const ContextItem& context, bool nonLocked) {
  static constexpr const char* COMPONENT_ATTRIBUTES[] = { "Cbundle", "Cclass", "Cgroup", "Csub", "Cvariant", "Cvendor"};
  static constexpr const char* versionAttribute = "Cversion";
  for (const auto& [componentId, component] : context.components) {
    XMLTreeElement* componentElement = element->CreateElement("component");
    if (componentElement) {
      for (const auto& name : COMPONENT_ATTRIBUTES) {
        const string& value = component.instance->GetAttribute(name);
        SetAttribute(componentElement, name, value);
      }
      if (context.configFiles.find(componentId) != context.configFiles.end()) {
        const string& rteDir = component.instance->GetAttribute("rtedir");
        if (!rteDir.empty()) {
          error_code ec;
          // Adjust component's rtePath relative to cprj
          SetAttribute(componentElement, "rtedir", fs::relative(context.cproject->directory + "/" + rteDir, context.directories.cprj, ec).generic_string());
        }
      }
      if (!component.generator.empty()) {
        const string& genDir = component.instance->GetAttribute("gendir");
        if (!genDir.empty()) {
          error_code ec;
          // Adjust component's genDir relative to cprj
          SetAttribute(componentElement, "gendir", fs::relative(context.cproject->directory + "/" + genDir, context.directories.cprj, ec).generic_string());
        }
      }

      // Check whether non-locked option is not set or component version is required from user
      if (!nonLocked || !RteUtils::GetSuffix(component.item->component, *ProjMgrUtils::PREFIX_CVERSION, true).empty()) {

        // Set Cversion attribute
        SetAttribute(componentElement, versionAttribute, component.instance->GetAttribute(versionAttribute));

        // Config files
        for (const auto& configFileMap : context.configFiles) {
          if (configFileMap.first == componentId) {
            for (const auto& [configFileId, configFile] : configFileMap.second) {
              XMLTreeElement* fileElement = componentElement->CreateElement("file");
              if (fileElement) {
                SetAttribute(fileElement, "attr", "config");
                SetAttribute(fileElement, "name", configFileId);
                SetAttribute(fileElement, "category", configFile->GetAttribute("category"));
                const auto originalFile = configFile->GetFile(context.rteActiveTarget->GetName());
                SetAttribute(fileElement, "version", originalFile->GetVersionString());
              }
            }
          }
        }
      }

      GenerateCprjOptions(componentElement, component.item->build);
      GenerateCprjMisc(componentElement, component.item->build.misc);
      GenerateCprjVector(componentElement, component.item->build.defines, "defines");
      GenerateCprjVector(componentElement, component.item->build.undefines, "undefines");
      GenerateCprjVector(componentElement, component.item->build.addpaths, "includes");
      GenerateCprjVector(componentElement, component.item->build.delpaths, "excludes");
    }

  }
}

void ProjMgrGenerator::GenerateCprjVector(XMLTreeElement* element, const vector<string>& vec, string tag) {
  if (!vec.empty()) {
    XMLTreeElement* childElement = element->CreateElement(tag);
    childElement->SetText(GetStringFromVector(vec, ";"));
  }
}

void ProjMgrGenerator::GenerateCprjOptions(XMLTreeElement* element, const BuildType& buildType) {

  if (buildType.optimize.empty() && buildType.debug.empty() && buildType.warnings.empty()) {
    return;
  }

  XMLTreeElement *optionsElement = element->CreateElement("options");
  if (optionsElement)
  {
    SetAttribute(optionsElement, "optimize", buildType.optimize);
    SetAttribute(optionsElement, "debug", buildType.debug);
    SetAttribute(optionsElement, "warnings", buildType.warnings);
  }
}

void ProjMgrGenerator::GenerateCprjMisc(XMLTreeElement* element, const vector<MiscItem>& miscVec) {
  for (const auto& miscIt : miscVec) {
    GenerateCprjMisc(element, miscIt);
  }
}

void ProjMgrGenerator::GenerateCprjMisc(XMLTreeElement* element, const MiscItem& misc) {
  const map<string, vector<string>>& FLAGS_MATRIX = {
    {"asflags", misc.as},
    {"cflags", misc.c},
    {"cxxflags", misc.cpp},
    {"ldflags", misc.link},
    {"ldcflags", misc.link_c},
    {"ldcxxflags", misc.link_cpp},
    {"arflags", misc.lib}
  };
  for (const auto& flags : FLAGS_MATRIX) {
    if (!flags.second.empty()) {
      XMLTreeElement* flagsElement = element->CreateElement(flags.first);
      if (flagsElement) {
        flagsElement->AddAttribute("add", GetStringFromVector(flags.second, " "));
        flagsElement->AddAttribute("compiler", RteUtils::GetPrefix(misc.compiler, '@'));
      }
    }
  }
}

void ProjMgrGenerator::GenerateCprjLinkerScript(XMLTreeElement* element, const string& compiler, const string& linkerScript) {
  if (!linkerScript.empty()) {
    XMLTreeElement* ldflagsElement = nullptr;
    for (auto& child : element->GetChildren()) {
      if (child->GetTag() == "ldflags") {
        ldflagsElement = child;
        break;
      }
    }
    if (!ldflagsElement) {
      ldflagsElement = element->CreateElement("ldflags");
      ldflagsElement->AddAttribute("compiler", compiler);
    }
    if (ldflagsElement) {
      ldflagsElement->AddAttribute("file", linkerScript);
    }
  }
}

void ProjMgrGenerator::GenerateCprjGroups(XMLTreeElement* element, const vector<GroupNode>& groups, const string& compiler) {
  for (const auto& groupNode : groups) {
    XMLTreeElement* groupElement = element->CreateElement("group");
    if (groupElement) {
      if (!groupNode.group.empty()) {
        groupElement->AddAttribute("name", groupNode.group);
      }

      GenerateCprjOptions(groupElement, groupNode.build);
      GenerateCprjMisc(groupElement, groupNode.build.misc);
      GenerateCprjVector(groupElement, groupNode.build.defines, "defines");
      GenerateCprjVector(groupElement, groupNode.build.undefines, "undefines");
      GenerateCprjVector(groupElement, groupNode.build.addpaths, "includes");
      GenerateCprjVector(groupElement, groupNode.build.delpaths, "excludes");

      for (const auto& fileNode : groupNode.files) {
        XMLTreeElement* fileElement = groupElement->CreateElement("file");
        fileElement->AddAttribute("name", fileNode.file);
        if (fileElement) {
          fileElement->AddAttribute("category", fileNode.category);

          GenerateCprjOptions(fileElement, fileNode.build);
          GenerateCprjMisc(fileElement, fileNode.build.misc);
          GenerateCprjVector(fileElement, fileNode.build.defines, "defines");
          GenerateCprjVector(fileElement, fileNode.build.undefines, "undefines");
          GenerateCprjVector(fileElement, fileNode.build.addpaths, "includes");
          GenerateCprjVector(fileElement, fileNode.build.delpaths, "excludes");
        }

      }
      GenerateCprjGroups(groupElement, groupNode.groups, compiler);
    }
  }
}

void ProjMgrGenerator::SetAttribute(XMLTreeElement* element, const string& name, const string& value) {
  if (!value.empty()) {
    element->AddAttribute(name, value);
  }
}

const string ProjMgrGenerator::GetStringFromVector(const vector<string>& vector, const char* delimiter) {
  if (!vector.size()) {
    return RteUtils::EMPTY_STRING;
  }
  ostringstream stream;
  copy(vector.begin(), vector.end(), ostream_iterator<string>(stream, delimiter));
  string s = stream.str();
  // Remove last delimiter
  s.pop_back();
  return s;
}

const string ProjMgrGenerator::GetLocalTimestamp() {
  // Timestamp
  time_t rawtime = time(nullptr);
  struct tm* timeinfo = localtime(&rawtime);
  if (timeinfo == nullptr) {
    return "0000-00-00T00:00:00";
  }
  ostringstream timestamp;
  timestamp << put_time(timeinfo, "%FT%T");
  return timestamp.str();
}

bool ProjMgrGenerator::WriteXmlFile(const string& file, XMLTree* tree) {
  // Format Xml content
  XmlFormatter xmlFormatter(tree, SCHEMA_FILE, SCHEMA_VERSION);
  const string& xmlContent = xmlFormatter.GetContent();

  // Save file
  ofstream xmlFile(file);
  if (!xmlFile) {
    return false;
  }

  xmlFile << xmlContent;
  xmlFile << std::endl;
  xmlFile.close();

  return true;
}
