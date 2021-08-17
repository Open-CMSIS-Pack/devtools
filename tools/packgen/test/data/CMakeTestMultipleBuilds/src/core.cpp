#include <stdio.h>

#ifdef CORE_DEF1
#error CORE_DEF1 is defined!
#endif

#include "core.h"

#ifndef CORE_DEF1
#error CORE_DEF1 is not defined!
#endif

int core (void)
{
  return 0;
}
