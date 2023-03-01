
#include "RTE_Components.h"             // Component selection
#include CMSIS_device_header            // Device header

#include "File.h"
#ifndef HEADER
#error "HEADER is not defined!"
#endif

#include "File1.h"
#ifndef HEADER1
#error "HEADER1 is not defined!"
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

#ifndef DEF1GROUP
#error "DEF1GROUP is not defined!"
#endif

#ifndef DEF1FILE
#error "DEF1FILE is not defined!"
#endif

#ifndef DEF2FILE
#error "DEF2FILE is not defined!"
#endif

#ifdef DEF2GROUP
#error "DEF2GROUP not defined!"
#endif

int main (void) {
  return 1;
}
