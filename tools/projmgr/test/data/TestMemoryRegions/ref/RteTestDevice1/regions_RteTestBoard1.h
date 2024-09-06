#ifndef REGIONS_RTETESTBOARD1_H
#define REGIONS_RTETESTBOARD1_H


//-------- <<< Use Configuration Wizard in Context Menu >>> --------------------
//------ With VS Code: Open Preview for Configuration Wizard -------------------

// <n> Auto-generated using information from packs
// <i> Device Family Pack (DFP):   ARM::RteTest_DFP@0.8.0
// <i> Board Support Pack (BSP):   ARM::RteTest_BSP@0.9.0

// <h> ROM Configuration
// =======================
// <h> __ROM0 (is rx memory: Device_Rom1+Device_Rom2 from DFP)
//   <o> Base address <0x0-0xFFFFFFFF:8>
//   <i> Defines base address of memory region. Default: 0x01000000
//   <i> Contains Startup and Vector Table
#define __ROM0_BASE 0x01000000
//   <o> Region size [bytes] <0x0-0xFFFFFFFF:8>
//   <i> Defines size of memory region. Default: 0x01040000
#define __ROM0_SIZE 0x01040000
// </h>

// <h> __ROM1 (is rx memory: Flash-External0 from BSP)
//   <o> Base address <0x0-0xFFFFFFFF:8>
//   <i> Defines base address of memory region. Default: 0x70000000
#define __ROM1_BASE 0x70000000
//   <o> Region size [bytes] <0x0-0xFFFFFFFF:8>
//   <i> Defines size of memory region. Default: 0x04000000
#define __ROM1_SIZE 0x04000000
// </h>

// <h> __ROM2 (is rx memory: Flash-External1 from BSP)
//   <o> Base address <0x0-0xFFFFFFFF:8>
//   <i> Defines base address of memory region. Default: 0x71000000
#define __ROM2_BASE 0x71000000
//   <o> Region size [bytes] <0x0-0xFFFFFFFF:8>
//   <i> Defines size of memory region. Default: 0x04000000
#define __ROM2_SIZE 0x04000000
// </h>

// <h> __ROM3 (unused)
//   <o> Base address <0x0-0xFFFFFFFF:8>
//   <i> Defines base address of memory region.
#define __ROM3_BASE 0
//   <o> Region size [bytes] <0x0-0xFFFFFFFF:8>
//   <i> Defines size of memory region.
#define __ROM3_SIZE 0
// </h>

// </h>

// <h> RAM Configuration
// =======================
// <h> __RAM0 (is rwx memory: Device_Ram1+Device_Ram2 from DFP)
//   <o> Base address <0x0-0xFFFFFFFF:8>
//   <i> Defines base address of memory region. Default: 0x91000000
//   <i> Contains uninitialized RAM, Stack, and Heap
#define __RAM0_BASE 0x91000000
//   <o> Region size [bytes] <0x0-0xFFFFFFFF:8>
//   <i> Defines size of memory region. Default: 0x01040000
#define __RAM0_SIZE 0x01040000
// </h>

// <h> __RAM1 (is rwx memory: RAM-External0 from BSP)
//   <o> Base address <0x0-0xFFFFFFFF:8>
//   <i> Defines base address of memory region. Default: 0x60000000
#define __RAM1_BASE 0x60000000
//   <o> Region size [bytes] <0x0-0xFFFFFFFF:8>
//   <i> Defines size of memory region. Default: 0x00800000
#define __RAM1_SIZE 0x00800000
// </h>

// <h> __RAM2 (is rwx memory: RAM-External1 from BSP)
//   <o> Base address <0x0-0xFFFFFFFF:8>
//   <i> Defines base address of memory region. Default: 0x61000000
#define __RAM2_BASE 0x61000000
//   <o> Region size [bytes] <0x0-0xFFFFFFFF:8>
//   <i> Defines size of memory region. Default: 0x00800000
#define __RAM2_SIZE 0x00800000
// </h>

// <h> __RAM3 (unused)
//   <o> Base address <0x0-0xFFFFFFFF:8>
//   <i> Defines base address of memory region.
#define __RAM3_BASE 0
//   <o> Region size [bytes] <0x0-0xFFFFFFFF:8>
//   <i> Defines size of memory region.
#define __RAM3_SIZE 0
// </h>

// </h>

// <h> Stack / Heap Configuration
//   <o0> Stack Size (in Bytes) <0x0-0xFFFFFFFF:8>
//   <o1> Heap Size (in Bytes) <0x0-0xFFFFFFFF:8>
#define __STACK_SIZE 0x00000200
#define __HEAP_SIZE 0x00000C00
// </h>


#endif /* REGIONS_RTETESTBOARD1_H */
