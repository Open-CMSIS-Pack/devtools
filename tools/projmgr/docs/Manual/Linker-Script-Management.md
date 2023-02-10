# Linker Script Management

<!-- markdownlint-disable MD009 -->
<!-- markdownlint-disable MD013 -->
<!-- markdownlint-disable MD036 -->

**Table of Contents**

- [Linker Script Management](#linker-script-management)
  - [Overview](#overview)
  - [Linker Script Preprocessing](#linker-script-preprocessing)
  - [Automatic Linker Script generation](#automatic-linker-script-generation)
    - [File locations](#file-locations)
    - [User Modifications to Memory Regions](#user-modifications-to-memory-regions)
    - [Linker Script Templates](#linker-script-templates)

## Overview

A Linker Script contains a series of Linker directives that specify the available memory and how it should be used by a project. The Linker directives reflect exactly the available memory resources and memory map for the project context.

The following sequence describes the Linker Script management of the **csolution - CMSIS Project Manager**:

1. The [`linker:`](YML-Input-Format.md#linker) node specifies an explicit linker script and/or memory regions header file. This overrules linker scripts that are part of software components or specified using the `file:` notation.

2. If no [`linker:`](YML-Input-Format.md#linker) node is used, a linker script file can be provided using the `file:` notation under [`groups:`](YML-Input-Format.md#groups) or as part of software components. The extensions `.sct`, `.scf`, `.ld`, and `.icf` are recognized as Linker Script files.

3. If no Linker script is found, a [Linker Script is generated](#automatic-linker-script-generation) based on information that is provided by the `<memory>` element in Device Family Packs (DFP) and Board Support Packs (BSP).
 
## Linker Script Preprocessing

A Linker Script file is preprocessed when a `regions:` header file is specified in the [`linker:`](YML-Input-Format.md#linker) node or when the [Linker Script file is automatically generated](#automatic-linker-script-generation). A standard C preprocessor is used to create the final linker script as shown below.

![Linker Script File Generation](./images/linker-script-file.png "Linker Script File Generation")

## Automatic Linker Script generation

If a project context does not specify any linker script a `regions_<device_or_board>.h` is generated and a toolchain specific linker script template is used.

If `regions_<device_or_board>.h` is **not** available, it is generated based on information of the software packs using the:

- [`<device>` - `<memory>` element in the DFP](https://open-cmsis-pack.github.io/Open-CMSIS-Pack-Spec/main/html/pdsc_boards_pg.html#element_board_memory)
- [`<board>` - `<memory>` element in the BSP](https://open-cmsis-pack.github.io/Open-CMSIS-Pack-Spec/main/html/pdsc_family_pg.html#element_memory)

### File locations

The file `regions_<device_or_board>.h` and the Linker Script file should be generated in the [RTE directory](Overview.md#rte-directory-structure) path `\RTE\Device\<device>`. The actual file name is extended with:

- `Bname` when the `*.cproject.yml` file uses in the project context a [`board:`](YML-Input-Format.md#board-name-conventions) specification, i.e. `regions_IMXRT1050-EVKB.h`
- `Dname` name when the `*.cproject.yml` file uses in the project context only a [`device:`](YML-Input-Format.md#device-name-conventions) specification, i.e. `regions_stm32u585xx.h`.
  
### User Modifications to Memory Regions

The file `regions_<device_or_board>.h` can be modified by the user as it might be required to adjust the memory regions or give additional attributes (such as `noinit`).  Therefore this file should has [Configuration Wizard Annotations](https://open-cmsis-pack.github.io/Open-CMSIS-Pack-Spec/main/html/configWizard.html).

### Linker Script Templates

The following compiler specific Linker Script files are used when no explicit file is specified.  The files are located in the directory `<cmsis-toolbox-installation-dir>/.etc` of the CMSIS-Toolbox.

Linker Script Template  | Linker control file for ...
:-----------------------|:-----------------------------
ac6_linker_script.sct   | Arm Compiler version 6
gcc_linker_script.ld    | GCC Compiler
iar_linker_script.icf   | IAR Compiler
