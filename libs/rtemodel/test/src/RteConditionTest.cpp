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

TEST_F(RteConditionTest, MissingIgnoredFulfilled) {
  // load project to get a working target and condition contexts
  RteKernelSlim rteKernel;
  rteKernel.SetCmsisPackRoot(RteModelTestConfig::CMSIS_PACK_ROOT);
  RteCprjProject* loadedCprjProject = rteKernel.LoadCprj(RteTestM3_cprj);
  ASSERT_NE(loadedCprjProject, nullptr);
  RteTarget* activeTarget = loadedCprjProject->GetActiveTarget();
  ASSERT_NE(activeTarget, nullptr);
  RteConditionContext* filterContext = activeTarget->GetFilterContext();
  ASSERT_NE(filterContext, nullptr);

  RteDependencySolver* depSolver = activeTarget->GetDependencySolver();
  ASSERT_NE(depSolver, nullptr);

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
