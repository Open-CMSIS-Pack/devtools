/*
 * Copyright (c) 2020-2024 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "CbuildLayer.h"

#include "Cbuild.h"
#include "CbuildKernel.h"
#include "CbuildUtils.h"

#include "ErrLog.h"
#include "RteUtils.h"
#include "RteFsUtils.h"
#include "RteModel.h"
#include "XMLTreeSlim.h"
#include "XmlFormatter.h"
#include "ProductInfo.h"

#include <string>
#include <iostream>
#include <sstream>
#include <iterator>
#include <fstream>
#include <list>
#include <set>
#include <map>
#include <algorithm>

using namespace std;

#define PDEXT ".cprj"             // Project description extension
#define CLEXT ".clayer"           // Layer extension
#define EOL "\n"                  // End of line

CbuildLayer::CbuildLayer(void) {
  // Reserved
}

CbuildLayer::~CbuildLayer(void) {
  if (nullptr != m_cprjTree) {
    delete m_cprjTree;
  }

  if (nullptr != m_cprjTree) {
    delete m_cprj;
  }

  for (auto it = m_layerTree.begin(); it != m_layerTree.end(); ++it) {
    delete it->second;
  }

  for (auto it = m_layer.begin(); it != m_layer.end(); ++it) {
    delete it->second;
  }
}

XMLTree* CbuildLayer::GetTree(void) const {
  return m_cprjTree;
};

xml_elements* CbuildLayer::GetElements(void) const {
  return m_cprj;
};

string CbuildLayer::GetTimestamp(void) const {
  return m_timestamp;
};

string CbuildLayer::GetTool(void) const {
  return m_tool;
};

bool CbuildLayer::Extract(const CbuildLayerArgs& args) {
  // Init Cprj
  if (!InitXml(args.file)) {
    return false;
  }

  // Check if the project has layers
  if (!m_cprj->layers) {
    LogMsg("M215");
    return false;
  }

  // Init Header Info
  InitHeaderInfo(args.file);

  // Construct RTE Model
  if (!ConstructModel (m_cprj->compilers, args)) {
    return false;
  }

  set<string> cprjPackIDList;
  const auto cprjPacks = m_cprj->packages->GetChildren();
  for (auto cprjPack : cprjPacks) {
    cprjPackIDList.insert(RtePackage::GetPackageIDfromAttributes(*cprjPack));
  }

  // Set absolute output path
  const string& outputPath = RteFsUtils::AbsolutePath(RteUtils::BackSlashesToSlashes(args.output)).generic_string();

  // Iterate over list of layers
  const auto layers = m_cprj->layers->GetChildren();
  for (auto layer : layers) {
    const string& layerName = layer->GetAttribute("name");
    if (layerName.empty()) {
      // Missing <layer name> element
      LogMsg("M609", VAL("NAME", "layer name"));
      return false;
    }

    if (!args.layerFiles.empty() && (find(args.layerFiles.begin(), args.layerFiles.end(), layerName) == args.layerFiles.end())) {
      continue;
    }

    // Root
    XMLTreeSlim* xmlTreeLayer = new XMLTreeSlim();
    XMLTreeElement* rootElement;
    rootElement = xmlTreeLayer->CreateElement("cprj");

    // Created
    XMLTreeElement* createdElement = rootElement->CreateElement("created");
    createdElement->AddAttribute("tool", m_tool);
    createdElement->AddAttribute("timestamp", string(m_timestamp));

    // Created::Used
    XMLTreeElement* usedElement = createdElement->CreateElement("used");
    usedElement->AddAttribute("file", m_cprjFile);
    usedElement->AddAttribute("path", m_cprjPath);
    usedElement->AddAttribute("timestamp", m_cprj->created->GetAttribute("timestamp"));

    // Info
    XMLTreeElement* infoElement = rootElement->CreateElement("info");
    infoElement->AddAttribute("isLayer", "true");
    XMLTreeElement* nameElement = infoElement->CreateElement("name");
    nameElement->SetText(layerName);
    for (auto child : layer->GetChildren()) {
      CopyElement(infoElement, child);
    }

    // Layers
    XMLTreeElement* layersElement = rootElement->CreateElement("layers");
    CopyElement(layersElement, layer);

    // Packages
    XMLTreeElement* packagesElement = rootElement->CreateElement("packages");
    for (auto layerPack : m_layerPackages[layerName]) {
      const bool fixedVersion = cprjPackIDList.find(layerPack) != cprjPackIDList.end();
      if (!fixedVersion) {
        layerPack = RtePackage::CommonIdFromId(layerPack);
      }
      for (auto cprjPack : cprjPacks) {
        string cprjPackID = RtePackage::GetPackageIDfromAttributes(*cprjPack, fixedVersion);
        if (layerPack == cprjPackID) {
          CopyElement(packagesElement, cprjPack);
          break;
        }
      }
    }

    // Compilers
    if (m_cprj->compilers) {
      CopyElement(rootElement, m_cprj->compilers);
    }

    // Target
    if (layer->GetAttributeAsBool("hasTarget")) {
      if (!m_cprj->target) {
        // Missing <target> element
        LogMsg("M609", VAL("NAME", "target"));
        return false;
      }
      CopyElement(rootElement, m_cprj->target);
    }

    // Components
    CopyMatchedChildren(m_cprj->components, rootElement, layerName);

    // Files
    CopyMatchedChildren(m_cprj->files, rootElement, layerName);

    // Write Xml file
    const string& layerPath = outputPath + "/" + layerName;
    const string& layerFilename = layerPath + "/" + layerName + CLEXT;
    error_code ec;
    fs::create_directories(layerPath, ec);
    if (ec) {
      LogMsg("M211", PATH(layerPath));
      return false;
    }
    if (!WriteXmlFile(layerFilename, xmlTreeLayer)) {
      return false;
    }

    // Find infrastructure files
    for (auto& p : fs::recursive_directory_iterator(m_cprjPath, ec)) {
      const string& file = p.path().stem().generic_string();
      if (file == "layer."+layerName) {
        m_layerFiles[layerName].insert(p.path().generic_string().substr(m_cprjPath.length(), string::npos));
      }
    }

    // Copy files
    for (auto file : m_layerFiles[layerName]) {
      const string& origin = m_cprjPath + "/" + file;
      const string& destination = layerPath + "/" + file;
      // Create intermediate directories
      const string& dir = fs::is_directory(origin.c_str(), ec) ?
                   fs::path(destination).generic_string() :
                   fs::path(destination).remove_filename().generic_string();
      fs::create_directories(dir, ec);
      if (ec) {
        LogMsg("M211", PATH(dir));
        return false;
      }
      if (fs::is_directory(origin.c_str(), ec)) {
        // Directory: copy all files recursively
        fs::copy(origin, destination, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
      } else {
        // Single file
        fs::copy_file(origin, destination, fs::copy_options::overwrite_existing, ec);
      }
      if (ec) {
        LogMsg("M208", VAL("ORIG", origin), VAL("DEST", destination));
        return false;
      }
    }
  }

  return true;
}

bool CbuildLayer::Compose(const CbuildLayerArgs& args) {
  // Initialize header info
  InitHeaderInfo(args.file);

  if (nullptr != m_cprjTree) {
    delete m_cprjTree;
  }

  // Root
  m_cprjTree = new XMLTreeSlim();
  XMLTreeElement* rootElement;
  rootElement = m_cprjTree->CreateElement("cprj");

  // Created
  XMLTreeElement* createdElement = rootElement->CreateElement("created");
  createdElement->AddAttribute("tool", m_tool);
  createdElement->AddAttribute("timestamp", string(m_timestamp));

  // Info
  set<string> categoryList, keywordsList, licenseList;
  XMLTreeElement* infoElement = rootElement->CreateElement("info");
  infoElement->AddAttribute("isLayer", "false");

  // Create first level elements
  XMLTreeElement* layersElement = rootElement->CreateElement("layers");
  XMLTreeElement* packagesElement = rootElement->CreateElement("packages");
  XMLTreeElement* compilersElement = rootElement->CreateElement("compilers");
  XMLTreeElement* targetElement = rootElement->CreateElement("target");
  XMLTreeElement* componentsElement = rootElement->CreateElement("components");
  XMLTreeElement* filesElement = rootElement->CreateElement("files");

  // Parse layer files
  list<string> layerNameList;
  for (auto layerFile : args.layerFiles) {
    string layerName;
    if (!InitXml(layerFile, &layerName)) {
      return false;
    }
    layerNameList.push_back(layerName);
  }

  // Iterate over list of layers
  for (auto it : m_layer) {
    xml_elements* element = it.second;

    // Read info fields
    GetArgsFromChild(element->layer, "category", categoryList);
    GetArgsFromChild(element->layer, "keywords", keywordsList);
    GetArgsFromChild(element->layer, "license", licenseList);

    // Layers
    CopyElement(layersElement, element->layer);

    // Packages
    const auto packages = element->packages->GetChildren();
    for (auto package : packages) {
      // Copy avoiding duplicates
      const auto cprjPackages = packagesElement->GetChildren();
      bool duplicated = false;
      for (auto cprjPackage : cprjPackages){
        if (cprjPackage->GetAttributes() == package->GetAttributes()) {
          // package is already in the cprj packages list
          duplicated = true;
          break;
        }
      }
      if (!duplicated) {
        CopyElement(packagesElement, package);
      }
    }

    // Compilers
    if (element->compilers) {
      const auto compilers = element->compilers->GetChildren();
      for (auto compiler : compilers) {
        // Copy avoiding duplicates
        const auto cprjCompilers = compilersElement->GetChildren();
        bool duplicated = false;
        for (auto cprjCompiler : cprjCompilers) {
          if (cprjCompiler->GetAttributes() == compiler->GetAttributes()) {
            // compiler is already in the cprj compilers list
            duplicated = true;
            break;
          }
        }
        if (!duplicated) {
          CopyElement(compilersElement, compiler);
        }
      }
    }

    // Target
    if (element->layer->GetAttributeAsBool("hasTarget")) {
      if (!targetElement->HasChildren()) {
        CopyElement(rootElement, element->target, false);
      } else {
        // Warning (multiple layers have target elements)
        LogMsg("M631");
      }
    }

    // Components
    if (element->components) {
      const auto components = element->components->GetChildren();
      for (auto component : components) {
        CopyElement(componentsElement, component);
      }
    }

    // Files
    if (element->files) {
      const auto children = element->files->GetChildren();
      for (auto child : children) {
        CopyNestedGroups(filesElement, child);
      }
    }
  }

  // Check target element
  if (!targetElement->HasChildren()) {
    // Warning (no target element was found)
    LogMsg("M631");
    rootElement->RemoveChild(targetElement, true);
  }

  // Add info fields
  if (!args.name.empty()) {
    infoElement->CreateElement("name")->SetText(args.name);
  }
  XMLTreeElement* description = infoElement->CreateElement("description");
  if (args.description.empty()) {
    description->SetText("Automatically generated project");
  } else {
    description->SetText(args.description);
  }
  const string& categoryText = MergeArgs(categoryList);
  if (!categoryText.empty()) {
    infoElement->CreateElement("category")->SetText(categoryText);
  }
  const string& keywordsText = MergeArgs(keywordsList);
  if (!keywordsText.empty()) {
    infoElement->CreateElement("keywords")->SetText(keywordsText);
  }
  const string& licenseText = MergeArgs(licenseList);
  if (!licenseText.empty()) {
    infoElement->CreateElement("license")->SetText(licenseText);
  }

  // Copy files recursively
  for (auto layerFile : args.layerFiles) {
    fs::path absPath = RteFsUtils::AbsolutePath(RteUtils::BackSlashesToSlashes(layerFile));

    const string& origin = absPath.parent_path().generic_string();
    error_code ec;
    fs::copy(origin, m_cprjPath, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    if (ec) {
      LogMsg("M208", VAL("ORIG", origin), VAL("DEST", m_cprjPath));
      return false;
    }
    const string& clayer = m_cprjPath + "/" + absPath.filename().generic_string();
    fs::remove(clayer, ec);
    if (ec) {
      LogMsg("M212", PATH(clayer));
      return false;
    }
  }

  // Get readme file in the format layer.<name>.md
  for (auto layerName : layerNameList) {
    const string& readmeFile = "layer." + layerName + ".md";
    error_code ec;
    for (auto& p : fs::recursive_directory_iterator(m_cprjPath, ec)) {
      if (fs::is_regular_file(p, ec)) {
        if (p.path().filename() == readmeFile) {
          m_readmeFiles.push_back(p.path().generic_string());
        }
      }
    }
  }

  // Merge readme files
  if (!m_readmeFiles.empty()) {
    const string& projectReadmeFile = m_cprjPath + "/README.md";
    ofstream projectReadme(projectReadmeFile);
    if (!projectReadme) {
      LogMsg("M210", PATH(projectReadmeFile));
      return false;
    }
    for (auto it = m_readmeFiles.begin(); it != m_readmeFiles.end(); it++) {
      const string& layerReadmeFile = *it;
      ifstream layerReadme(layerReadmeFile);
      stringstream buffer;
      buffer << layerReadme.rdbuf();
      projectReadme << buffer.str();
      if (next(it) != m_readmeFiles.end()) projectReadme << EOL;
      layerReadme.close();
      error_code ec;
      fs::remove(layerReadmeFile, ec);
      if (ec) {
        LogMsg("M212", PATH(layerReadmeFile));
        return false;
      }
    }
    // Flush and close file
    projectReadme.flush();
    projectReadme.close();
  }

  // Write Xml file
  if (!WriteXmlFile(args.file, m_cprjTree)) {
    return false;
  }

  // Construct RTE Model
  if (!ConstructModel (compilersElement, args)) {
    return false;
  }

  return true;
}

bool CbuildLayer::Add(const CbuildLayerArgs& args) {
  // Init Cprj
  if (!InitXml(args.file)) {
    return false;
  }
  InitHeaderInfo(args.file);

  // Created
  std::map<std::string, std::string> attributes;
  attributes["timestamp"] = m_timestamp;
  attributes["tool"] = m_tool;
  if (!m_cprj->created) m_cprj->created = m_cprj->root->CreateElement("created");
  m_cprj->created->SetAttributes(attributes);

  // Info
  set<string> categoryList, keywordsList, licenseList;
  GetArgsFromChild(m_cprj->info, "category", categoryList);
  GetArgsFromChild(m_cprj->info, "keywords", keywordsList);
  GetArgsFromChild(m_cprj->info, "license", licenseList);

  // Check if the project has layers
  if (!m_cprj->layers) {
    m_cprj->layers = m_cprj->root->CreateElement("layers");
  }

  // Parse layer files
  for (auto layerFile : args.layerFiles) {
    string layerName;
    if (!InitXml(layerFile, &layerName)) {
      return false;
    }
  }

  // Iterate over list of layers
  for (auto it : m_layer) {
    xml_elements* element = it.second;

    // Read info fields
    GetArgsFromChild(element->layer, "category", categoryList);
    GetArgsFromChild(element->layer, "keywords", keywordsList);
    GetArgsFromChild(element->layer, "license", licenseList);

    // Layers
    CopyElement(m_cprj->layers, element->layer);

    // Packages
    auto packages = element->packages->GetChildren();
    for (auto package : packages) {
      // Copy avoiding duplicates
      auto cprjPackages = m_cprj->packages->GetChildren();
      bool duplicated = false;
      for (auto cprjPackage : cprjPackages) {
        if (cprjPackage->GetAttributes() == package->GetAttributes()) {
          // package is already in the cprj packages list
          duplicated = true;
          break;
        }
      }
      if (!duplicated) {
        CopyElement(m_cprj->packages, package);
      }
    }

    // Compilers
    const auto compilers = element->compilers->GetChildren();
    for (auto compiler : compilers) {
      // Copy avoiding duplicates
      const auto cprjCompilers = m_cprj->compilers->GetChildren();
      bool duplicated = false;
      for (auto cprjCompiler : cprjCompilers){
        if (cprjCompiler->GetAttributes() == compiler->GetAttributes()) {
          // compiler is already in the cprj compilers list
          duplicated = true;
          break;
        }
      }
      if (!duplicated) {
        CopyElement(m_cprj->compilers, compiler);
      }
    }

    // Target
    if (element->layer->GetAttributeAsBool("hasTarget")) {
      if (!m_cprj->target) {
        CopyElement(m_cprj->root, element->target);
      } else {
        // Warning (multiple layers have target element)
        LogMsg("M631");
      }
    }

    // Components
    if (element->components) {
      for (auto child : element->components->GetChildren()) {
        CopyElement(m_cprj->components, child);
      }
    }

    // Files
    if (element->files) {
      const auto children = element->files->GetChildren();
      for (auto child : children) {
        CopyNestedGroups(m_cprj->files, child);
      }
    }
  }

  // Add info fields
  XMLTreeElement* category = m_cprj->info->GetFirstChild("category");
  if (!category) {
    category = m_cprj->info->CreateElement("category");
  }
  category->SetText(MergeArgs(categoryList));
  XMLTreeElement* keywords = m_cprj->info->GetFirstChild("keywords");
  if (!keywords) {
    keywords = m_cprj->info->CreateElement("keywords");
  }
  keywords->SetText(MergeArgs(keywordsList));
  XMLTreeElement* license = m_cprj->info->GetFirstChild("license");
  if (!license) {
    license = m_cprj->info->CreateElement("license");
  }
  license->SetText(MergeArgs(licenseList));

  // Copy files recursively
  for (auto layerFile : args.layerFiles) {
    fs::path absPath = RteFsUtils::AbsolutePath(RteUtils::BackSlashesToSlashes(layerFile));
    const string& origin = absPath.parent_path().generic_string();
    const string& clayer = m_cprjPath + "/" + absPath.filename().generic_string();
    error_code ec;
    fs::copy(origin, m_cprjPath, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    if (ec) {
      LogMsg("M208", VAL("ORIG", origin), VAL("DEST", m_cprjPath));
      return false;
    }
    fs::remove(clayer, ec);
    if (ec) {
      LogMsg("M212", PATH(clayer));
      return false;
    }
  }

  // Sort first level sections
  auto sections = m_cprj->root->GetChildren();
  sections.sort(CompareSections);

  for (auto section : sections)
    m_cprj->root->RemoveChild(section, false);
  for (auto section : sections)
    m_cprj->root->AddChild(section);

  // Write Xml file
  if (!WriteXmlFile(args.file, m_cprjTree, true)) {
    return false;
  }

  // Construct RTE Model
  if (!ConstructModel (m_cprj->compilers, args)) {
    return false;
  }

  return true;
}

bool CbuildLayer::Remove(const CbuildLayerArgs& args) {
  // Init Cprj
  if (!InitXml(args.file)) {
    return false;
  }

  // Check if the project has layers
  if (!m_cprj->layers) {
    LogMsg("M215");
    return false;
  }

  // Init Header Info
  InitHeaderInfo(args.file);

  // Created
  std::map<std::string, std::string> attributes;
  attributes["timestamp"] = m_timestamp;
  attributes["tool"] = m_tool;
  m_cprj->created->SetAttributes(attributes);

  // Info
  set<string> categoryList, keywordsList, licenseList;
  GetArgsFromChild(m_cprj->info, "category", categoryList);
  GetArgsFromChild(m_cprj->info, "keywords", keywordsList);
  GetArgsFromChild(m_cprj->info, "license", licenseList);

  // Construct RTE Model
  if (!ConstructModel (m_cprj->compilers, args)) {
    return false;
  }

  set<string> remainingLayers;
  set<string> remainingPacks;
  set<string> remainingCategories;
  set<string> remainingKeywords;
  set<string> remainingLicenses;
  set<string> categoriesToBeRemoved;
  set<string> keywordsToBeRemoved;
  set<string> licensesToBeRemoved;

  set<string> cprjPackIDList;
  auto cprjPacks = m_cprj->packages->GetChildren();
  for (auto cprjPack : cprjPacks) {
    cprjPackIDList.insert(RtePackage::GetPackageIDfromAttributes(*cprjPack));
  }

  // Iterate over list of layers
  const auto layers = m_cprj->layers->GetChildren();
  for (auto layer : layers) {
    const string& layerName = layer->GetAttribute("name");

    // Remaining layers
    if (find(args.layerFiles.begin(), args.layerFiles.end(), layerName) == args.layerFiles.end()) {
      remainingLayers.insert(layerName);
      remainingPacks.insert(m_layerPackages[layerName].begin(), m_layerPackages[layerName].end());
      GetArgsFromChild(layer, "category", remainingCategories);
      GetArgsFromChild(layer, "keywords", remainingKeywords);
      GetArgsFromChild(layer, "license", remainingLicenses);
      continue;
    }

    // Layers to be removed
    // Read info fields
    GetArgsFromChild(layer, "category", categoriesToBeRemoved);
    GetArgsFromChild(layer, "keywords", keywordsToBeRemoved);
    GetArgsFromChild(layer, "license", licensesToBeRemoved);

    // Layers
    m_cprj->layers->RemoveChild(layer, false);

    // Packages
    set<string> packsToBeRemoved = GetDiff(remainingPacks, m_layerPackages[layerName]);
    for (string packTobeRemoved : packsToBeRemoved) {
      bool fixedVersion = cprjPackIDList.find(packTobeRemoved) != cprjPackIDList.end();
      cprjPacks = m_cprj->packages->GetChildren();
      for (auto cprjPack : cprjPacks) {
        string cprjPackID = RtePackage::GetPackageIDfromAttributes(*cprjPack, fixedVersion);
        if ((fixedVersion && (packTobeRemoved == cprjPackID)) ||
           (!fixedVersion && (packTobeRemoved.find(cprjPackID) != string::npos))) {
          m_cprj->packages->RemoveChild(cprjPack, true);
        }
      }
    }

    // Target
    if (layer->GetAttributeAsBool("hasTarget")) {
      if (m_cprj->target) {
        m_cprj->root->RemoveChild(m_cprj->target, true);
      }
      // Missing <target> element in cprj
      LogMsg("M631");
    }

    // Components
    const auto components = m_cprj->components->GetChildren();
    for (auto cprjComponent : components){
      if (cprjComponent->GetAttribute("layer") == layerName) {
        m_cprj->components->RemoveChild(cprjComponent, true);
      }
    }

    // Files
    RemoveMatchedChildren(layerName, m_cprj->files);

    // Find infrastructure files
    error_code ec;
    for (auto& p : fs::recursive_directory_iterator(m_cprjPath, ec)) {
      const string& file = p.path().stem().generic_string();
      if (file == "layer."+layerName) {
        m_layerFiles[layerName].insert(p.path().generic_string().substr(m_cprjPath.length(), string::npos));
      }
    }

    // Remove files
    for (auto layerFile : m_layerFiles[layerName]) {
      const string& path = m_cprjPath + "/" + layerFile;
      //fs::remove_all(path, ec); // TODO: check wrong behaviour in Linux
      if (fs::is_directory(path, ec)) {
        for(auto& p: fs::recursive_directory_iterator(path, ec)) {
          if (fs::is_regular_file(p, ec)) fs::remove(p, ec);
        }
      } else {
        fs::remove(path, ec);
      }
      if (ec) {
        LogMsg("M212", PATH(path));
        return false;
      }
    }
  }

  // Update info fields
  XMLTreeElement* category = m_cprj->info->GetFirstChild("category");
  if (category) category->SetText(MergeArgs(RemoveArgs(GetDiff(remainingCategories, categoriesToBeRemoved), categoryList)));
  XMLTreeElement* keywords = m_cprj->info->GetFirstChild("keywords");
  if (keywords) keywords->SetText(MergeArgs(RemoveArgs(GetDiff(remainingKeywords, keywordsToBeRemoved), keywordsList)));
  XMLTreeElement* license = m_cprj->info->GetFirstChild("license");
  if (license) license->SetText(MergeArgs(RemoveArgs(GetDiff(remainingLicenses, licensesToBeRemoved), licenseList)));

  // Write Xml file
  if (!WriteXmlFile(args.file, m_cprjTree, true)) {
    return false;
  }

  return true;
}

void CbuildLayer::CopyMatchedChildren(const XMLTreeElement* origin, XMLTreeElement* destination, const string& layer, const string& parentLayer) {
  /*
  CopyMatchedChildren:
  Recursively copy origin element ('files' or 'components') and its children elements into destination parent if the children's layer attribute matches
  */
  const string& tag = origin->GetTag();
  const string& originLayer = origin->GetAttribute("layer");

  // Element with empty layer attribute inherits the layer assignment from its parent
  const string& effectiveLayer = originLayer.empty() ? parentLayer : originLayer;

  if (tag == "group") {
    // Skip group with different layer assignment but process the nested groups further if unassigned
    if (!effectiveLayer.empty() && (effectiveLayer != layer)) {
      return;
    }
  } else if (!((tag == "files") || (tag == "components"))) {
    // Skip any other element with different or empty layer assignment (except 'files' and 'components')
    if (effectiveLayer != layer) {
      return;
    }
  }

  XMLTreeElement* copy = destination->CreateElement(origin->GetTag());
  copy->SetText(origin->GetText());
  copy->SetAttributes(origin->GetAttributes());
  for (auto child : origin->GetChildren()) {
    CopyMatchedChildren(child, copy, layer, effectiveLayer);
  }

  // Remove 'files' or 'group' if empty
  if ((!copy->HasChildren()) && ((tag == "files") || (tag == "group"))) {
    destination->RemoveChild(copy, true);
  }
}

void CbuildLayer::RemoveMatchedChildren(const string& layer, XMLTreeElement* item) {
  /*
  RemoveMatchedChildren:
  Recursively remove children elements if the children's layer attribute matches
  Remove the element itself if it becomes an empty 'files' or 'group' element
  */
  auto children = item->GetChildren();
  for (auto child : children) {
    if (child->GetAttribute("layer") == layer) {
      item->RemoveChild(child, true);
    } else {
      RemoveMatchedChildren(layer, child);
    }
  }
  // Remove 'files' or 'group' if empty
  const string& tag = item->GetTag();
  if ((!item->HasChildren()) && ((tag == "files")||(tag == "group"))) {
    item->GetParent()->RemoveChild(item, true);
  }
}

void CbuildLayer::CopyElement (XMLTreeElement* destination, const XMLTreeElement* origin, const bool create) {
  /*
  CopyElement:
  Recursively copy origin element into given destination parent
  If create flag is false it reuses a destination's child
  */
  XMLTreeElement* copy = create ? destination->CreateElement(origin->GetTag()) : destination->GetFirstChild(origin->GetTag());
  if (!copy) {
    return;
  }
  copy->SetText(origin->GetText());
  copy->SetAttributes(origin->GetAttributes());
  if (origin->HasChildren()) {
    for (auto child : origin->GetChildren()) {
      CopyElement(copy, child);
    }
  }
}

void CbuildLayer::CopyNestedGroups (XMLTreeElement* destination, const XMLTreeElement* origin) {
  /*
  CopyNestedGroups:
  Recursively copy elements merging nested groups with same name
  The origin parameter shall be tipically a 'group' or 'file' element
  */
  XMLTreeElement* copy = nullptr;
  if (origin->GetTag() == "group") {
    for (const auto child : destination->GetChildren()) {
      if ((child->GetTag() == "group") && (child->GetAttribute("name") == origin->GetAttribute("name"))) {
        // Merge groups
        copy = child;
        break;
      }
    }
  }
  // Copy element
  if (!copy) {
    copy = destination->CreateElement(origin->GetTag());
    copy->SetText(origin->GetText());
    copy->SetAttributes(origin->GetAttributes());
  }
  // Iterate recursively over children elements
  if (origin->HasChildren()) {
    for (auto child : origin->GetChildren()) {
      CopyNestedGroups(copy, child);
    }
  }
}

set<string> CbuildLayer::SplitArgs(const string& args) {
  /*
  SplitArgs:
  Split arguments from the string 'args' into a set of strings
  */
  set<string> s;
  size_t end = 0, start = 0, len = args.length();
  while (end < len) {
    if ((end = args.find(", ", start)) == string::npos) end = len;
    if (start > 0) start++;
    s.insert(args.substr(start, end - start));
    start = end + 1;
  }
  return s;
}

set<string> CbuildLayer::GetDiff(const set<string>& actual, const set<string>& reference) {
  /*
  GetDiff:
  Retrieve items from the 'reference' set that are not present in the 'actual' set
  */
  set<string> intersect, diff;
  set_intersection(reference.begin(), reference.end(), actual.begin(), actual.end(), inserter(intersect, intersect.begin()));
  set_difference(reference.begin(), reference.end(), intersect.begin(), intersect.end(), inserter(diff, diff.begin()));
  return diff;
}

set<string> CbuildLayer::RemoveArgs(const set<string>& remove, const set<string>& reference) {
  /*
  RemoveArgs:
  Remove items listed in 'remove' set from 'reference' set
  */
  set<string> list = reference;
  for (auto rem_item : remove) {
    for (auto ref_item : list) {
      if (rem_item.compare(ref_item) == 0) {
        list.erase(ref_item);
        break;
      }
    }
  }
  return list;
}

string CbuildLayer::MergeArgs(const set<string>& reference) {
  /*
  MergeArgs:
  Merge arguments from 'reference' set into a comma separated string
  */
  string text;
  for (auto it = reference.begin(); it != reference.end(); it++) {
    text += (*it);
    if (next(it) != reference.end()) text += ", ";
  }
  return text;
}

void CbuildLayer::GetArgsFromChild(const XMLTreeElement* parent, const string& tag, set<string>& list) {
  /*
  GetArgsFromChild:
  Read arguments from child's element into a list
  */
  XMLTreeElement* child = parent->GetFirstChild(tag);
  if (child) {
    set<string> args = SplitArgs(child->GetText());
    list.insert(args.begin(), args.end());
  }
}

bool CbuildLayer::InitXml(const string &file, string* layerName) {
  error_code ec;
  if (!fs::exists(file, ec)) { // file does not exist
    LogMsg("M204", PATH(file));
    return false;
  }

  // Parse Xml Tree
  XMLTree* tree = new XMLTreeSlim();
  tree->Init();
  if (!tree->AddFileName(file, true)) {
    delete tree;
    LogMsg("M203", PATH(file));
    return false;
  }

  // Get sections
  xml_elements *elements = new xml_elements();
  if (!GetSections(tree, elements, layerName)) {
    delete tree;
    delete elements;
    return false;
  }

  // Save member variables
  if (elements->isLayer) {
    m_layerTree[*layerName] = tree;
    m_layer[*layerName] = elements;
  } else {
    m_cprjTree = tree;
    m_cprj = elements;
    m_cprj->root->ClearAttributes();
  }
  return true;
}

bool CbuildLayer::GetSections(const XMLTree* tree, xml_elements* elements, string* layerName) {
  /*
  GetSections:
  Retrieve first level elements
  */

  // Get pointers to cprj sections
  elements->root = tree->GetFirstChild("cprj");
  if (!elements->root) {
    // Missing <cprj> element
    LogMsg("M609", VAL("NAME", "cprj"));
    return false;
  }
  elements->info = elements->root->GetFirstChild("info");
  if (!elements->info) {
    // Missing <info> element
    LogMsg("M609", VAL("NAME", "info"));
    return false;
  }
  elements->packages = elements->root->GetFirstChild("packages");
  if (!elements->packages) {
    // Missing <packages> element
    LogMsg("M609", VAL("NAME", "packages"));
    return false;
  }

  // Get pointer to layer section and layer name
  elements->layers = elements->root->GetFirstChild("layers");
  elements->isLayer = elements->info->GetAttributeAsBool("isLayer");
  if (elements->isLayer) {
    if (elements->layers) {
      elements->layer = elements->layers->GetFirstChild("layer");
    }
    if (!elements->layer) {
      // Missing <layer> element
      LogMsg("M609", VAL("NAME", "layer"));
      return false;
    }
    *layerName = elements->layer->GetAttribute("name");
  }

  // Get pointer to compilers section
  elements->compilers = elements->root->GetFirstChild("compilers");
  if ((!elements->isLayer) && (!elements->compilers)) {
    // Missing <compilers> element in cprj
    LogMsg("M609", VAL("NAME", "compilers"));
    return false;
  }

  // Get pointer to target
  elements->target = elements->root->GetFirstChild("target");
  if (!elements->isLayer) {
    if (!elements->target) {
      // Missing <target> element in cprj
      LogMsg("M631");
    }
  } else {
    if (!elements->target && elements->layer->GetAttributeAsBool("hasTarget")) {
      // Missing <target> element in clayer
      LogMsg("M609", VAL("NAME", "target"));
      return false;
    }
  }

  // Get pointer to other elements
  elements->created    = elements->root->GetFirstChild("created");
  elements->components = elements->root->GetFirstChild("components");
  elements->files      = elements->root->GetFirstChild("files");

  return true;
}

bool CbuildLayer::WriteXmlFile(const string &file, XMLTree* tree, const bool saveBackup) {
  // Format Xml content
  XmlFormatter xmlFormatter(tree, SCHEMA_FILE, SCHEMA_VERSION);
  const string& xmlContent = xmlFormatter.GetContent();

  // Save backup file
  if (saveBackup) {
    error_code ec;
    fs::rename(file, file + ".bak", ec);
    if (ec) {
      LogMsg("M210", PATH(file));
      return false;
    }
  }

  // Save file
  ofstream xmlFile(file);
  if (!xmlFile) {
    LogMsg("M210", PATH(file));
    return false;
  }

  xmlFile << xmlContent;
  xmlFile << std::endl;
  xmlFile.close();

  return true;
}

bool CbuildLayer::InitHeaderInfo(const string &file) {
  // Used path and tool
  fs::path filePath = RteFsUtils::AbsolutePath(RteUtils::BackSlashesToSlashes(file));
  m_cprjFile = filePath.filename().generic_string();
  m_cprjPath = filePath.remove_filename().generic_string();
  m_tool = string(ORIGINAL_FILENAME) + " " + string(VERSION_STRING);

  // Timestamp
  m_timestamp = CbuildUtils::GetLocalTimestamp();

  return true;
}

int CbuildLayer::GetSectionNumber (const string& tag) {
  /*
  GetSectionNumber:
  Get ordinal number for 'tag'
  */
  if      (tag == "created")    return 1;
  else if (tag == "info")       return 2;
  else if (tag == "layers")     return 3;
  else if (tag == "packages")   return 4;
  else if (tag == "compilers")  return 5;
  else if (tag == "target")     return 6;
  else if (tag == "components") return 7;
  else if (tag == "files")      return 8;
  return 0;
}

bool CbuildLayer::CompareSections (const XMLTreeElement* first, const XMLTreeElement* second) {
  /*
  CompareSections:
  Compare operator to sort first level sections
  */
  return (GetSectionNumber(first->GetTag()) < GetSectionNumber(second->GetTag()));
}

bool CbuildLayer::ConstructModel (const XMLTreeElement* compilers, const CbuildLayerArgs& args) {
  const string& toolchain = compilers->GetFirstChild()->GetAttribute("name");
  const string& update = "", intdir = "";
  if (!CbuildKernel::Get()->Construct({args.file, args.rtePath, args.compilerRoot, toolchain, update, intdir, args.envVars, false, true})) {
    return false;
  }
  m_layerFiles = CbuildKernel::Get()->GetModel()->GetLayerFiles();
  m_layerPackages = CbuildKernel::Get()->GetModel()->GetLayerPackages();
  CbuildKernel::Destroy();
  return true;
}
