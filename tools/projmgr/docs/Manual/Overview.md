# csolution: CMSIS Project Manager (Users Manual - Draft)

<!-- markdownlint-disable MD013 -->
<!-- markdownlint-disable MD036 -->

>**Important Note:**
> While the CMSIS-Toolbox is already version 1.0.0 or higher, several aspects of the tools are still under review.  The sections that are market with `Proposal` are features that are not implemented or subject to change in the final implementation.  All other features are considered as stable and changes that are not upward compatible are avoided whenever possible.

Manual Chapters                           | Content
:-----------------------------------------|:-------------------------
[Usage](#usage)                           | Overall Concept, tool setup, and invocation commands
[Project Examples](#project-examples)     | Various example projects to get started
[Project Structure](#project-structure)   | Overall structure of projects
[YML Input Format](YML-Input-Format.md)   | Format of the various YML input files (csolution, cproject, clayer, cdefault)
[YML CBuild Format](YML-CBuild-Format.md) | Format of the YML CBuild output file.

**Table of Contents**

- [csolution: CMSIS Project Manager (Users Manual - Draft)](#csolution-cmsis-project-manager-users-manual---draft)
  - [Revision History](#revision-history)
  - [Overview of Operation](#overview-of-operation)
  - [Usage](#usage)
    - [Requirements](#requirements)
    - [Invocation](#invocation)
    - [Command Examples](#command-examples)
  - [Project Examples](#project-examples)
    - [Minimal Project Setup](#minimal-project-setup)
    - [Software Layers](#software-layers)
    - [Project Setup for Multiple Targets and Builds](#project-setup-for-multiple-targets-and-builds)
    - [Project Setup for Related Projects](#project-setup-for-related-projects)
  - [Project Structure](#project-structure)
    - [RTE Directory Structure](#rte-directory-structure)
      - [Proposal](#proposal)
    - [Output Directory Structure](#output-directory-structure)
    - [Software Components](#software-components)
    - [PLM of Configuration Files](#plm-of-configuration-files)
    - [RTE_Components.h](#rte_componentsh)
  - [Proposals](#proposals)
    - [CMSIS-Zone Integration](#cmsis-zone-integration)
    - [CMSIS-Pack extensions](#cmsis-pack-extensions)
      - [Layers in packs](#layers-in-packs)

## Revision History

Version            | Description
:------------------|:-------------------------
Draft              | Work in progress

## Overview of Operation

The **csolution - CMSIS Project Manager** processes **user input files** (in YML format) and **software packs**
(in Open-CMSIS-Pack format) to create self-contained CMSIS-Build input files that allow to generate independent projects
which may be a part of a more complex application.

The **csolution - CMSIS Project Manager** supports the user with the following features:

- Access to the content of software packs in Open-CMSIS-Pack format to:
  - Setup the tool chain based on a *Device* or *Board* that is defined in the CMSIS-Packs.
  - Add software components that are provided in the various software packs to the application.
- Organize applications (with a `*.csolution.yml` file) into projects that are independently managed
  (using `*.cproject.yml` files).
- Manage the resources (memory, peripherals, and user defined) across the entire application to:
  - Partition the resources of the system and create related system and linker configuration.
  - Support in the configuration of software stacks (such as RTOS threads).
  - Hint the user for inclusion of software layers that are pre-configured for typical use cases.
- Organize software layers (with a `*.clayer.yml` file) that enable code reuse across similar applications.
- Manage multiple hardware targets to allow application deployment to different hardware
  (test board, production hardware, etc.).
- Manage multiple build types to support software verification (debug build, test build, release build, ect.)
- Support multiple compiler toolchains (GCC, LLVM, Arm Compiler 6, IAR, etc.) for project deployment.

> **Note:** The **csolution - CMSIS Project Manager** is currently under development and part of the
  **[Open-CMSIS-Pack](https://www.open-cmsis-pack.org/index.html)** open source project.

![Overview](./images/Overview.png "Overview")

This picture above outlines the operation. The **CMSIS Project Manager** uses the following files.

Input Files              | Description
:------------------------|:---------------------------------
[Generic Software Packs](https://open-cmsis-pack.github.io/Open-CMSIS-Pack-Spec/main/html/cp_PackTutorial.html#cp_SWComponents) | Provide re-usable software components that are typically configurable  towards a user application.
[DFP Software Packs](https://open-cmsis-pack.github.io/Open-CMSIS-Pack-Spec/main/html/cp_PackTutorial.html#createPack_DFP)     | Device related information on the tool configuration. May refer an *.rzone file.
[BSP Software Packs](https://open-cmsis-pack.github.io/Open-CMSIS-Pack-Spec/main/html/cp_PackTutorial.html#createPackBoard)    | Board specific configuration (i.e. memory). May refer to an *.rzone file that defines board components.
[*.rzone files](https://arm-software.github.io/CMSIS_5/Zone/html/xml_rzone_pg.html)                 | Definition of memory and peripheral resources. If it does not exist, content is created from DFP.
*.cdefault.yml           | **Step 1:** setup of an environment (could be an IDE) to pre-define a toolchain.
*.csolution.yml          | **Step 2:** complete scope of the application with build order of sub-projects. Defines target and build types.
*.cproject.yml           | **Step 3:** content of an independent build (linker run) - directly relates to a `*.cprj` file.
*.clayer.yml             | **Step 4:** set of source files along with pre-configured components for reuse in different applications.

> **Note:** **Steps 1 - 4** indicate the order of processing of the user input files.

Output Files             | Description
:------------------------|:---------------------------------
[Project Build Files *.cprj](https://arm-software.github.io/CMSIS_5/Build/html/cprjFormat_pg.html) | Project build information for a Open-CMSIS-Pack based tool environment.
Run-Time Environment (RTE)  | Contains the user configured files of a project along with RTE_Components.h inventory file.
[Project Resource Files *.fzone](https://arm-software.github.io/CMSIS_5/Zone/html/GenDataModel.html)     | Resource and partition data structure for template based code generators.

## Usage

### Requirements

The CMSIS-Pack repository must be present in the development environment.

- There are several ways to initialize and configure the pack repository, for example using the
  `cpackget` tool available from <https://github.com/Open-CMSIS-Pack/cpackget>
- Before running `csolution` the location of the pack repository shall be set via the environment variable
  `CMSIS_PACK_ROOT` otherwise its default location (todo what is the default?) will be taken.

### Invocation

```text
CMSIS Project Manager 1.3.0 (C) 2022 Arm Ltd. and Contributors

Usage:
  csolution [-V] [--version] [-h] [--help]
            <command> [<arg>] [OPTIONS...]

 Commands:
   list packs            Print list of used packs from the pack repository
        boards           Print list of available board names
        devices          Print list of available device names
        components       Print list of available components
        dependencies     Print list of unresolved project dependencies
        contexts         Print list of contexts in a csolution.yml
        generators       Print list of code generators of a given context
   convert               Convert *.csolution.yml input file in *.cprj files
   run                   Run code generator

 Options:
   -s, --solution arg    Input csolution.yml file
   -c, --context arg     Input context name <cproject>[.<build-type>][+<target-type>]
   -f, --filter arg      Filter words
   -g, --generator arg   Code generator identifier
   -m, --missing         List only required packs that are missing in the pack repository
   -l, --load arg        Set policy for packs loading [latest|all|required]
   -e, --export arg      Set suffix for exporting <context><suffix>.cprj retaining only specified versions
   -n, --no-check-schema Skip schema check
   -o, --output arg      Output directory
```

### Command Examples

Print list of installed packs. The list can be filtered by words provided with the option `--filter`:

```text
csolution list packs [--filter "<filter words>"]
```

Print list of packs that are required by the `<example.csolution.yml>`.

```text
csolution list packs -s <example.csolution.yml>
```

Print list of missing packs to the file `packs.txt` that are required by the `<example.csolution.yml>` but not available
in the pack repository. This missing packs might be installed using the
[`cpackget`](../../../cpackget/docs/cpackget.md) tool.

```text
csolution list packs -s <example.csolution.yml> -m >packs.txt
cpackget add -f packs.txt
```

Print list of available device names. The list can be filtered by words provided with the option `--filter`:

```text
csolution list devices [--filter "<filter words>"]
```

Print list of unresolved project dependencies. Device, board, and software components are specified as part of the
`*.csolution.yml` and `*.cproject.yml` files. The list may be filtered by words provided with the option `--filter`:

```text
csolution list dependencies -s mysolution.csolution.yml [-f "<filter words>"]
```

Convert `example.csolution.yml` into *.cprj file(s):

```text
csolution convert -s example.csolution.yml
```

List external code generators that are used to create software components in `*.gpdsc` format. It outputs the generator
ID that is required for the `run` command.

```text
csolution list generators -s mysolution.csolution.yml
```

Run a generator (in this case STCubeMX) for a specific project context.

```text
csolution run -g STCubeMX -s mysolution.csolution.yml -c Blinky.Debug+STM32L4
```

## Project Examples

The repository [csolution-examples](https://github.com/Open-CMSIS-Pack/csolution-examples) provides several working examples.  

The section below explains the the overall concepts consider by the `csolution` tool based on examples.

### Minimal Project Setup

A minimal application requires two files:

- `Sample.csolution.yml` that defines the device and selects the compiler.
- `Sample.cproject.yml` that defines the files and software components that belong to the device.

Simple applications require just one self-contained file.

**Simple Project: `Sample.csolution.yml`**

```yml
solution:
  compiler: AC6
  misc:
    - C:
      - -std=c99
    - ASM:
      - -masm=auto
    - Link:
      - --info summarysizes

  packs:
    - pack: ARM::CMSIS
    - pack: Keil::LPC1700_DFP

  target-types:
    - type: MyHardware
      device: NXP::LPC1768

  projects:
    - project: ./Sample.cproject.yml
```

**Simple Project: `Sample.cproject.yml`**

```yml
project:
  groups:
    - group: App
      files:
        - file: ./main.c

  components:
    - component: ARM::CMSIS:CORE
    - component: Device:Startup
```

### Software Layers

Software layers collect source files and software components along with configuration files for re-use in different projects as shown in the picture below. There are two ways to share configuration settings:

- By default, the configuration files of a `layer` are store within the directory structure of the software layer. Projects can therefore share a `layer` with common configuration settings.

- With [RTE directory settings](./YML-Input-Format.md#rte-dirs) that are specific to software component `Cclass` names it is possible partly share a common configuration across layers.

![Project Layers](./images/Layers.png "Project Layers")

An application could be composed of various layers, for example to compose an IoT cloud application:

- **Demo.cproject.yml**: Implements the IoT Reference example.
- **Socket.clayer.yml**: A software layer that provides the Socket interface for internet connectivity.
- **Board.clayer.yml**: A software layer that provides the hardware interfaces to the device hardware.

**Example:**

The project [AWS_MQTT_MutualAuth_SW_Framework](https://github.com/Open-CMSIS-Pack/AWS_MQTT_MutualAuth_SW_Framework) provides an example for software layers.

### Project Setup for Multiple Targets and Builds

Complex examples require frequently slightly different targets and/or modifications during build, i.e. for testing. The
picture below shows a setup during software development that supports:

- Unit/Integration Testing on simulation models (called Virtual Hardware) where Virtual Drivers implement the interface
  to simulated I/O.
- Unit/Integration Testing the same software setup on a physical board where Hardware Drivers implement the interface to
  physical I/O.
- System Testing where the software is combined with more software components that compose the final application.

![Target and Build Types](./images/TargetBuild-Types.png "Target and Build Types")

As the software may share a large set of common files, provisions are required to manage such projects. The common way
in other IDE's is to add:

- **target-types** that select a target system. In the example this would be:
  - `Virtual`: for Simulation Models.
  - `Board`: for a physical evaluation board.
  - `Production-HW`: for system integration test and the final product delivery.
- **build-types** add the flexibility to configure each target build towards a specific testing. It might be:
  - `Debug`: for a full debug build of the software for interactive debug.
  - `Test`: for a specific timing test using a test interface with code maximal optimization.
  - `Release`: for the final code deployment to the systems.

It is required to generate reproducible builds that can deployed on independent CI/CD test systems. To achieve that, the
CMSIS Project Manager generates *.cprj output files with the following naming conventions:

`<projectname>[.<build-type>][+target-type].cprj` this would generate for example: `Multi.Debug+Production-HW.cprj`

This enables that each target and/or build type can be identified and independently generated which provides the support
for test automation. It is however not required to build every possible combination, this should be under user control.

**Flexible Builds for multi-target projects**

Currently multi-target projects require the setup of a `*.csolution.yml` file to define `target-types` and
`build-types`. Note, that this is currently under review, but this documents the current status.

**File: MultiTarget.csolution.yml**

```yml
solution:
  compiler: AC6

  target-types:
    - type: Board
      board: NUCLEO-L552ZE-Q

    - type: Production-HW
      device: STM32L552XY         # specifies device

    - type: Virtual
      board: VHT-Corstone-300     # Virtual Hardware platform (appears as board)
      
  build-types:
    - type: Debug
      optimize: none
      debug: on

    - type: Test
      optimize: size
      debug: on

    - type: Release
      optimize: size
      debug: off

projects:
  - project: .\MyProject.cproject.yml
```

**File: MyProject.csolution.yml**

```yml
project:
  groups:
    - group: My group1
      files:
        - file: file1a.c
        - file: file1b.c
        - file: file1c.c

    - group: My group2
      files:
        - file: file2a.c

    - group: Test-Interface
      for-type: .Test
      files:
        - file: fileTa.c

  layers:
    - layer: NUCLEO-L552ZE-Q/Board.clayer.yml   # tbd find a better way: i.e. $Board$.clayer.yml
      for-type: +Board

    - layer: Production.clayer.yml              # added for target type: Production-HW
      for-type: +Production-HW

    - layer: Corstone-300/Board.clayer.yml      # added for target type: VHT-Corstone-300
      for-type: +VHT-Corstone-300

  components:
    - component: Device:Startup
    - component: CMSIS:RTOS2:FreeRTOS
    - component: ARM::CMSIS:DSP&Source          # not added for build type: Test
      not-for-type: .Test                           
```

### Project Setup for Related Projects

A solution is the software view of the complete system. It combines projects that can be generated independently and
therefore manages related projects. It may be also deployed to different targets during development as described in the
previous section under [Project Setup for Multiple Targets and Builds](#project-setup-for-multiple-targets-and-builds).

The picture below shows a system that is composed of:

- Project A: that implements a time-critical control algorithm running on a independent processor #2.
- Project B: which is a diagram of a cloud connected IoT application with Machine Learning (ML) functionality.
- Project C: that is the data model of the Machine Learning algorithm and separate to allow independent updates.
- Project D: that implements the device security (for example with TF-M that runs with TrustZone in secure mode).

In addition such systems may have a boot-loader that can be also viewed as another independent project.

![Related Projects of an Embedded System](./images/Solution.png "Related Projects of an Embedded System")

To manage the complexity of such related a projects, the `*.csolution.yml` file is introduced. At this level the
`target-types` and `build-types` may be managed, so that a common set is available across the system. However it should
be also possible to add project specific `build-types` at project level. (tdb: `target-types` might be only possible at
solution level).

- `target-types` describe a different hardware target system and have therefore different API files for peripherals or a
  different hardware configuration.

- `build-types` describe a build variant of the same hardware target. All `build-types` share the same API files for
  peripherals and the same hardware configuration, but may compile a different variant (i.e. with test I/O enabled) of
  an application.

**Related Projects: `iot-product.csolution.yml`**

```yml
solution:
  target-types:
    - type: Board
      board: NUCLEO-L552ZE-Q

    - type: Production-HW
      device: STM32U5X          # specifies device
      
  build-types:
    - type: Debug
      optimize: none
      debug: on

    - type: Test
      optimize: size
      debug: on
    
  projects:
    - project: /security/TFM.cproject.yml
      for-type: .Release
    - project: /application/MQTT_AWS.cproject.yml
    - project: /bootloader/Bootloader.cproject.yml
      not-for-type: +Virtual
```

## Project Structure

This section describes how the files of a csolution based project should be organized to allow the scenarios described above and describes the default directory structure.

Refer to [Directory Control](./YML-Input-Format.md#directory-control) for information about configuring this directories.

### RTE Directory Structure

The RTE directory contains the configuration information for software components and is generated by the `csolution` tool. It contains:

- Configuration files of the software components. These files have `attr="config"` in the PDSC-file of the software packs.  Refer to [PLM of Configuration Files](#plm-of-configuration-files) for more information.
- The file [`RTE_components.h`](#rte_componentsh) and pre-include files that are generated based on the PDSC-file information of the software packs.

> **Note:** The `./RTE` directory structure is maintained by tools. You should not modify the structure of this directory.

Default RTE directory structure     | Content
:-----------------------------------|:---------------
`<csolution>`                       | Base directory that one or more `*.csolution.yml` files.
`<project>`                         | Each project has its own directory, this base directory contains the `*.cproject.yml` file.
`<project>/RTE/<Cclass>`            | Configurable files for each component `Cclass` have a common directory.
`<project>/RTE/<Cclass>/<device>`   | Configurable files for components that have a condition to a `device` are in a separate directory.
`<project>/RTE/<context-dir>`       | Directory for `RTE_components.h` and pre-include files that are generated by the `csolution` tool.
`<layer>`                           | `*.clayer.yml` and related source files of a layer have a specific directory.
`<layer>/RTE/<Cclass>`              | Configurable files for each component `Cclass` have a common directory.
`<layer>/RTE/<Cclass>/<device>`     | Configurable files for components that have a condition to a `device` are in a separate directory.

The `<context-dir>` has the following format: `_<project-name>.<release-type>_<build-type>`.

#### Proposal

The default RTE directory structure can be modified with [`rte-dirs:`](YML-Input-Format.md#rte-dirs).
This list node allows to specify for each software component `Cclass` the directory that should be used to partly share a common configuration across a `project:` or `layer:`.

Review if the `<context-dir>` can just have the same format as the `context` which is: `<project-name>+<build-type>.<release-type>`.
I don't think it is required to align `<project>/RTE/<context-dir>` with other tools, but the remaining RTE directory structure should aligned with existing implementations (MDK, CMSIS-Eclipse-Pack).

### Output Directory Structure

By default the following output and working directories are used. The content of these output directories is generated by the `cbuild` step.

Output / Working Directory                    | Content
:---------------------------------------------|:---------------
`./out/<project>/<target>/<build>`            | Contains the final binary and symbol files of a project context.
`./tmp/<project>/<target>/<build>`            | Contains interim files (`*.o`, `*.lst`) of a project context.

### Software Components

Software components are re-usable library or source files that require no modification in the user application.
Optionally, configurable source and header files are provided that allow to set parameters for the software component.

- Configurable source and header files are copied to the project using the directory structure explained above.
- Libraries, source, and header files that are not configurable (and need no modification) are stored in the directory
  of the software component (typically part of CMSIS_Pack_ROOT) and get included directly from this location into the
  project.
- An Include Path to the header files of the software component is added to the C/C++ Compiler control string.

### PLM of Configuration Files

Configurable source and header files have a version information that is required during Project Lifetime Management
(PLM) of a project. The version number is important when the underlying software pack changes and provides a newer
configuration file version.

Depending on the PLM status of the application, the `csolution` performs for configuration files the following
operation:

1. **Add** a software component for the first time: the related config file is copied twice into the related `RTE`
   project directory. The first copy can be modified by the user with the parameters for the user application. The
   second copy is an unmodified  backup file with the format `<configfile>.<ext>.base@version`.

    **Example:** A configuration file `ConfigFile.h` at version `1.2.0` is copied:

    ```c
    ./RTE/component_class/ConfigFile.h                  // user editable configuration file
    ./RTE/component_class/ConfigFile.h.base@1.2.0       // current unmodified configuration file with version
                                                        // information; used as a base for version comparison
    ```

    The `csolution` outputs a user notification to indicate that files are added:

    ```text
    ./RTE/component_class/ConfigFile.h -  info: component 'name' added configuration file version '1.2.0'
    ```

    >**NOTE:** The unmodified files with `@version` information should be committed to the repository of the version control system as this files are used to upgrade configuration information using merge utilities.

2. **Upgrade** (or downgrade) a software component: if the version of the unmodified backup file is identical, no
   operation is performed. If the version differs, the new configuration file is copied with the format
   `<configfile>.<ext>.update@version`.

    **Example:** after configuration file `ConfigFile.h` to version `1.3.0` the directory contains these files:

    ```c
    ./RTE/component_class/ConfigFile.h                  // user editable configuration file (based on current version)
    ./RTE/component_class/ConfigFile.h.update@1.3.0     // new configuration file; used to start a 3-way merge
    ./RTE/component_class/ConfigFile.h.base@1.2.0       // current unmodified configuration file with version 
                                                        // information; used as a base for version comparison
    ```

    The `csolution` outputs a user notification to indicate that configuration files have changed:

    ```text
    ./RTE/component_class/ConfigFile.h - warning: component 'name' upgrade for configuration file version '1.3.0'
                                                  added, but file inactive
    ```

3. **User action to complete upgrade**: The user has now several options (outside of `csolution`) to merge the
   configuration file information. A potential way could be to use a 3-way merge utility. After merging the
   configuration file, the hidden backup should be deleted and the unmodified new version should become the hidden
   backup. The previous configuration file may be stored as backup as shown below.

    ```c
    ./RTE/component_class/ConfigFile.h                  // new configuration file with merge configuration
    ./RTE/component_class/ConfigFile.h.bak              // previous configuration file stored as backup
    ./RTE/component_class/ConfigFile.h.base@1.3.0       // current unmodified configuration file with version 
                                                        // information; used as a base for version comparison
    ```

- **Note: Multiple Instances of Configuration files**

   The system is also capable of handling multiple instances of configuration files as explained in the CMSIS-Pack
   specification under
   [Component Instances](https://open-cmsis-pack.github.io/Open-CMSIS-Pack-Spec/main/html/pdsc_components_pg.html#Component_Instances).
   In this case the instance %placeholder% is expanded as shown below:

   ```c
   ./RTE/component_class/ConfigFile_0.h
   ./RTE/component_class/ConfigFile_0.h.base@1.2.0
   ./RTE/component_class/ConfigFile_1.h
   ./RTE/component_class/ConfigFile_1.h.base@1.2.0
   ```

### RTE_Components.h

The file `./RTE/RTE_Components.h` is automatically created by the CMSIS Project Manager (during CONVERT). For each
selected software component it contains `#define` statements required by the component. These statements are defined in
the `*.PDSC` file for that component. The following example shows a sample content of a RTE_Components.h file:

```c
/* Auto generated Run-Time-Environment Component Configuration File *** Do not modify ! *** */

#ifndef RTE_COMPONENTS_H
#define RTE_COMPONENTS_H

/* Define the Device Header File: */
#define CMSIS_device_header "stm32f10x.h"

#define RTE_Network_Interface_ETH_0     /* Network Interface ETH 0 */
#define RTE_Network_Socket_BSD          /* Network Socket BSD */
#define RTE_Network_Socket_TCP          /* Network Socket TCP */
#define RTE_Network_Socket_UDP          /* Network Socket UDP */

#endif /* RTE_COMPONENTS_H */
```

The typical usage of the `RTE_Components.h` file is in header files to control the inclusion of files that are related
to other components of the same software pack.

```c
#include "RTE_Components.h"
#include  CMSIS_device_header

#ifdef  RTE_Network_Interface_ETH_0  // if component Network Interface ETH 0 is included
#include "Net_Config_ETH_0.h"        // add the related configuration file for this component
#endif
```

## Proposals

### CMSIS-Zone Integration

Suggest to split this into two sections:

- `resources:` to define the execution phases, memory regions and region splits, and peripherals. This section would be
  in the `csolution.yml` file.
- `requirements:` to define project requirements - effectively the partitioning of a system. It should be possible to
  assign to the application all remaining resources.

Add to the project the possibility to specify . The issue might be that the project files become overwhelming,
alternative is to keep partitioning in separate files.

```yml
resources:
  phases:    # define the life-time of a resource definition
    - phase: Boot
    - phase: OTA
    - phase: Run

  memories:              # specifies the required memory
    - split: SRAM_NS
      into:
      - region: DATA_NS
        size: 128k
        permission: n
      - region: DATA_BOOT
        phase: Boot      # region life-time (should allow to specify multiple phases)
        size: 128k
    
  peripherals:           # specifies the required peripherals
    - peripheral: I2C0
      permission: rw, s
```

### CMSIS-Pack extensions

#### Layers in packs

A layer is a set of pre-configured software components. It should be possible to store a layer in a pack and apply
filter conditions to it. In combination with interfaces specifications, an interactive IDE should be able to display suitable layers that could be added to an application.
