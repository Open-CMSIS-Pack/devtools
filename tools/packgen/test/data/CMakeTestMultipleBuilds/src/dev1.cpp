#include <stdio.h>

#ifdef DEV1_DEF1
#error DEV1_DEF1 is defined!
#endif

#include "dev1.h"
#include "core.h"

#ifndef DEV1_DEF1
#error DEV1_DEF1 is not defined!
#endif

int dev1 (void)
{
  return 0;
}
