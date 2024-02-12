/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "CBuildUnitTestEnv.h"

#include "CbuildLayer.h"

using namespace std;

class CbuildLayerTests : public CbuildLayer, public ::testing::Test {
public:
  CbuildLayerTests() {}
  virtual ~CbuildLayerTests() {}

  void SetUp();
  void TearDown();

  void CompareXMLFile(const string& file, const bool& expect);
};

void CbuildLayerTests::SetUp() {
}

void CbuildLayerTests::TearDown() {
  fs::current_path(CBuildUnitTestEnv::workingDir);
}

void CbuildLayerTests::CompareXMLFile(const string& file, const bool& expect) {
  if (expect) {
    ifstream f1, f2;
    string l1, l2;
    bool ret_val;

    string reffile = testinput_folder + "/outfile.xml.ref";
    f1.open(reffile);
    ret_val = f1.is_open();
    ASSERT_EQ(ret_val, true) << "Failed to open " << reffile;

    f2.open(file);
    ret_val = f2.is_open();
    ASSERT_EQ(ret_val, true) << "Failed to open " << file;

    while (getline(f1, l1) && getline(f2, l2)) {
      if (!l1.empty() && l1.rfind('\r') == l1.length() - 1) {
        l1.pop_back();
      }

      if (!l2.empty() && l2.rfind('\r') == l2.length() - 1) {
        l2.pop_back();
      }

      if (l1 != l2) {
        FAIL() << file << " is different from " << reffile;
      }
    }

    f1.close();
    f2.close();
  }
}

TEST_F(CbuildLayerTests, InitXml_InvalidFile) {
  string fileName = "InvalidFile";

  EXPECT_FALSE(InitXml(fileName));
}

TEST_F(CbuildLayerTests, InitXml_InvalidSchema) {
  error_code ec;
  string fileName = "InvalidSchema.pdsc";
  fs::current_path(testinput_folder, ec);

  EXPECT_FALSE(InitXml(fileName));
}

TEST_F(CbuildLayerTests, InitXml_NoLayer) {
  error_code ec;
  string fileName, layerName;
  fileName = "LayerInfoMissing.cprj";
  fs::current_path(testinput_folder, ec);

  EXPECT_FALSE(InitXml(fileName, &layerName));
  EXPECT_TRUE(layerName.empty());
}

TEST_F(CbuildLayerTests, InitXml_LayerFound) {
  error_code ec;
  string fileName = "LayerProject.cprj";
  fs::current_path(testinput_folder, ec);

  EXPECT_TRUE(InitXml(fileName));
}

TEST_F(CbuildLayerTests, InitHeaderInfo) {
  string prjFile  = "ValidTarget.cprj";
  string filePath = testinput_folder + "/" + prjFile;

  ASSERT_TRUE(InitHeaderInfo(filePath));
  EXPECT_EQ(prjFile, m_cprjFile);
}

TEST_F(CbuildLayerTests, WriteXmlFile) {
  error_code ec;
  string outFile;
  auto tree = make_unique<XMLTreeSlim>();
  tree->CreateElement("cprj");

  {
    outFile = testout_folder + "/Invalidfile.xml";
    if (fs::exists(outFile, ec)) {
      fs::remove(outFile, ec);
    }

    if (fs::exists(outFile + ".bak", ec)) {
      fs::remove(outFile + ".bak", ec);
    }

    ASSERT_FALSE(WriteXmlFile(outFile, tree.get(), true));
    ASSERT_FALSE(fs::exists(outFile, ec)) << "error: " << outFile << " does not exist";
    ASSERT_FALSE(fs::exists(outFile + ".bak", ec)) << "error: " << outFile << ".bak does not exist";
    CompareXMLFile(outFile, false);
  }

  {
    outFile = testout_folder + "/outfile.xml";
    if (fs::exists(outFile, ec)) {
      fs::remove(outFile, ec);
    }

    if (fs::exists(outFile + ".bak", ec)) {
      fs::remove(outFile + ".bak", ec);
    }

    std::ofstream ofs(outFile); ofs.close();

    ASSERT_TRUE(WriteXmlFile(outFile, tree.get(), true));
    ASSERT_TRUE(fs::exists(outFile, ec)) << "error: " << outFile << " does not exist";
    ASSERT_TRUE(fs::exists(outFile + ".bak", ec)) << "error: " << outFile << ".bak does not exist";
    CompareXMLFile(outFile, true);
  }
}

TEST_F(CbuildLayerTests, GetSections) {
  {
    auto tree = make_unique<XMLTreeSlim>();
    xml_elements elements; string layerName;
    // Missing <cprj> element
    EXPECT_FALSE(GetSections(tree.get(), &elements, &layerName));
  }

  {
    xml_elements elements; string layerName;
    auto tree = make_unique<XMLTreeSlim>();
    tree->CreateElement("cprj");
    // Missing <info> element
    EXPECT_FALSE(GetSections(tree.get(), &elements, &layerName));
  }

  {
    xml_elements elements; string layerName;
    auto tree = make_unique<XMLTreeSlim>();
    auto rootElement = tree->CreateElement("cprj");
    rootElement->CreateElement("info");
    // Missing <packages> element
    EXPECT_FALSE(GetSections(tree.get(), &elements, &layerName));
  }

  {
    xml_elements elements; string layerName;
    auto tree = make_unique<XMLTreeSlim>();
    auto rootElement = tree->CreateElement("cprj");
    rootElement->CreateElement("info");
    rootElement->CreateElement("packages");
    // Missing <compilers> element
    EXPECT_FALSE(GetSections(tree.get(), &elements, &layerName));
  }

  {
    xml_elements elements; string layerName;
    auto tree = make_unique<XMLTreeSlim>();
    auto rootElement = tree->CreateElement("cprj");
    rootElement->CreateElement("info");
    rootElement->CreateElement("packages");
    rootElement->CreateElement("compilers");
    //isLayer default to false
    EXPECT_TRUE(GetSections(tree.get(), &elements, &layerName));
  }

  {
    xml_elements elements; string layerName;
    auto tree = make_unique<XMLTreeSlim>();
    auto rootElement = tree->CreateElement("cprj");
    auto info = rootElement->CreateElement("info");
    info->AddAttribute("isLayer", "true");

    rootElement->CreateElement("packages");
    rootElement->CreateElement("compilers");
    rootElement->CreateElement("layers");
    // layers has no child element
    EXPECT_FALSE(GetSections(tree.get(), &elements, &layerName));
  }

  {
    xml_elements elements; string layerName;
    auto tree = make_unique<XMLTreeSlim>();
    auto rootElement = tree->CreateElement("cprj");
    auto info = rootElement->CreateElement("info");
    info->AddAttribute("isLayer", "false");

    rootElement->CreateElement("packages");
    rootElement->CreateElement("compilers");
    auto layers = rootElement->CreateElement("layers");
    auto layerElem = layers->CreateElement("layer");
    layerElem->AddAttribute("name", "application");

    ASSERT_TRUE(GetSections(tree.get(), &elements, &layerName));
    EXPECT_TRUE(layerName.empty());
  }

  {
    xml_elements elements; string layerName;
    auto tree = make_unique<XMLTreeSlim>();
    auto rootElement = tree->CreateElement("cprj");
    auto info = rootElement->CreateElement("info");
    info->AddAttribute("isLayer", "true");

    rootElement->CreateElement("packages");
    rootElement->CreateElement("compilers");
    auto layers = rootElement->CreateElement("layers");
    auto layerElem = layers->CreateElement("layer");
    layerElem->AddAttribute("name", "application");

    ASSERT_TRUE(GetSections(tree.get(), &elements, &layerName));
    EXPECT_EQ("application", layerName);
  }
}

TEST_F(CbuildLayerTests, CopyElement) {
  {
    XMLTreeElement dest;
    XMLTreeElement src;
    XMLTreeElement* info = src.CreateElement("info");
    info->AddAttribute("isLayer", "true");

    CopyElement(&dest, info);
    auto elem = dest.GetFirstChild("info");
    ASSERT_EQ(true, (nullptr == elem) ? false : true);
    EXPECT_TRUE(elem->GetAttributeAsBool("isLayer"));
  }

  {
    XMLTreeElement dest;
    XMLTreeElement src;
    XMLTreeElement* info = src.CreateElement("info");
    info->AddAttribute("isLayer", "true");

    CopyElement(&dest, info, false);
    auto elem = dest.GetFirstChild("info");
    EXPECT_EQ(true, (nullptr == elem) ? true : false);
  }
}

TEST_F(CbuildLayerTests, CopyMatchedChildren) {
  {
    XMLTreeElement dest, src;
    src.SetTag("files");
    CopyMatchedChildren(&src, &dest, "device");
    EXPECT_TRUE(dest.GetChildren().empty());
  }

  {
    XMLTreeElement dest, src;
    src.SetTag("group");
    CopyMatchedChildren(&src, &dest, "device");
    EXPECT_TRUE(dest.GetChildren().empty());
  }

  {
    XMLTreeElement dest, src;
    map<string, string> attr{ {"description", "test app"} };
    src.SetTag("files");
    src.SetText("test text");
    src.SetAttributes(attr);
    auto group = src.CreateElement("group");
    auto file = group->CreateElement("file");
    file->AddAttribute("layer", "device");

    CopyMatchedChildren(&src, &dest, "device");
    auto children = dest.GetChildren();
    ASSERT_EQ(children.size(), 1);
    auto groupChild = children.front()->GetFirstChild("group");
    ASSERT_EQ(true, (nullptr == groupChild) ? false: true);
    auto fileChild = groupChild->GetFirstChild("file");
    ASSERT_EQ(true, (nullptr == fileChild) ? false : true);
    EXPECT_EQ("device", fileChild->GetAttribute("layer"));
  }
}

TEST_F(CbuildLayerTests, RemoveMatchedChildren) {
  {
    XMLTreeElement item;
    RemoveMatchedChildren("device", &item);
    EXPECT_TRUE(item.GetChildren().empty());
  }

  {
    XMLTreeElement item;
    auto files = item.CreateElement("files");
    auto fileDevice = files->CreateElement("file");
    auto fileApplication = files->CreateElement("file");
    fileDevice->AddAttribute("layer", "device");
    fileApplication->AddAttribute("layer", "application");

    RemoveMatchedChildren("application", &item);
    auto children = item.GetChildren();
    ASSERT_EQ(children.size(), 1);

    RemoveMatchedChildren("device", &item);
    children = item.GetChildren();
    EXPECT_TRUE(item.GetChildren().empty());
  }
}

TEST_F(CbuildLayerTests, CopyNestedGroups) {
  {
    XMLTreeElement dest, files1, files2;
    // First layer elements
    files1.SetTag("files");
    auto group1 = files1.CreateElement("group");
    auto nestedGroup1 = group1->CreateElement("group");
    nestedGroup1->CreateElement("file");

    CopyNestedGroups(&dest, group1);
    auto child = dest.GetFirstChild("group");
    ASSERT_EQ(true, (nullptr == child) ? false: true);
    child = child->GetFirstChild("group");
    ASSERT_EQ(true, (nullptr == child) ? false: true);
    auto children = child->GetChildren();
    ASSERT_EQ(children.size(), 1);

    // Second layer elements
    files2.SetTag("files");
    auto group2 = files2.CreateElement("group");
    auto nestedGroup2 = group2->CreateElement("group");
    nestedGroup2->CreateElement("file");

    CopyNestedGroups(&dest, group2);
    child = dest.GetFirstChild("group");
    ASSERT_EQ(true, (nullptr == child) ? false: true);
    child = child->GetFirstChild("group");
    ASSERT_EQ(true, (nullptr == child) ? false: true);
    children = child->GetChildren();
    ASSERT_EQ(children.size(), 2);
  }
}

TEST_F(CbuildLayerTests, GetArgsFromChild) {
  {
    XMLTreeElement elem;
    string tag;
    set<string> list;
    GetArgsFromChild(&elem, tag, list);
    EXPECT_EQ(0, list.size());
  }

  {
    XMLTreeElement elem;
    auto info = elem.CreateElement("info");
    info->AddAttribute("isLayer", "true");
    info->CreateElement("category")->SetText("Blinky");
    info->CreateElement("keywords")->SetText("Blinky_keyword");
    info->CreateElement("license")->SetText("BSD-3");
    set<string> list;

    GetArgsFromChild(info, "category", list);
    GetArgsFromChild(info, "keywords", list);
    GetArgsFromChild(info, "license", list);
    EXPECT_EQ(3, list.size());
  }

}


TEST_F(CbuildLayerTests, MergeArgs) {
  string strArg = "ARM, CMSIS, CORE, device";
  set<string> args = { "CMSIS", "CORE", "ARM", "device" };
  auto res = SplitArgs(strArg);

  EXPECT_EQ(res, args);
  EXPECT_EQ(strArg, MergeArgs(res));

  res.clear();
  EXPECT_EQ("", MergeArgs(res));
}

TEST_F(CbuildLayerTests, RemoveArgs) {
  set<string> rem    = { "CMSIS", "CORE", "ARM", "device" };
  set<string> ref    = { "CMSIS", "SOURCE", "ARM", "Compiler" };
  set<string> expect = { "SOURCE", "Compiler" };
  auto res = RemoveArgs(rem, ref);
  EXPECT_EQ(expect, res);
}

TEST_F(CbuildLayerTests, GetDiff) {
  set<string> actual = { "CMSIS", "CORE", "ARM", "device" };
  set<string> ref = { "CMSIS", "SOURCE", "ARM", "Compiler" };
  set<string> expect = { "SOURCE", "Compiler"};
  auto res = GetDiff(actual, ref);
  EXPECT_EQ(expect, res);
}

TEST_F(CbuildLayerTests, GetSectionNumber) {
  EXPECT_EQ(1, GetSectionNumber("created"));
  EXPECT_EQ(2, GetSectionNumber("info"));
  EXPECT_EQ(3, GetSectionNumber("layers"));
  EXPECT_EQ(4, GetSectionNumber("packages"));
  EXPECT_EQ(5, GetSectionNumber("compilers"));
  EXPECT_EQ(6, GetSectionNumber("target"));
  EXPECT_EQ(7, GetSectionNumber("components"));
  EXPECT_EQ(8, GetSectionNumber("files"));
  EXPECT_EQ(0, GetSectionNumber("AnyOtherSection"));
}

TEST_F(CbuildLayerTests, CompareSections) {
  XMLTreeElement first, second;
  first.SetTag("packages");
  second.SetTag("layers");

  EXPECT_FALSE(CompareSections(&first, &second));

  second.SetTag("files");
  EXPECT_TRUE(CompareSections(&first, &second));
}
