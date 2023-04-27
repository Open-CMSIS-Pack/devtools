
#include <math.h>

#include "RTE_Components.h"
#include CMSIS_device_header

int main (void) {
  float pi = 3.141492653;
  float f = fabs(sin(pi));
  return (int)f;
}
