#include <stdio.h>

#ifdef LIB1_DEF1
#error LIB1_DEF1 is defined!
#endif

#include "lib1.h"

#ifndef LIB1_DEF1
#error LIB1_DEF1 is not defined!
#endif

int lib1 (void)
{
  return 0;
}
