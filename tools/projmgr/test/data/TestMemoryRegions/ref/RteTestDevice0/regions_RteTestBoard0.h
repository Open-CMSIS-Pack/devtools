#ifndef REGIONS_RTETESTBOARD0_H
#define REGIONS_RTETESTBOARD0_H


//-------- <<< Use Configuration Wizard in Context Menu >>> --------------------
//------ With VS Code: Open Preview for Configuration Wizard -------------------

// <n> Auto-generated using information from packs
// <i> Device Family Pack (DFP):   ARM::RteTest_DFP@0.8.0
// <i> Board Support Pack (BSP):   ARM::RteTest_BSP@0.9.0

// <h> ROM Configuration
// =======================
// <h> __ROM0 (is rx memory: IROM2 from DFP)
//   <o> Base address <0x0-0xFFFFFFFF:8>
//   <i> Defines base address of memory region. Default: 0x02000000
//   <i> Contains Startup and Vector Table
#define __ROM0_BASE 0x02000000
//   <o> Region size [bytes] <0x0-0xFFFFFFFF:8>
//   <i> Defines size of memory region. Default: 0x00040000
#define __ROM0_SIZE 0x00040000
// </h>

// <h> __ROM1 (is rx memory: IROM1 from DFP)
//   <o> Base address <0x0-0xFFFFFFFF:8>
//   <i> Defines base address of memory region. Default: 0x01000000
#define __ROM1_BASE 0x01000000
//   <o> Region size [bytes] <0x0-0xFFFFFFFF:8>
//   <i> Defines size of memory region. Default: 0x00040000
#define __ROM1_SIZE 0x00040000
// </h>

// <h> __ROM2 (is rx memory: IROM3 from DFP)
//   <o> Base address <0x0-0xFFFFFFFF:8>
//   <i> Defines base address of memory region. Default: 0x03000000
#define __ROM2_BASE 0x03000000
//   <o> Region size [bytes] <0x0-0xFFFFFFFF:8>
//   <i> Defines size of memory region. Default: 0x00040000
#define __ROM2_SIZE 0x00040000
// </h>

// <h> __ROM3 (is rx memory: IROM4+IROM5 from DFP)
//   <o> Base address <0x0-0xFFFFFFFF:8>
//   <i> Defines base address of memory region. Default: 0x04000000
#define __ROM3_BASE 0x04000000
//   <o> Region size [bytes] <0x0-0xFFFFFFFF:8>
//   <i> Defines size of memory region. Default: 0x01000000
#define __ROM3_SIZE 0x01000000
// </h>

// </h>

// <h> RAM Configuration
// =======================
// <h> __RAM0 (is rw memory: IRAM2 from DFP)
//   <o> Base address <0x0-0xFFFFFFFF:8>
//   <i> Defines base address of memory region. Default: 0x82000000
//   <i> Contains uninitialized RAM, Stack, and Heap
#define __RAM0_BASE 0x82000000
//   <o> Region size [bytes] <0x0-0xFFFFFFFF:8>
//   <i> Defines size of memory region. Default: 0x00020000
#define __RAM0_SIZE 0x00020000
// </h>

// <h> __RAM1 (is rw memory: IRAM1 from DFP)
//   <o> Base address <0x0-0xFFFFFFFF:8>
//   <i> Defines base address of memory region. Default: 0x81000000
#define __RAM1_BASE 0x81000000
//   <o> Region size [bytes] <0x0-0xFFFFFFFF:8>
//   <i> Defines size of memory region. Default: 0x00020000
#define __RAM1_SIZE 0x00020000
// </h>

// <h> __RAM2 (is rw memory: IRAM3 from DFP)
//   <o> Base address <0x0-0xFFFFFFFF:8>
//   <i> Defines base address of memory region. Default: 0x83000000
#define __RAM2_BASE 0x83000000
//   <o> Region size [bytes] <0x0-0xFFFFFFFF:8>
//   <i> Defines size of memory region. Default: 0x00020000
#define __RAM2_SIZE 0x00020000
// </h>

// <h> __RAM3 (is rw memory: IRAM4+IRAM5 from DFP)
//   <o> Base address <0x0-0xFFFFFFFF:8>
//   <i> Defines base address of memory region. Default: 0x84000000
#define __RAM3_BASE 0x84000000
//   <o> Region size [bytes] <0x0-0xFFFFFFFF:8>
//   <i> Defines size of memory region. Default: 0x01020000
#define __RAM3_SIZE 0x01020000
// </h>

// </h>

// <h> Stack / Heap Configuration
//   <o0> Stack Size (in Bytes) <0x0-0xFFFFFFFF:8>
//   <o1> Heap Size (in Bytes) <0x0-0xFFFFFFFF:8>
#define __STACK_SIZE 0x00000200
#define __HEAP_SIZE 0x00000C00
// </h>

// <n> Resources that are not allocated to linker regions
// <i> rx ROM:   Default0_Region from DFP:  BASE: 0x10000000  SIZE: 0x00040000
// <i> rx ROM:   Flash-External0 from BSP:  BASE: 0x70000000  SIZE: 0x04000000
// <i> rwx RAM:  RAM-External0 from BSP:    BASE: 0x60000000  SIZE: 0x00800000
// <i> rx ROM:   Flash-External1 from BSP:  BASE: 0x71000000  SIZE: 0x04000000
// <i> rwx RAM:  RAM-External1 from BSP:    BASE: 0x61000000  SIZE: 0x00800000


#endif /* REGIONS_RTETESTBOARD0_H */
