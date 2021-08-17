#include <stdio.h>

#include "cmsis_os2.h"

void thread_func(void *argument) {
  for (int itr = 1; itr < 20; ++itr) {
    printf("Hello world : %d\n\r", itr);
  }
}

int main(void) {
  osThreadNew(thread_func, NULL, NULL);
  if (osKernelReady == osKernelGetState()) {
    osKernelStart();
  }

  while (1);
}

