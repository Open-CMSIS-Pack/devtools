
#include "RTE_Components.h"             // Component selection
#include CMSIS_device_header            // Device header

#include "File.h"
#ifndef HEADER
#error "HEADER is not defined!"
#endif

#include "File2.h"
#ifndef HEADER2
#error "HEADER2 is not defined!"
#endif

#ifndef DEF1
#error "DEF1 is not defined!"
#endif

#ifndef DEF2
#error "DEF2 is not defined!"
#endif

#if (DEF2 != 2)
#error "DEF2 is not equal to 2!"
#endif

int main (void) {
  return 1;
}

