
#include "RTE_Components.h"             // Component selection
#include CMSIS_device_header            // Device header

#include "File1.h"
#ifndef HEADER1
#error "HEADER1 is not defined!"
#endif

#include "file2.h"
#ifndef header2
#error "header2 is not defined!"
#endif

#ifndef DEF1
#error "DEF1 is not defined!"
#endif

#ifdef DEF2
#error "DEF2 is defined!"
#endif

#ifndef DEF3
#error "DEF3 is not defined!"
#endif

#ifndef DEF3FILE
#error "DEF3FILE is not defined!"
#endif
