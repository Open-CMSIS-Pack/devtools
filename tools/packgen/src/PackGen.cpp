/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "PackGen.h"
#include "ProductInfo.h"

#include "RteFsUtils.h"
#include "XmlFormatter.h"
#include "CrossPlatform.h"

#include <cxxopts.hpp>
#include <iostream>
#include <fstream>
#include <functional>

using namespace std;

static constexpr const char* SCHEMA_FILE = "PACK.xsd";    // XML schema file name
static constexpr const char* SCHEMA_VERSION = "1.7.2";    // XML schema version

PackGen::PackGen(void) {
  // Reserved
}

PackGen::~PackGen(void) {
  if (nullptr != m_pdscTree) {
    delete m_pdscTree;
  }
}

int PackGen::RunPackGen(int argc, char *argv[]) {
  PackGen::Result result;
  PackGen generator;
  error_code ec;

  // Command line options
  const string& header = string(PRODUCT_NAME) + " " + string(VERSION_STRING) + " " + string(COPYRIGHT_NOTICE);
  cxxopts::Options options(ORIGINAL_FILENAME, header);
  cxxopts::ParseResult parseResult;
  bool nocheck = false, nozip = false;

  try {
    options.add_options()
      ("manifest", "", cxxopts::value<string>())
      ("o,output", "Output folder", cxxopts::value<string>())
      ("r,regenerate", "Regenerate CMake targets", cxxopts::value<bool>()->default_value("false"))
      ("v,verbose", "Verbose mode", cxxopts::value<bool>()->default_value("false"))
      ("c,nocheck", "Skip pack check", cxxopts::value<bool>()->default_value("false"))
      ("z,nozip", "Skip *.pack file creation", cxxopts::value<bool>()->default_value("false"))
      ("h,help", "Print usage")
      ;
    options.parse_positional({ "manifest" });
    options.positional_help("manifest.yml");

    parseResult = options.parse(argc, argv);
    generator.m_verbose = parseResult["verbose"].as<bool>();
    generator.m_regenerate = parseResult["regenerate"].as<bool>();
    nocheck = parseResult["nocheck"].as<bool>();
    nozip = parseResult["nozip"].as<bool>();
    if (parseResult.count("output")) {
      generator.m_outputRoot = parseResult["output"].as<string>();
    }
    if (parseResult.count("manifest")) {
      generator.m_manifest = parseResult["manifest"].as<string>();
    } else {
      // Print usage
      cout << options.help() << endl;
      return 0;
    }
  } catch (cxxopts::OptionException& e) {
    // Print error
    cerr << "packgen error: parsing command line failed!" << endl << e.what() << endl;
    return 1;
  }

  // Print usage
  if (parseResult.count("help")) {
    cout << options.help() << endl;
    return 0;
  }

  // Parse Manifest
  if (fs::exists(generator.m_manifest, ec)) {
    if (!generator.ParseManifest()) {
      return 1;
    }
  } else {
    cerr << "packgen error: Manifest file " << generator.m_manifest << " was not found" << endl;
    return 1;
  }

  // Check CMake
  result = generator.ExecCommand("cmake --version");
  if (result.second) {
    cerr << "packgen error: CMake was not found" << endl;
    return 1;
  }

  // Create CMake File API query
  if (!generator.CreateQuery()) {
    cerr << "packgen error: query creation failed" << endl;
    return 1;
  }

  // Parse CMake File API reply
  if (!generator.ParseTarget()) {
    cerr << "packgen error: target parsing failed" << endl;
    return 1;
  }

  // Elaborate component contents
  if (!generator.CreateComponents()) {
    cerr << "packgen error: component generation failed" << endl;
    return 1;
  }

  // Create PDSC file and copy pack files
  if (!generator.CreatePack()) {
    cerr << "packgen error: pack generation failed" << endl;
    return 1;
  }

  // Run PackChk
  if (!nocheck) {
    if (!generator.CheckPack()) {
      return 1;
    }
  }

  // Run 7zip
  if (!nozip) {
    if (!generator.CompressPack()) {
      return 1;
    }
  }

  return 0;
}

bool PackGen::ParseManifest(void) {
  try {
    YAML::Node manifest = YAML::LoadFile(m_manifest);

    // Get repository and output root
    error_code ec;
    m_manifest = fs::canonical(m_manifest, ec).generic_string();
    m_repoRoot = fs::path(m_manifest).parent_path().generic_string();
    if (m_outputRoot.empty()) {
      m_outputRoot = m_repoRoot;
    }
    else if (fs::path(m_outputRoot).is_relative()) {
      m_outputRoot = m_repoRoot + "/" + m_outputRoot;
    }

    // Build options for CMake File API (generation step)
    YAML::Node build = manifest["build"];
    for (const auto& item : build) {
      m_buildOptions.push_back({ item["name"].as<string>(),
                                 item["options"].as<string>() });
    }

    // Pack
    YAML::Node packs = manifest["packs"];
    for (const auto& item : packs) {
      packInfo pack;

      // Info
      ParseManifestInfo(item, pack);

      // Requirements
      ParseManifestRequirements(item, pack);

      // Releases
      ParseManifestReleases(item, pack);

      // Taxonomy
      ParseManifestTaxonomy(item, pack);

      // APIs
      ParseManifestApis(item, pack);

      // Components
      if (!ParseManifestComponents(item, pack)) {
        return false;
      }

      m_pack.push_back(pack);
    }

  } catch (YAML::Exception& e) {
    cerr << "packgen error: check YAML file!" << endl << e.what() << endl;
    return false;
  }

  return true;
}

void PackGen::ParseManifestInfo(const YAML::Node node, packInfo& pack) {
  pack.name = node["name"].as<string>();
  pack.description = node["description"].as<string>();
  pack.vendor = node["vendor"].as<string>();
  pack.license = node["license"].as<string>();
  pack.url = node["url"].as<string>();
}

void PackGen::ParseManifestReleases(const YAML::Node node, packInfo& pack) {
  YAML::Node releases = node["releases"];
  for (const auto& item : releases) {
    const releaseInfo& release = {
      item["version"].as<string>(),
      item["date"].as<string>(),
      item["description"].as<string>(),
    };
    pack.releases.push_back(release);
  }
}

void PackGen::ParseManifestRequirements(const YAML::Node node, packInfo& pack) {
  YAML::Node requirements = node["requirements"];
  if (requirements.IsDefined()) {
    auto parseRequirements = [](YAML::Node inputNode, list<map<string, string>>& outputList) {
      for (const auto& requirement : inputNode) {
        for (const auto& item : requirement) {
          map<string, string> attributes;
          for (const auto& attr : item.second) {
            attributes.insert({ attr.first.as<string>(), attr.second.as<string>() });
          }
          outputList.push_back({ attributes });
        }
      }
    };
    parseRequirements(requirements["packages"], pack.requirements.packages);
    parseRequirements(requirements["compilers"], pack.requirements.compilers);
    parseRequirements(requirements["languages"], pack.requirements.languages);
  }
}

void PackGen::ParseManifestTaxonomy(const YAML::Node node, packInfo& pack) {
  YAML::Node taxonomy = node["taxonomy"];
  for (const auto& item : taxonomy) {
    taxonomyInfo taxonomyInfo;
    YAML::Node attributes = item["attributes"];
    for (const auto& attr : attributes) {
      taxonomyInfo.attributes.insert({ attr.first.as<string>(), attr.second.as<string>() });
    }
    taxonomyInfo.description = item["description"].as<string>();
    pack.taxonomy.push_back(taxonomyInfo);
  }
}

void PackGen::ParseManifestApis(const YAML::Node node, packInfo& pack) {
  YAML::Node apis = node["apis"];
  for (const auto& item : apis) {
    const string& name = item["name"].as<string>();
    pack.apis.push_back(name);
    YAML::Node attributes = item["attributes"];
    for (const auto& attr : attributes) {
      m_apis[name].attributes.insert({ attr.first.as<string>(), attr.second.as<string>() });
    }
    m_apis[name].description = item["description"].as<string>();
    YAML::Node files = item["files"];
    for (const auto& file : files) {
      // File name
      const string& fileName = file["name"].as<string>();
      // Attributes
      map<string, string> fileAttributes;
      YAML::Node attributes = file["attributes"];
      for (const auto& attr : attributes) {
        fileAttributes.insert({ attr.first.as<string>(), attr.second.as<string>() });
      }
      m_apis[name].files.push_back({ fileName, fileAttributes });
    }
  }
}

bool PackGen::ParseManifestComponents(const YAML::Node node, packInfo& pack) {
  YAML::Node components = node["components"];
  for (const auto& item : components) {
    const string& name = item["name"].as<string>();

    if (m_components.find(name) != m_components.end()) {
      cerr << "packgen warning: component '" << name << "' has been defined multiple times" << endl;
      continue;
    }
    pack.components.push_back(name);

    // Target name(s)
    YAML::Node target = item["target"];
    if (target && target.IsScalar()) {
      m_components[name].target.push_back(target.as<string>());
    } else if (target && target.IsSequence()) {
      for (const auto& tgt : target) {
        m_components[name].target.push_back(tgt.as<string>());
      }
    } else {
      cerr << "packgen error: target field is mandatory for every component!" << endl;
      return false;
    }

    // Optional build name(s)
    YAML::Node build = item["build"];
    if (build && build.IsScalar()) {
      m_components[name].builds.names.push_back(build.as<string>());
    } else if (build && build.IsSequence()) {
      for (const auto& bld : build) {
        m_components[name].builds.names.push_back(bld.as<string>());
      }
    } else {
      m_components[name].builds.names.push_back(m_buildOptions.front().name);
    }

    // Optional "operation" field
    if (item["operation"]) {
      m_components[name].builds.operation = item["operation"].as<string>();
    }

    // Component attributes
    YAML::Node attributes = item["attributes"];
    for (const auto& attr : attributes) {
      m_components[name].attributes.insert({ attr.first.as<string>(), attr.second.as<string>() });
    }
    m_components[name].description = item["description"].as<string>();

    // Component dependencies
    YAML::Node dependencies = item["dependencies"];
    if (dependencies && dependencies.IsScalar()) {
      m_components[name].dependency.push_back(dependencies.as<string>());
    }
    else {
      for (const auto& dep : dependencies) {
        m_components[name].dependency.push_back(dep.as<string>());
      }
    }

    // Component external conditions
    YAML::Node conditions = item["conditions"];
    for (const auto& cond : conditions) {
      for (const auto& rule : cond) {
        // Rule (require, accept or deny)
        const string& conditionRule = rule.first.as<string>();
        // Attributes
        map<string, string> conditionAttributes;
        for (const auto& attr : rule.second) {
          conditionAttributes.insert({ attr.first.as<string>(), attr.second.as<string>() });
        }
        m_components[name].condition.push_back({ conditionRule, conditionAttributes });
      }
    }

    // Files
    YAML::Node files = item["files"];
    for (const auto& file : files) {
      // File name
      const string& fileName = file["name"].as<string>();
      // Attributes
      map<string, string> fileAttributes;
      YAML::Node attributes = file["attributes"];
      for (const auto& attr : attributes) {
        fileAttributes.insert({ attr.first.as<string>(), attr.second.as<string>() });
      }
      // File conditions
      list<conditionInfo> conditionInfo;
      YAML::Node conditions = file["conditions"];
      for (const auto& cond : conditions) {
        for (const auto& rule : cond) {
          // Rule (require, accept or deny)
          const string& conditionRule = rule.first.as<string>();
          // Attributes
          map<string, string> conditionAttributes;
          for (const auto& attr : rule.second) {
            conditionAttributes.insert({ attr.first.as<string>(), attr.second.as<string>() });
          }
          conditionInfo.push_back({ conditionRule, conditionAttributes });
        }
      }
      m_components[name].files.push_back({ fileName, fileAttributes, conditionInfo });
    }

    // Extensions
    YAML::Node extensions = item["extensions"];
    for (const auto& ext : extensions) {
      m_extensions[name].push_back(ext.as<string>());
    }
  }

  return true;
}

void PackGen::AddComponentBuildInfo(const string& componentName, buildInfo& reference) {
  m_components[componentName].build.src.insert(reference.src.begin(), reference.src.end());
  m_components[componentName].build.inc.insert(reference.inc.begin(), reference.inc.end());
  m_components[componentName].build.def.insert(reference.def.begin(), reference.def.end());
}

void PackGen::InsertBuildInfo(buildInfo& build, const string& targetName, const string& buildName) {
  build.src.insert(m_target[targetName][buildName].build.src.begin(), m_target[targetName][buildName].build.src.end());
  build.inc.insert(m_target[targetName][buildName].build.inc.begin(), m_target[targetName][buildName].build.inc.end());
  build.def.insert(m_target[targetName][buildName].build.def.begin(), m_target[targetName][buildName].build.def.end());

  // Recursively insert build info from dependencies
  for (const auto& dependencyName : m_target[targetName][buildName].dependency) {
    InsertBuildInfo(build, dependencyName, buildName);
  }
}

void PackGen::GetBuildInfoIntersection(buildInfo& reference, buildInfo& actual, buildInfo& intersect) {
  set_intersection(reference.src.begin(), reference.src.end(), actual.src.begin(), actual.src.end(), inserter(intersect.src, intersect.src.begin()));
  set_intersection(reference.inc.begin(), reference.inc.end(), actual.inc.begin(), actual.inc.end(), inserter(intersect.inc, intersect.inc.begin()));
  set_intersection(reference.def.begin(), reference.def.end(), actual.def.begin(), actual.def.end(), inserter(intersect.def, intersect.def.begin()));
}

void PackGen::GetBuildInfoDifference(buildInfo& reference, buildInfo& actual, buildInfo& difference) {
  set_difference(reference.src.begin(), reference.src.end(), actual.src.begin(), actual.src.end(), inserter(difference.src, difference.src.begin()));
  set_difference(reference.inc.begin(), reference.inc.end(), actual.inc.begin(), actual.inc.end(), inserter(difference.inc, difference.inc.begin()));
  set_difference(reference.def.begin(), reference.def.end(), actual.def.begin(), actual.def.end(), inserter(difference.def, difference.def.begin()));
}

void PackGen::GetBuildInfo(buildInfo& reference, const list<string>& targetNames, const list<string>& buildNames, const string& operation) {
  auto buildNameIterator = buildNames.begin();
  for (const auto& targetName : targetNames) {
    InsertBuildInfo(reference, targetName, *buildNameIterator);
  }
  if (operation.empty()) {
    return;
  }
  bool intersection = (operation == "intersection");
  bool difference = (operation == "difference");

  while (++buildNameIterator != buildNames.end()) {
    buildInfo actual, result;
    for (const auto& targetName : targetNames) {
      InsertBuildInfo(actual, targetName, *buildNameIterator);
    }
    if (intersection) {
      GetBuildInfoIntersection(reference, actual, result);
    } else if (difference) {
      GetBuildInfoDifference(reference, actual, result);
    }
    reference.src = result.src;
    reference.inc = result.inc;
    reference.def = result.def;
  }
}

bool PackGen::CreateComponents(void) {

  // Set full build info of every component
  for (const auto& component : m_components) {
    const string& componentName = component.first;
    const auto& operation = component.second.builds.operation;
    buildInfo componentBuildInfo;
    GetBuildInfo(componentBuildInfo, component.second.target, component.second.builds.names, operation);
    AddComponentBuildInfo(componentName, componentBuildInfo);
  }

  // Filter out component dependencies
  for (const auto& component : m_components) {
    const string& name = component.first;
    for (const auto& dependency : component.second.dependency) {
      // Filter out component
      if (m_components.find(dependency) != m_components.end()) {
        for (const auto& src : m_components[dependency].build.src) {
          m_components[name].build.src.erase(src);
        }
        for (const auto& inc : m_components[dependency].build.inc) {
          m_components[name].build.inc.erase(inc);
        }
        for (const auto& def : m_components[dependency].build.def) {
          m_components[name].build.def.erase(def);
        }
      }
      // Filter out target
      else if (m_target.find(dependency) != m_target.end()) {
        for (const auto& buildName : component.second.builds.names) {
          for (const auto& src : m_target[dependency][buildName].build.src) {
            m_components[name].build.src.erase(src);
          }
          for (const auto& inc : m_target[dependency][buildName].build.inc) {
            m_components[name].build.inc.erase(inc);
          }
          for (const auto& def : m_target[dependency][buildName].build.def) {
            m_components[name].build.def.erase(def);
          }
        }
      }
    }
  }

  // Verbose mode: print components build info
  if (m_verbose) {
    for (const auto& component : m_components) {
      cout << endl << "COMPONENT: " << component.first << endl;
      for (const auto& src : component.second.build.src) {
        cout << "src: " << src << endl;
      }
      for (const auto& inc : component.second.build.inc) {
        cout << "inc: " << inc << endl;
      }
      for (const auto& def : component.second.build.def) {
        cout << "def: " << def << endl;
      }
    }
  }

  return true;
}

bool PackGen::ParseTarget(void) {

  error_code ec;
  fs::current_path(m_repoRoot, ec);

  // Iterate over build options
  for (const auto& build : m_buildOptions) {

    const string& buildRoot = m_outputRoot + "/" + build.name;
    const string& replyDir = buildRoot + "/.cmake/api/v1/reply";

    for (const auto& p : fs::recursive_directory_iterator(replyDir, ec)) {
      const string& file = p.path().stem().generic_string();
      if (file.compare(0, 6, "target") == 0) {
        try {
          YAML::Node target = YAML::LoadFile(p.path().generic_string());

          const string& name = target["name"].as<string>();

          YAML::Node sources = target["sources"];
          for (const auto& item : sources) {
            string src = item["path"].as<string>();
            fs::path canonical = fs::canonical(src, ec);
            if (canonical.empty()) {
              cerr << "packgen warning: file '" << src << "' was not found" << endl;
              continue;
            }
            src = canonical.generic_string();
            src.erase(0, m_repoRoot.length() + 1);
            m_target[name][build.name].build.src.insert(src);
          }

          YAML::Node includes = target["compileGroups"][0]["includes"];
          for (const auto& item : includes) {
            string inc = item["path"].as<string>();
            fs::path canonical = fs::canonical(inc, ec);
            if (canonical.empty()) {
              cerr << "packgen warning: directory '" << inc << "' was not found" << endl;
              continue;
            }
            inc = canonical.generic_string();
            inc.erase(0, m_repoRoot.length() + 1);
            m_target[name][build.name].build.inc.insert(inc);
          }

          YAML::Node defines = target["compileGroups"][0]["defines"];
          for (const auto& item : defines) {
            const string& def = item["define"].as<string>();
            m_target[name][build.name].build.def.insert(def);
          }

          YAML::Node dependencies = target["dependencies"];
          for (const auto& item : dependencies) {
            string dep = item["id"].as<string>();
            dep = dep.substr(0, dep.find("::"));
            m_target[name][build.name].dependency.insert(dep);
          }

        }
        catch (YAML::Exception& e) {
          cerr << "packgen warning: target parsing" << endl << e.what() << endl;
        }
      }
    }
  }

  // Verbose mode: print cmake targets build info
  if (m_verbose) {
    for (const auto& target : m_target) {
      for (const auto& build : m_buildOptions) {
        cout << endl << "TARGET: " << target.first << endl << "BUILD: " << build.name << endl;
        auto buildMap = target.second;
        for (const auto& src : buildMap[build.name].build.src) {
          cout << "src: " << src << endl;
        }
        for (const auto& inc : buildMap[build.name].build.inc) {
          cout << "inc: " << inc << endl;
        }
        for (const auto& def : buildMap[build.name].build.def) {
          cout << "def: " << def << endl;
        }
        for (const auto& dep : buildMap[build.name].dependency) {
          cout << "dep: " << dep << endl;
        }
      }
    }
  }

  return true;
}

bool PackGen::CreatePack() {

  // Iterate over packs
  for (auto&& pack : m_pack) {

    // Set output path for generated pack
    pack.outputDir = m_outputRoot + "/" + pack.vendor + "." + pack.name + "." + pack.releases.begin()->version;

    // Clean output
    RteFsUtils::RemoveDir(pack.outputDir);

    // Copy license
    error_code ec;
    fs::create_directories(pack.outputDir, ec);
    fs::copy_file(m_repoRoot + "/" + pack.license, pack.outputDir + "/" + pack.license, fs::copy_options::overwrite_existing, ec);

    // Root
    m_pdscTree = new XMLTreeSlim();
    XMLTreeElement* rootElement;
    rootElement = m_pdscTree->CreateElement("package");

    // Pack Info
    CreatePackInfo(rootElement, pack);

    // Requirements
    if ((!pack.requirements.packages.empty()) || (!pack.requirements.compilers.empty()) || (!pack.requirements.languages.empty())) {
      CreatePackRequirements(rootElement, pack);
    }

    // Releases
    if (!pack.releases.empty()) {
      CreatePackReleases(rootElement, pack);
    }

    // APIs
    if (!pack.apis.empty()) {
      CreatePackApis(rootElement, pack);
    }

    // Taxonomy
    if (!pack.taxonomy.empty()) {
      CreatePackTaxonomy(rootElement, pack);
    }

    // Components and conditions
    if (!pack.components.empty()) {
      CreatePackComponentsAndConditions(rootElement, pack);
    }

    // Format Xml content
    XmlFormatter xmlFormatter(m_pdscTree, SCHEMA_FILE, SCHEMA_VERSION);
    const string& xmlContent = xmlFormatter.GetContent();

    // Save file
    const string& file = pack.outputDir + "/" + pack.vendor + "." + pack.name + ".pdsc";
    ofstream xmlFile(file);
    if (!xmlFile) {
      return false;
    }

    xmlFile << xmlContent;
    xmlFile << std::endl;
    xmlFile.close();
  }

  return true;
}

void PackGen::SetAttribute(XMLTreeElement* element, const string& name, const string& value) {
  if (!value.empty()) {
    element->AddAttribute(name, value);
  }
}

void PackGen::CreatePackInfo(XMLTreeElement* rootElement, packInfo& pack) {
  XMLTreeElement* nameElement = rootElement->CreateElement("name");
  XMLTreeElement* descriptionElement = rootElement->CreateElement("description");
  XMLTreeElement* vendorElement = rootElement->CreateElement("vendor");
  XMLTreeElement* licenseElement = rootElement->CreateElement("license");
  XMLTreeElement* urlElement = rootElement->CreateElement("url");
  nameElement->SetText(pack.name);
  descriptionElement->SetText(pack.description);
  vendorElement->SetText(pack.vendor);
  licenseElement->SetText(pack.license);
  urlElement->SetText(pack.url);
}

void PackGen::CreatePackRequirements(XMLTreeElement* rootElement, packInfo& pack) {
  XMLTreeElement* requirementsElement = rootElement->CreateElement("requirements");
  auto setRequirements = [](list<map<string, string>>& requirementsList, XMLTreeElement* parentElement, const string& groupTag, const string& itemTag) {
    if (!requirementsList.empty()) {
      XMLTreeElement* packagesElement = parentElement->CreateElement(groupTag);
      for (const auto& attributes : requirementsList) {
        XMLTreeElement* packageElement = packagesElement->CreateElement(itemTag);
        for (const auto& attribute : attributes) {
          SetAttribute(packageElement, attribute.first, attribute.second);
        }
      }
    }
  };
  setRequirements(pack.requirements.packages, requirementsElement, "packages", "package");
  setRequirements(pack.requirements.compilers, requirementsElement, "compilers", "compiler");
  setRequirements(pack.requirements.languages, requirementsElement, "languages", "language");
}

void PackGen::CreatePackReleases(XMLTreeElement* rootElement, packInfo& pack) {
  XMLTreeElement* releasesElement = rootElement->CreateElement("releases");
  for (const auto& release : pack.releases) {
    XMLTreeElement* releaseElement = releasesElement->CreateElement("release");
    releaseElement->AddAttribute("version", release.version);
    releaseElement->AddAttribute("date", release.date);
    releaseElement->SetText(release.description);
  }
}

void PackGen::CreatePackApis(XMLTreeElement* rootElement, packInfo& pack) {
  XMLTreeElement* apisElement = rootElement->CreateElement("apis");
  for (const auto& apiName : pack.apis) {
    auto apiInfo = m_apis[apiName];
    // Add API
    XMLTreeElement* apiElement = apisElement->CreateElement("api");
    for (const auto& attribute : apiInfo.attributes) {
      SetAttribute(apiElement, attribute.first, attribute.second);
    }
    if (!apiInfo.description.empty()) {
      XMLTreeElement* descriptionElement = apiElement->CreateElement("description");
      descriptionElement->SetText(apiInfo.description);
    }
    if (!apiInfo.files.empty()) {
      XMLTreeElement* filesElement = apiElement->CreateElement("files");
      for (const auto& file : apiInfo.files) {
        XMLTreeElement* fileElement = filesElement->CreateElement("file");
        SetAttribute(fileElement, "name", file.name);
        for (const auto& attribute : file.attributes) {
          SetAttribute(fileElement, attribute.first, attribute.second);
        }
        const string dst = pack.outputDir + "/" + file.name;
        CopyItem(m_repoRoot + "/" + file.name, dst, m_extensions[apiName]);
      }
    }
  }
}

void PackGen::CreatePackTaxonomy(XMLTreeElement* rootElement, packInfo& pack) {
  XMLTreeElement* taxonomyElement = rootElement->CreateElement("taxonomy");
  for (const auto& taxonomy : pack.taxonomy) {
    XMLTreeElement* descriptionElement = taxonomyElement->CreateElement("description");
    for (const auto& attribute : taxonomy.attributes) {
      SetAttribute(descriptionElement, attribute.first, attribute.second);
    }
    descriptionElement->SetText(taxonomy.description);
  }
}

void PackGen::CreatePackComponentsAndConditions(XMLTreeElement* rootElement, packInfo& pack) {
  XMLTreeElement* conditionsElement = rootElement->CreateElement("conditions");
  XMLTreeElement* componentsElement = rootElement->CreateElement("components");

  // Iterate over components
  for (const auto& componentName : pack.components) {
    auto componentInfo = m_components[componentName];

    // Add Conditions
    if (!componentInfo.dependency.empty() || !componentInfo.condition.empty()) {
      XMLTreeElement* conditionElement = conditionsElement->CreateElement("condition");
      SetAttribute(conditionElement, "id", componentName + " Condition");
      for (const auto& dependency : componentInfo.dependency) {
        // Add automatic conditions on dependencies
        if (m_components.find(dependency) != m_components.end()) {
          XMLTreeElement* requireElement = conditionElement->CreateElement("require");
          for (const auto& attribute : m_components[dependency].attributes) {
            if (attribute.first == "Cversion") {
              continue;
            }
            SetAttribute(requireElement, attribute.first, attribute.second);
          }
        }
      }
      for (const auto& condition : componentInfo.condition) {
        // Add component conditions described in manifest
        XMLTreeElement* ruleElement = conditionElement->CreateElement(condition.rule);
        for (const auto& attribute : condition.attributes) {
          SetAttribute(ruleElement, attribute.first, attribute.second);
        }
      }
    }

    // Add Component
    XMLTreeElement* componentElement = componentsElement->CreateElement("component");
    for (const auto& attribute : componentInfo.attributes) {
      SetAttribute(componentElement, attribute.first, attribute.second);
    }
    if (!componentInfo.description.empty()) {
      XMLTreeElement* descriptionElement = componentElement->CreateElement("description");
      descriptionElement->SetText(componentInfo.description);
    }
    if (!componentInfo.dependency.empty() || !componentInfo.condition.empty()) {
      componentElement->AddAttribute("condition", componentName + " Condition");
    }

    // Add files
    if (!componentInfo.build.src.empty() || !componentInfo.build.inc.empty() || !componentInfo.files.empty()) {
      XMLTreeElement* filesElement = componentElement->CreateElement("files");
      // Source files from CMake targets
      for (const auto& src : componentInfo.build.src) {
        XMLTreeElement* fileElement = filesElement->CreateElement("file");
        fileElement->AddAttribute("category", "source");
        fileElement->AddAttribute("name", src);
        const string dst = pack.outputDir + "/" + src;
        CopyItem(m_repoRoot + "/" + src, dst, m_extensions[componentName]);
      }
      // Include paths from CMake targets
      for (const auto& inc : componentInfo.build.inc) {
        XMLTreeElement* fileElement = filesElement->CreateElement("file");
        fileElement->AddAttribute("category", "include");
        fileElement->AddAttribute("name", inc + "/");
        const string dst = pack.outputDir + "/" + inc;
        CopyItem(m_repoRoot + "/" + inc, dst, m_extensions[componentName]);
      }
      // Other files described in manifest
      for (const auto& file : componentInfo.files) {
        XMLTreeElement* fileElement = filesElement->CreateElement("file");
        SetAttribute(fileElement, "name", file.name);
        for (const auto& attribute : file.attributes) {
          SetAttribute(fileElement, attribute.first, attribute.second);
        }
        const string dst = pack.outputDir + "/" + file.name;
        CopyItem(m_repoRoot + "/" + file.name, dst, m_extensions[componentName]);

        // Add file conditions described in manifest
        if (!file.conditions.empty()) {
          XMLTreeElement* conditionElement = conditionsElement->CreateElement("condition");
          SetAttribute(conditionElement, "id", file.name + " Condition");
          for (const auto& condition : file.conditions) {
            XMLTreeElement* ruleElement = conditionElement->CreateElement(condition.rule);
            for (const auto& attribute : condition.attributes) {
              SetAttribute(ruleElement, attribute.first, attribute.second);
            }
          }
          fileElement->AddAttribute("condition", file.name + " Condition");
        }
      }
    }

    // Add defines
    if (!componentInfo.build.def.empty()) {
      string defines;
      for (const auto& def : componentInfo.build.def) {
        size_t pos = def.find('=');
        defines += "\n#define " + (pos == string::npos ? def : string(def).replace(pos, 1, " "));
      }
      XMLTreeElement* definesElement = componentElement->CreateElement("Pre_Include_Global_h");
      definesElement->SetText(defines);
    }
  }
}

bool PackGen::CheckPack(void) {
  PackGen::Result result;
  error_code ec;
  auto workingDir = fs::current_path(ec);

  // Iterate over packs
  for (const auto& pack : m_pack) {

    // Change working dir
    fs::current_path(pack.outputDir, ec);

    // Packchk
    result = ExecCommand("PackChk \"" + pack.vendor + "." + pack.name + ".pdsc\"");
    if (result.second) {
      cerr << "packgen error: packchk failed\n" << result.first << endl;
      return false;
    }
  }

  // Restore working dir
  fs::current_path(workingDir, ec);
  return true;
}

bool PackGen::CompressPack(void) {
  PackGen::Result result;
  error_code ec;
  auto workingDir = fs::current_path(ec);

  // Iterate over packs
  for (const auto& pack : m_pack) {

    // Change working dir
    fs::current_path(pack.outputDir, ec);

    // 7zip
    result = ExecCommand("7z a \"" + pack.vendor + "." + pack.name + "." + pack.releases.begin()->version + ".pack\" -tzip");
    if (result.second) {
      cerr << "packgen error: 7zip failed\n" << result.first << endl;
      return false;
    }
  }

  // Restore working dir
  fs::current_path(workingDir, ec);
  return true;
}

YAML::Emitter& operator << (YAML::Emitter& out, const queryRequests& req) {
  out << YAML::BeginMap << YAML::Key << "kind" << YAML::Value << req.kind << YAML::Key << "version" << YAML::Value;
  out << YAML::BeginMap << YAML::Key << "major" << YAML::Value << req.major << YAML::Key << "minor" << YAML::Value << req.minor;
  out << YAML::EndMap;
  out << YAML::EndMap;
  return out;
}

bool PackGen::CreateQuery() {
  static const queryRequests codemodel = {"codemodel", 2, 0};
  static const queryRequests cache = {"cache", 2, 0};
  static const map<string, list<queryRequests>> requests = {{"requests", {codemodel, cache} }};

  YAML::Emitter query;
  query << YAML::DoubleQuoted << YAML::Flow << requests;

  // Iterate over build options
  for (const auto& build : m_buildOptions) {

    // Create query file
    const string& buildRoot = m_outputRoot + "/" + build.name;
    const string& queryDir = buildRoot + "/.cmake/api/v1/query/client-cmsis";
    const string& replyDir = buildRoot + "/.cmake/api/v1/reply";

    error_code ec;
    if (fs::exists(replyDir, ec) && !m_regenerate) {
       continue;
    } else {
       RteFsUtils::RemoveDir(buildRoot);
    }
    fs::create_directories(queryDir, ec);

    ofstream fileStream(queryDir + "/query.json");
    fileStream << query.c_str();
    fileStream << flush;
    fileStream.close();

    // Run CMake
    const string cmd = build.options + " -S \"" + m_repoRoot + "\" -B \"" + buildRoot + "\"";
    PackGen::Result result = ExecCommand(cmd);
    if (result.second) {
      cerr << "packgen error: CMake failed\n" << result.first << endl;
      return false;
    }
  }

  return true;
}

const PackGen::Result PackGen::ExecCommand(const string& cmd) {
  array<char, 128> buffer;
  string result;
  int ret_code = -1;
  std::function<int(FILE*)> close = _pclose;
  std::function<FILE*(const char*, const char*)> open = _popen;

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

bool PackGen::CopyItem(const string& src, const string& dst, list<string>& ext) {
  //Copy file or directory recursively filtering extensions
  error_code ec;
  fs::path srcPath = fs::path(src);
  fs::path dstPath = fs::path(dst);
  if (fs::is_regular_file(srcPath)) {
    // Copy file
    fs::create_directories(dstPath.parent_path(), ec);
    fs::copy_file(srcPath, dstPath, fs::copy_options::overwrite_existing, ec);
  } else {
    // Copy directory recursively filtering extensions
    for (const auto& p : fs::recursive_directory_iterator(srcPath, ec)) {
      ext = ext.empty() ? list<string>{".h", ".hpp"} : ext;
      if (find(ext.begin(), ext.end(), p.path().extension()) != ext.end()) {
        string filename = dstPath.generic_string() + p.path().generic_string().substr(srcPath.generic_string().length(), string::npos);
        fs::create_directories(fs::path(filename).parent_path(), ec);
        fs::copy_file(p.path(), fs::path(filename), fs::copy_options::overwrite_existing, ec);
      }
    }
  }
  return true;
}
