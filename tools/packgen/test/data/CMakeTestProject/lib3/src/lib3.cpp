#include <stdio.h>

#ifdef LIB3_DEF1
#error LIB3_DEF1 is defined!
#endif

#include "lib3.h"

#ifndef LIB3_DEF1
#error LIB3_DEF1 is not defined!
#endif

int lib3 (void)
{
  return 0;
}
