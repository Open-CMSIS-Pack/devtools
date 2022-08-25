#include "RTE_Components.h"
#include CMSIS_device_header

extern int Dummy1(int i);
extern int Dummy2(int i);

int main (void) {
  Dummy1(0);
  Dummy2(0);
  return 1;
}
