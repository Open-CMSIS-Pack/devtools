/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "RteModelTestConfig.h"

#include "RteModel.h"
#include "RteKernelSlim.h"
#include "RteCprjProject.h"

using namespace std;

class RteConditionTest : public RteModelTestConfig {
  // reserved for future extensions
};


TEST(RteConditionValidateTest, Validate)
{
  RteAcceptExpression componentExpression(nullptr);
  EXPECT_FALSE(componentExpression.Validate());
  componentExpression.AddAttribute("Cclass", "MyClass");
  EXPECT_FALSE(componentExpression.Validate());
  componentExpression.AddAttribute("Cgroup", "MyGroup");
  EXPECT_TRUE(componentExpression.Validate());
  componentExpression.AddAttribute("c", "contId");
  EXPECT_FALSE(componentExpression.Validate());
  componentExpression.RemoveAttribute("Cclass");
  EXPECT_FALSE(componentExpression.Validate());

  RteDenyExpression deviceExpression(nullptr);
  deviceExpression.AddAttribute("Dname", "MyDevice");
  deviceExpression.AddAttribute("Dcore", "MyCore");
  EXPECT_TRUE(deviceExpression.Validate());
  deviceExpression.AddAttribute("Bname", "MyBoard");
  EXPECT_FALSE(deviceExpression.Validate());
  deviceExpression.RemoveAttribute("Bname");
  deviceExpression.AddAttribute("Pname", "MyProcessor");
  EXPECT_TRUE(deviceExpression.Validate());
  deviceExpression.AddAttribute("Unknown", "unknown");
  EXPECT_FALSE(deviceExpression.Validate());
}

TEST_F(RteConditionTest, MissingIgnoredFulfilledSelectable) {
  // load project to get a working target and condition contexts
  RteKernelSlim rteKernel;
  rteKernel.SetCmsisPackRoot(RteModelTestConfig::CMSIS_PACK_ROOT);
  RteCprjProject* loadedCprjProject = rteKernel.LoadCprj(RteTestM3_cprj);
  ASSERT_NE(loadedCprjProject, nullptr);
  RteTarget* activeTarget = loadedCprjProject->GetActiveTarget();
  ASSERT_NE(activeTarget, nullptr);
  RteConditionContext* filterContext = activeTarget->GetFilterContext();
  ASSERT_NE(filterContext, nullptr);
  RteModel* rteModel = activeTarget->GetFilteredModel();
  ASSERT_NE(rteModel, nullptr);

  RteDependencySolver* depSolver = activeTarget->GetDependencySolver();
  ASSERT_NE(depSolver, nullptr);
  EXPECT_EQ(depSolver->GetConditionResult(), RteItem::FULFILLED);

  RtePackageInstanceInfo packInfo(nullptr, "ARM.RteTest.0.1.0");
  RtePackage* pack = rteModel->GetPackage(packInfo);
  ASSERT_NE(pack, nullptr);
  RteCondition* denyDependency = pack->GetCondition("DenyDependency");
  ASSERT_NE(denyDependency, nullptr);
  RteCondition* denyRequireDependency = pack->GetCondition("DenyRequireDependency");
  ASSERT_NE(denyRequireDependency, nullptr);
  RteCondition* denyAcceptDependency = pack->GetCondition("DenyAcceptDependency");
  ASSERT_NE(denyAcceptDependency, nullptr);
  RteCondition* denyDenyDependency = pack->GetCondition("DenyDenyDependency");
  ASSERT_NE(denyDenyDependency, nullptr);

  // select component to check dependencies
  list<RteComponent*> components;
  RteComponentInstance item(nullptr);
  item.SetTag("component");
  item.SetAttributes({ {"Cclass","RteTest" },
                       {"Cgroup", "AcceptDependency" },
                       {"Cversion","0.9.9"},
                       {"condition","AcceptDependency"} });
  item.SetPackageAttributes(packInfo);

  RteComponent* c = rteModel->FindComponents(item, components);
  ASSERT_NE(c, nullptr);
  activeTarget->SelectComponent(c, 1, true);
  EXPECT_EQ(depSolver->GetConditionResult(), RteItem::FULFILLED); // required dependency already selected

 // check deny dependencies 1
  EXPECT_EQ(denyDependency->Evaluate(depSolver), RteItem::INCOMPATIBLE);
  EXPECT_EQ(denyRequireDependency->Evaluate(depSolver), RteItem::INCOMPATIBLE);
  EXPECT_EQ(denyAcceptDependency->Evaluate(depSolver), RteItem::INCOMPATIBLE);
  EXPECT_EQ(denyDenyDependency->Evaluate(depSolver), RteItem::FULFILLED);

// manually deselect components satisfying dependency
  item.SetAttributes({ {"Cclass","RteTest" },
                       {"Cgroup", "LocalFile" },
                       {"Cversion","0.0.3"} });
  components.clear();
  c = rteModel->FindComponents(item, components);
  ASSERT_NE(c, nullptr);
  activeTarget->SelectComponent(c, 0, true);
  EXPECT_EQ(depSolver->GetConditionResult(), RteItem::FULFILLED); // still fulfilled

   // check deny dependencies 2
  EXPECT_EQ(denyDependency->Evaluate(depSolver), RteItem::INCOMPATIBLE);
  EXPECT_EQ(denyRequireDependency->Evaluate(depSolver), RteItem::FULFILLED);
  EXPECT_EQ(denyAcceptDependency->Evaluate(depSolver), RteItem::INCOMPATIBLE);
  EXPECT_EQ(denyDenyDependency->Evaluate(depSolver), RteItem::FULFILLED);

  item.SetAttribute("Cgroup", "GlobalFile"),
  components.clear();
  c = rteModel->FindComponents(item, components);
  ASSERT_NE(c, nullptr);
  activeTarget->SelectComponent(c, 0, true);
  EXPECT_EQ(depSolver->GetConditionResult(), RteItem::SELECTABLE);

  // try to auto-resolve => no change (multiple variants for "AcceptDependency")
  EXPECT_EQ(depSolver->ResolveDependencies(), RteItem::SELECTABLE);

  // check deny dependencies 3
  EXPECT_EQ(denyDependency->Evaluate(depSolver), RteItem::FULFILLED);
  EXPECT_EQ(denyRequireDependency->Evaluate(depSolver), RteItem::FULFILLED);
  EXPECT_EQ(denyAcceptDependency->Evaluate(depSolver), RteItem::FULFILLED);
  EXPECT_EQ(denyDenyDependency->Evaluate(depSolver), RteItem::INCOMPATIBLE);

  // select component that directly depends on global file
  item.SetAttributes({ {"Cclass","RteTest" },
                     {"Cgroup", "RequireDependency" },
                     {"Cversion","0.9.9"},
                     {"condition", "GlobalFile"}});
  components.clear();
  c = rteModel->FindComponents(item, components);
  ASSERT_NE(c, nullptr);
  activeTarget->SelectComponent(c, 1, true);
  EXPECT_EQ(depSolver->GetConditionResult(), RteItem::SELECTABLE);
  // try to resolve = > FULFILED (multiple variants for "AcceptDependency" component, but only one for "RequireDependency")
  EXPECT_EQ(depSolver->ResolveDependencies(), RteItem::FULFILLED);

  // check deny dependencies 4
  EXPECT_EQ(denyDependency->Evaluate(depSolver), RteItem::INCOMPATIBLE);
  EXPECT_EQ(denyRequireDependency->Evaluate(depSolver), RteItem::FULFILLED);
  EXPECT_EQ(denyAcceptDependency->Evaluate(depSolver), RteItem::INCOMPATIBLE);
  EXPECT_EQ(denyDenyDependency->Evaluate(depSolver), RteItem::FULFILLED);

  // evaluate other condition  possibilities
  RteRequireExpression deviceExpression(nullptr);
  deviceExpression.AddAttribute("Dname", activeTarget->GetDeviceName());
  deviceExpression.ConstructID();
  EXPECT_EQ(deviceExpression.Evaluate(filterContext), RteItem::FULFILLED);
  EXPECT_EQ(deviceExpression.Evaluate(depSolver), RteItem::IGNORED);

  RteAcceptExpression componentExpression(nullptr);
  componentExpression.AddAttribute("Cclass", "MyClass");
  componentExpression.AddAttribute("Cgroup", "MyGroup");
  componentExpression.ConstructID();
  EXPECT_EQ(componentExpression.Evaluate(filterContext), RteItem::IGNORED);
  EXPECT_EQ(componentExpression.Evaluate(depSolver), RteItem::MISSING);

  RteAcceptExpression acceptExpression(nullptr);
  acceptExpression.AddAttribute("Unknown", "unknown");
  acceptExpression.ConstructID();
  EXPECT_EQ(acceptExpression.Evaluate(filterContext), RteItem::IGNORED);
  EXPECT_EQ(acceptExpression.Evaluate(depSolver), RteItem::IGNORED);

  RteDenyExpression denyExpression(nullptr);
  denyExpression.AddAttribute("Unknown", "unknown");
  denyExpression.ConstructID();
  EXPECT_EQ(denyExpression.Evaluate(filterContext), RteItem::IGNORED);
  EXPECT_EQ(denyExpression.Evaluate(depSolver), RteItem::IGNORED);
}
// end of RteConditionTest.cpp
