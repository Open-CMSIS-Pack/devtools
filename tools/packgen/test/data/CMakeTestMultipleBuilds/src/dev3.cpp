#include <stdio.h>

#ifdef DEV3_DEF1
#error DEV3_DEF1 is defined!
#endif

#include "dev3.h"
#include "core.h"

#ifndef DEV3_DEF1
#error DEV3_DEF1 is not defined!
#endif

int dev3 (void)
{
  return 0;
}
