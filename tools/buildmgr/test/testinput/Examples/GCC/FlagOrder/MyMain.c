
#include "RTE_Components.h"
#include CMSIS_device_header

#include "test_order.h"

#if TEST_ORDER_VALUE != 'b'
  #error Included test_order.h in the wrong order
#endif

int main (void) {
  return 1;
}
