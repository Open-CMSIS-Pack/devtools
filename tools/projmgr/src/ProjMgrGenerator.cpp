/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgrGenerator.h"
#include "ProductInfo.h"

#include "RteFsUtils.h"
#include "XmlFormatter.h"
#include "XMLTreeSlim.h"

#include <algorithm>
#include <iterator>
#include <fstream>

using namespace std;

static constexpr const char* SCHEMA_FILE = "PACK.xsd";    // XML schema file name
static constexpr const char* SCHEMA_VERSION = "1.7.2";    // XML schema version


ProjMgrGenerator::ProjMgrGenerator(void) {
  // Reserved
}

ProjMgrGenerator::~ProjMgrGenerator(void) {
  // Reserved
}

bool ProjMgrGenerator::GenerateCprj(ContextItem& context, const string& filename) {
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
    GenerateCprjPackages(packagesElement, context.packages);
  }

  // Compilers
  if (compilersElement) {
    GenerateCprjCompilers(compilersElement, context.toolchain);
  }

  // Target
  if (targetElement) {
    GenerateCprjTarget(targetElement, context);
  }

  // Components
  if (componentsElement) {
    GenerateCprjComponents(componentsElement, context);
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

void ProjMgrGenerator::GenerateCprjPackages(XMLTreeElement* element, const std::map<string, RtePackage*>& packages) {
  for (const auto& package : packages) {
    XMLTreeElement* packageElement = element->CreateElement("package");
    if (packageElement) {
      packageElement->AddAttribute("name", package.second->GetName());
      packageElement->AddAttribute("vendor", package.second->GetVendorName());
      packageElement->AddAttribute("version", package.second->GetVersionString());
    }
  }
}

void ProjMgrGenerator::GenerateCprjCompilers(XMLTreeElement* element, const ToolchainItem& toolchain) {
  XMLTreeElement* compilerElement = element->CreateElement("compiler");
  if (compilerElement) {
    compilerElement->AddAttribute("name", toolchain.name);
    if (!toolchain.version.empty()) {
      compilerElement->AddAttribute("version", toolchain.version);
    }
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
  constexpr const char* BOARD_ATTRIBUTES[] = { "Bvendor", "Bname", "Bversion" };
  for (const auto& name : BOARD_ATTRIBUTES) {
    const string& value = attributes[name];
    SetAttribute(element, name, value);
  }

  XMLTreeElement* targetOutputElement = element->CreateElement("output");
  if (targetOutputElement) {
    targetOutputElement->AddAttribute("name", context.name);
    targetOutputElement->AddAttribute("type", context.outputType);
    targetOutputElement->AddAttribute("intdir", context.directories.intdir);
    targetOutputElement->AddAttribute("outdir", context.directories.outdir);
  }

  // TODO Generate toolchain settings (warnings, debug, includes)
  GenerateCprjMisc(element, context.misc, context.toolchain.name);
  GenerateCprjLinkerScript(element, context.toolchain.name, context.linkerScript);
  GenerateCprjVector(element, context.defines, "defines");
  GenerateCprjVector(element, context.includes, "includes");
}

void ProjMgrGenerator::GenerateCprjComponents(XMLTreeElement* element, const ContextItem& context) {
  static constexpr const char* COMPONENT_ATTRIBUTES[] = { "Cbundle", "Cclass", "Cgroup", "Csub", "Cvariant", "Cvendor", "Cversion" };
  for (const auto& component : context.components) {
    XMLTreeElement* componentElement = element->CreateElement("component");
    if (componentElement) {
      for (const auto& name : COMPONENT_ATTRIBUTES) {
        const string& value = component.second.first->GetAttribute(name);
        SetAttribute(componentElement, name, value);
      }

      // Config files
      for (const auto& configFileMap : context.configFiles) {
        if (configFileMap.first == component.first) {
          for (const auto& configFile : configFileMap.second) {
            XMLTreeElement* fileElement = componentElement->CreateElement("file");
            if (fileElement) {
              SetAttribute(fileElement, "attr", "config");
              SetAttribute(fileElement, "name", configFile.first);
              SetAttribute(fileElement, "category", configFile.second->GetAttribute("category"));
              // TODO: Set config file version according to new PLM
              const auto originalFile = configFile.second->GetFile(context.rteActiveTarget->GetName());
              SetAttribute(fileElement, "version", originalFile->GetVersionString());
            }
          }
        }
      }

      // TODO Generate toolchain settings (warnings, debug)
      GenerateCprjMisc(componentElement, component.second.second->build.misc, context.toolchain.name);
      GenerateCprjVector(componentElement, component.second.second->build.defines, "defines");
      GenerateCprjVector(componentElement, component.second.second->build.undefines, "undefines");
      GenerateCprjVector(componentElement, component.second.second->build.addpaths, "includes");
      GenerateCprjVector(componentElement, component.second.second->build.delpaths, "excludes");
    }

  }
}

void ProjMgrGenerator::GenerateCprjVector(XMLTreeElement* element, const vector<string>& vec, string tag) {
  if (!vec.empty()) {
    XMLTreeElement* childElement = element->CreateElement(tag);
    childElement->SetText(GetStringFromVector(vec, ";"));
  }
}

void ProjMgrGenerator::GenerateCprjMisc(XMLTreeElement* element, const vector<MiscItem>& misc, const std::string& compiler) {
  for (const auto& miscIt : misc) {
    const map<string, vector<string>>& FLAGS_MATRIX = {
      {"asflags", miscIt.as},
      {"cflags", miscIt.c},
      {"cxxflags", miscIt.cpp},
      {"ldflags", miscIt.link},
      {"arflags", miscIt.lib}
    };
    if (miscIt.compiler.empty() || (miscIt.compiler == compiler)) {
      for (const auto& flags : FLAGS_MATRIX) {
        if (!flags.second.empty()) {
          XMLTreeElement* flagsElement = element->CreateElement(flags.first);
          if (flagsElement) {
            flagsElement->AddAttribute("add", GetStringFromVector(flags.second, " "));
            flagsElement->AddAttribute("compiler", miscIt.compiler.empty() ? compiler : miscIt.compiler);
          }
        }
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

      // TODO Generate toolchain settings (warnings, debug)
      GenerateCprjMisc(groupElement, groupNode.build.misc, compiler);
      GenerateCprjVector(groupElement, groupNode.build.defines, "defines");
      GenerateCprjVector(groupElement, groupNode.build.undefines, "undefines");
      GenerateCprjVector(groupElement, groupNode.build.addpaths, "includes");
      GenerateCprjVector(groupElement, groupNode.build.delpaths, "excludes");

      for (const auto& fileNode : groupNode.files) {
        XMLTreeElement* fileElement = groupElement->CreateElement("file");
        fileElement->AddAttribute("name", fileNode.file);
        if (fileElement) {
          fileElement->AddAttribute("category", fileNode.category);

          // TODO Generate toolchain settings (warnings, debug)
          GenerateCprjMisc(fileElement, fileNode.build.misc, compiler);
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
