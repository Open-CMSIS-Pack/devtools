/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "RteModelTestConfig.h"

#include "RteItem.h"
#include "RteCondition.h"
#include <map>
using namespace std;


TEST(RteItemTest, GetComponentID_all_attributes) {
  const map<string, string> attributes = {
    {"Cvendor" , "Vendor"  },
    {"Cclass"  , "Class"   },
    {"Cbundle" , "Bundle"  },
    {"Cgroup"  , "Group"   },
    {"Csub"    , "Sub"     },
    {"Cvariant", "Variant" },
    {"Cversion", "9.9.9"   },
    {"Capiversion", "1.1.1"   }
  };
  RteItem item(attributes);
  item.SetTag("require");

  EXPECT_EQ("::Class:Group(API)@1.1.1", item.GetApiID(true));
  EXPECT_EQ("::Class:Group(API)", item.GetApiID(false));

  EXPECT_EQ("Vendor::Class&Bundle:Group:Sub&Variant@9.9.9", item.GetComponentID(true));
  EXPECT_EQ("Vendor::Class&Bundle:Group:Sub&Variant", item.GetComponentID(false));

  EXPECT_EQ("Class&Bundle:Group:Sub&Variant", item.GetPartialComponentID(true));
  EXPECT_EQ("Class:Group:Sub&Variant", item.GetPartialComponentID(false));

  EXPECT_EQ("Vendor::Class&Bundle:Group:Sub", item.GetComponentAggregateID());

  EXPECT_EQ("Vendor::Class&Bundle", item.GetBundleShortID());
  EXPECT_EQ("Vendor::Class&Bundle", item.GetBundleID(false));
  EXPECT_EQ("Vendor::Class&Bundle@9.9.9", item.GetBundleID(true));

  EXPECT_EQ("Class:Group:Sub", item.GetTaxonomyDescriptionID());

  EXPECT_EQ("require Vendor::Class&Bundle:Group:Sub&Variant@9.9.9", item.GetDependencyExpressionID());
}

TEST(RteItemTest, GetComponentID_reduced_attributes) {
  const map<string, string> attributes = {
    {"Cvendor" , "Vendor"  },
    {"Cclass"  , "Class"   },
    {"Cgroup"  , "Group"   }
  };
  RteItem item(attributes);
  item.SetTag("accept");
  EXPECT_EQ("accept Vendor::Class:Group", item.GetDependencyExpressionID());

  EXPECT_EQ("Vendor::Class:Group", item.GetComponentID(true));
  EXPECT_EQ("Class:Group", item.GetPartialComponentID(true));

  EXPECT_TRUE(item.GetBundleID(true).empty());
  EXPECT_TRUE(item.GetBundleShortID().empty());

  EXPECT_EQ("Class:Group", item.GetTaxonomyDescriptionID());
}

TEST(RteItemTest, ComponentAttributesFromId) {

  string id = "Vendor::Class&Bundle:Group:Sub&Variant@9.9.9";
  RteItem item("component", nullptr);
  item.SetAttributesFomComponentId(id);
  EXPECT_EQ(id, item.GetComponentID(true));

  id = "Class&Bundle:Group:Sub&Variant@9.9.9";
  item.SetAttributesFomComponentId(id);
  EXPECT_EQ(id, item.GetComponentID(true));

  id = "Vendor::Class:Group&Variant";
  item.SetAttributesFomComponentId(id);
  EXPECT_EQ(id, item.GetComponentID(true));

  id = "Class:Group:Sub&Variant";
  item.SetAttributesFomComponentId(id);
  EXPECT_EQ(id, item.GetComponentID(true));

  id = "Class:Group:&Variant";
  item.SetAttributesFomComponentId(id);
  EXPECT_EQ("Class:Group&Variant", item.GetComponentID(true));
}


// end of RteItemTest.cpp
