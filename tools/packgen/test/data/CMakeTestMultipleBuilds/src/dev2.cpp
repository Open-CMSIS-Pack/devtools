#include <stdio.h>

#ifdef DEV2_DEF1
#error DEV2_DEF1 is defined!
#endif

#include "dev2.h"
#include "core.h"

#ifndef DEV2_DEF1
#error DEV2_DEF1 is not defined!
#endif

int dev2 (void)
{
  return 0;
}
