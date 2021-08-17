
 #include "RTE_Components.h"             // Component selection
 #include "Include/RelativeInclude.h"    // ARM::Other:GlobalLevel

int main (void) {

#ifdef LOCAL_PRE_INCLUDE
#error local pre-include seen in main.c outside of the component
#endif

#ifdef GLOBAL_PRE_INCLUDE
#warning global pre-include seen in non component module
#endif

#ifndef RTE_COMPONENTS_H
#error RTE_COMPONENTS_H path?
#endif

  while (1)
  {
  }
}

