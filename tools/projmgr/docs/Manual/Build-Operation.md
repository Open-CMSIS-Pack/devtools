# Build Operation

<!-- markdownlint-disable MD009 -->
<!-- markdownlint-disable MD013 -->
<!-- markdownlint-disable MD036 -->

The following chapter explains the overall build process that of the CMSIS-Toolbox and how to add a new compiler toolchain.

**Table of Contents**

- [Build Operation](#build-operation)
  - [Build Process Overview](#build-process-overview)
    - [`csolution` tool](#csolution-tool)
    - [`cbuild` tool](#cbuild-tool)
  - [Add Toolchain to CMSIS-Toolbox](#add-toolchain-to-cmsis-toolbox)
    - [Steps](#steps)
    - [CMake Variables](#cmake-variables)
      - [`BRANCHPROT` Values](#branchprot-values)

## Build Process Overview

The section below describes the overall process to add a new compiler toolchain to the CMSIS-Toolbox. 

### `csolution` tool

The [**csolution - CMSIS Project Manager**](Overview.md) processes user [**Input Files** (in YAML format)](YML-Input-Format.md) and the `*.pdsc` metadata files of **Software Packs** and performs the following operations:

- Generate build information in the [**Project Area**](Overview.md#project-area) with the following files: [`*.cbuild-idx.yml`, `*.cbuild.yml`](YML-CBuild-Format.md), and `*.cprj` files for the **cbuild** tool.
- Generate header files in the [**RTE Directory**](Overview.md#rte-directory-structure) for each [context](YML-Input-Format.md#context) with the following files: [RTE_components.h](Overview.md#rte_componentsh) and pre-include files from the `*.pdsc` metadata.
- [Copy the configuration files](Overview.md#plm-of-configuration-files) from selected software components to the [**RTE Directory**](Overview.md#rte-directory-structure).

The picture below outlines these operations.

![Operation of `csolution` tool](./images/csolution-operation.png "Operation of `csolution` tool")

### `cbuild` tool

The [**cbuild - Build Manager**](../../../buildmgr/docs/cbuild.md) uses a `*.cprj` build information files to perform the following operations:

- Generate a `CMakeList.txt` file that includes a `compiler_name.<version>.cmake` file for toolchain specific configuration. This file contains the complete build information for a `*.cproject.yml` file and describes how to build a binary image or a library file.
- This `CMakeList.txt` file is then processed by the `CMake` tool to generate the `build.ninja` file with the actual build commands.
- This `build.ninja` file is then used by the `Ninja` tool to generate the binary image or a library file with the selected toolchain.

The picture below outlines these operations.

![Operation of `csolution` tool](./images/cbuild-operation.png "Operation of `csolution` tool")

## Add Toolchain to CMSIS-Toolbox

The following section explains how to add a compiler toolchain to the CMSIS-Toolbox.

### Steps

The section below describes the steps to add a new compiler toolchain to the CMSIS-Toolbox. 

1. Define a `compiler_name` for the new compiler toolchain, i.e. `CLang`.
2. Add this `compiler_name` to the `"CompilerType":` in the schema file [`./tools/projmgr/schemas/common.schema.json`](https://github.com/Open-CMSIS-Pack/devtools/blob/main/tools/projmgr/schemas/common.schema.json).
3. Create a new CMake file in [`./tools/buildmgr/cbuildgen/config`](https://github.com/Open-CMSIS-Pack/devtools/tree/main/tools/buildmgr/cbuildgen/config) with the naming convention `compiler_name.<version>.cmake`.
4. Map with the file `compiler_name.<version>.cmake`. the **CMake** input variables to the **CMake** toolchain variables.
   - Use an existing `*.cmake` file, i.e. `GCC.<version>.cmake` as reference.
 
### CMake Variables

The `CMakeLists.txt` file sets the following **CMake** input variables that should be processed by `compiler_name.<version>.cmake`.

CMake Variable                                   | Possible Values           | Description
:------------------------------------------------|:--------------------------|:-----------------------
`BYTE_ORDER`                                     | Little-endian, Big-endian | [Endian processor configuration](https://open-cmsis-pack.github.io/Open-CMSIS-Pack-Spec/main/html/pdsc_family_pg.html#Dendian)
`CPU`                                            | [DCoreEnum](https://open-cmsis-pack.github.io/Open-CMSIS-Pack-Spec/main/html/pdsc_family_pg.html#DcoreEnum)      | Processor core selection
`FPU`                                            | [DfpuEnum](https://open-cmsis-pack.github.io/Open-CMSIS-Pack-Spec/main/html/pdsc_family_pg.html#DfpuEnum)        | Floating point unit support
`DSP`                                            | [DdspEnum](https://open-cmsis-pack.github.io/Open-CMSIS-Pack-Spec/main/html/pdsc_family_pg.html#DdspEnum)        | DSP instruction set support
`TZ`                                             | [DtzEnum](https://open-cmsis-pack.github.io/Open-CMSIS-Pack-Spec/main/html/pdsc_family_pg.html#DtzEnum)          | TrustZone support
`SECURE`                                         | [DsecureEnum](https://open-cmsis-pack.github.io/Open-CMSIS-Pack-Spec/main/html/pdsc_family_pg.html#DsecureEnum)  | Software model selection
`MVE`                                            | [DmveEnum](https://open-cmsis-pack.github.io/Open-CMSIS-Pack-Spec/main/html/pdsc_family_pg.html#DmveEnum)        | MVE instruction set support
`BRANCHPROT`                                     | [`BRANCHPROT` values](#branchprot-values)  | [Branch protection
`OPTIMIZE`                                       | [Optimize values](YML-Input-Format.md#optimize)  | Generic optimize levels for code generation
`WARNINGS`                                       | [Warnings values](YML-Input-Format.md#warnings)  | Control warning level for compiler diagnostic
`DEBUG`                                          | [Debug values](YML-Input-Format.md#debug)        | Control the generation of debug information
`DEFINES`                                        | [Define symbols](YML-Input-Format.md#define)     | List of symbol #define statements

#### `BRANCHPROT` Values

The following table lists the possible values for the CMake variable `BRANCHPROT`.

Values        |	Description
--------------|------------------------------
NO_BRANCHPROT |	Branch protection not used
BTI	          | Using BTI (Branch Target ID)
BTI_SIGNRET   | Using	BTI + Sign Return


The `compiler_name.<version>.cmake` sets the following **CMake** variables to specify the toolchain and select toolchain options.

CMake Variable                                   | Description
:------------------------------------------------|:-----------------------
`ASM_CPU`, `CC_CPU`, `CXX_CPU`                   | Device selection set according to the combination of device attributes (`CPU`, `FPU`, `DSP`, `MVE`, etc.)
`AS_LEG_CPU`, `AS_ARM_CPU`, `AS_GNU_CPU`         | Similar to the previous item but for assembly dialect variants (if applicable)
`ASM_FLAGS`, `CC_FLAGS`, `CXX_FLAGS`, `LD_FLAGS` | Flags applicable to all modules of the given language
`CC_SECURE`, `LD_SECURE`                         | Flags applicable only for secure projects
`_PI`                                            | Pre-include option
`_ISYS`                                          | system include option
`LIB_PREFIX`                                     | Generated library name prefix
`LIB_SUFFIX`                                     | Generated library name suffix
`EXE_SUFFIX`                                     | Generated executable name suffix
`ELF2HEX`                                        | Flags for ELF to HEX conversion
`ELF2BIN`                                        | Flags for ELF to BIN conversion
`CMAKE_C_COMPILER_ID`                            | CMake compiler identifier
`CMAKE_C_COMPILER_VERSION`                       | CMake compiler version
     