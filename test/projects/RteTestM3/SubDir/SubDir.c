#include "RTE_Components.h"             // Component selection

#ifdef LOCAL_PRE_INCLUDE
#error local pre-include seen in SubDir.c outside of the component
#endif

#ifdef GLOBAL_PRE_INCLUDE
#warning global pre-include seen in non component module
#else
#error global pre-include is NOT seen in non component module
#endif

#ifndef RTE_COMPONENTS_H
#error RTE_COMPONENTS_H path?
#endif

int SubDirFunc (void) {
 return 0;
}
