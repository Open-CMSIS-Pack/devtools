
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

#ifndef DEF1-GROUP
#error "DEF1-GROUP is not defined!"
#endif

#ifndef DEF1-FILE
#error "DEF1-FILE is not defined!"
#endif

#ifndef DEF2-FILE
#error "DEF2-FILE is not defined!"
#endif

#ifdef DEF3
#error "DEF3 is defined!"
#endif

#ifdef DEF2-GROUP
#error "DEF2-GROUP is not defined!"
#endif

int main (void) {
  return 1;
}
