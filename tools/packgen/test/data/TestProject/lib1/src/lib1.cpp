#include <stdio.h>

#ifdef LIB_DEF
#error LIB_DEF is defined!
#endif

#include "lib1.h"

#ifndef LIB1_DEF
#error LIB1_DEF is not defined!
#endif

int lib1 (void)
{
  return 0;
}
