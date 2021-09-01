#include <stdio.h>

#ifdef LIB2_DEF1
#error LIB2_DEF1 is defined!
#endif

#include "lib2.h"

#ifndef LIB2_DEF1
#error LIB2_DEF1 is not defined!
#endif

int lib2 (void)
{
  return 0;
}
