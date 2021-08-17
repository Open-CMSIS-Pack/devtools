#include <stdio.h>

#ifdef LIB5_DEF1
#error LIB5_DEF1 is defined!
#endif

#include "lib5.h"

#ifndef LIB5_DEF1
#error LIB5_DEF1 is not defined!
#endif

int lib5 (void)
{
  return 0;
}
