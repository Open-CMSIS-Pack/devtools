/*----------------------------------------------------------------------------
 * CMSIS bare-metal 'main' function template
 * Copyright (c) 2022: Arm Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *---------------------------------------------------------------------------*/
 
#include "RTE_Components.h"          // component selection 
#include  CMSIS_device_header        // device header file
 
 
int main (void) {
 
  // System Initialization
  SystemCoreClockUpdate();
  // ... other initalizations

  // Application code starts here
  for (;;) {
    // ...
	}
}
