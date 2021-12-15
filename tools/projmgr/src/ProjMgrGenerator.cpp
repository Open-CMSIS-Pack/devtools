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

bool ProjMgrGenerator::GenerateCprj(ProjMgrProjectItem& project, const string& filename) {
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
  GenerateCprjInfo(rootElement, project.description);

  // Create first level elements
  XMLTreeElement* packagesElement = rootElement->CreateElement("packages");
  XMLTreeElement* compilersElement = rootElement->CreateElement("compilers");
  XMLTreeElement* targetElement = rootElement->CreateElement("target");
  XMLTreeElement* componentsElement = rootElement->CreateElement("components");
  XMLTreeElement* filesElement = rootElement->CreateElement("files");

  // Packages
  if (packagesElement) {
    GenerateCprjPackages(packagesElement, project.packages);
  }

  // Compilers
  if (compilersElement) {
    GenerateCprjCompilers(compilersElement, project.toolchain);
  }

  // Target
  if (targetElement) {
    GenerateCprjTarget(targetElement, project);
  }

  // Components
  if (componentsElement) {
    GenerateCprjComponents(componentsElement, project);
  }

  // Files
  if (filesElement) {
    GenerateCprjGroups(filesElement, project.groups);
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

void ProjMgrGenerator::GenerateCprjTarget(XMLTreeElement* element, const ProjMgrProjectItem& project) {
  static constexpr const char* DEVICE_ATTRIBUTES[] = { "Ddsp", "Dendian", "Dfpu", "Dmve", "Dname", "Dsecure", "Dtz", "Dvendor" };
  map<string, string> attributes = project.targetAttributes;
  if (attributes["Dendian"] == "Configurable") {
    attributes["Dendian"].erase();
  }
  for (const auto& name : DEVICE_ATTRIBUTES) {
    const string& value = attributes[name];
    SetAttribute(element, name, value);
  }

  XMLTreeElement* targetOutputElement = element->CreateElement("output");
  if (targetOutputElement) {
    targetOutputElement->AddAttribute("name", project.name);
    targetOutputElement->AddAttribute("type", project.outputType);
  }
}

void ProjMgrGenerator::GenerateCprjComponents(XMLTreeElement* element, const ProjMgrProjectItem& project) {
  static constexpr const char* COMPONENT_ATTRIBUTES[] = { "Cbundle", "Cclass", "Cgroup", "Csub", "Cvariant", "Cvendor", "Cversion" };
  for (const auto& component : project.components) {
    XMLTreeElement* componentElement = element->CreateElement("component");
    if (componentElement) {
      for (const auto& name : COMPONENT_ATTRIBUTES) {
        const string& value = component.second->GetAttribute(name);
        SetAttribute(componentElement, name, value);
      }
      // TODO Generate toolchain settings (warnings, debug, misc, includes, defines)
      // GenerateCprjMisc(componentElement, project.componentsMisc.at(componentId));
    }
  }
}

void ProjMgrGenerator::GenerateCprjMisc(XMLTreeElement* element, const MiscItem& misc) {
  if (!misc.all.empty()) {
    XMLTreeElement* asflagsElement = element->CreateElement("asflags");
    if (asflagsElement) {
      asflagsElement->AddAttribute("add", GetStringFromVector(misc.all));
    }
    XMLTreeElement* cflagsElement = element->CreateElement("cflags");
    if (cflagsElement) {
      cflagsElement->AddAttribute("add", GetStringFromVector(misc.all));
    }
    XMLTreeElement* cxxflagsElement = element->CreateElement("cxxflags");
    if (cxxflagsElement) {
      cxxflagsElement->AddAttribute("add", GetStringFromVector(misc.all));
    }
  } else {
    if (!misc.as.empty()) {
      XMLTreeElement* asflagsElement = element->CreateElement("asflags");
      if (asflagsElement) {
        asflagsElement->AddAttribute("add", GetStringFromVector(misc.as));
      }
    }
    if (!misc.c.empty()) {
      XMLTreeElement* cflagsElement = element->CreateElement("cflags");
      if (cflagsElement) {
        cflagsElement->AddAttribute("add", GetStringFromVector(misc.c));
      }
    }
    if (!misc.cpp.empty()) {
      XMLTreeElement* cxxflagsElement = element->CreateElement("cxxflags");
      if (cxxflagsElement) {
        cxxflagsElement->AddAttribute("add", GetStringFromVector(misc.cpp));
      }
    }
  }
}

void ProjMgrGenerator::GenerateCprjGroups(XMLTreeElement* element, const vector<GroupNode>& groups) {
  for (const auto& groupNode : groups) {
    XMLTreeElement* groupElement = element->CreateElement("group");
    if (groupElement) {
      if (!groupNode.group.empty()) {
        groupElement->AddAttribute("name", groupNode.group);
      }


      // TODO Generate toolchain settings (warnings, debug, misc, includes, defines)
      // GenerateCprjMisc(groupElement, groupNode.misc);

      for (const auto& fileNode : groupNode.files) {
        XMLTreeElement* fileElement = groupElement->CreateElement("file");
        fileElement->AddAttribute("name", fileNode.file);
        if (fileElement) {
          if (!fileNode.category.empty()) {
            fileElement->AddAttribute("category", fileNode.category);
          }

          // TODO Generate toolchain settings (warnings, debug, misc, includes, defines)
          //GenerateCprjMisc(fileElement, fileNode.misc);
        }

      }
      GenerateCprjGroups(groupElement, groupNode.groups);
    }
  }
}

void ProjMgrGenerator::SetAttribute(XMLTreeElement* element, const string& name, const string& value) {
  if (!value.empty()) {
    element->AddAttribute(name, value);
  }
}

const string ProjMgrGenerator::GetStringFromVector(const vector<string>& vector) {
  if (!vector.size()) {
    return RteUtils::EMPTY_STRING;
  }
  ostringstream stream;
  copy(vector.begin(), vector.end(), ostream_iterator<string>(stream, " "));
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
