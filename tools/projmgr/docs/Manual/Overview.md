# CMSIS Project Manager (Users Manual - Draft)

The **[Open-CMSIS-Pack](https://www.open-cmsis-pack.org/index.html) Project Manager** processes **User Input Files** (in YML format) and **Software Packs** (in Open-CMSIS-Pack format) to create self-contained CMSIS-Build input files that allow to generate  independent projects which may be a part of a more complex application.

The **CMSIS Project Manager** supports the user with the following features:

- Access to the content of software packs in Open-CMSIS-Pack format to:
  - Setup the tool chain based on a *Device* or *Board* that is defined in the CMSIS-Packs.
  - Add software components that are provided in the various software packs to the application.
- Organize applications (with a `*.csolution.yml` file) into projects that are independently managed (using `*.cproject.yml` files).
- Manage the resources (memory, peripherals, and user defined) across the entire application to:
  - Partition the resources of the system and create related system and linker configuration.
  - Support in the configuration of software stacks (such as RTOS threads).
  - Hint the user for inclusion of software layers that are pre-configured for typical use cases.
- Organize software layers (with a `*.clayer.yml` file) that enable code reuse across similar applications.
- Manage multiple hardware targets to allow application deployment to different hardware (test board, production hardware, etc.).
- Manage multiple build types to support software verification (debug build, test build, release build, ect.)
- Support multiple compiler toolchains (GCC, LLVM, Arm Compiler 6, IAR, etc.) for project deployment.

Manual Chapters    | Content
:------------------|:-------------------------
Usage              | Overall Concept, tool setup, invocation commands, and naming conventions.
YML Input Format   | Format of the various input files.

## Revision History

Version            | Description
:------------------|:-------------------------
Draft              | Work in progress


# Usage

The  **CMSIS Project Manager** is a command line utility that is available for different operating systems.  

## Overview of Operation

![Overview](./images/Overview.png "Overview")

This picture above outlines the operation. The **CMSIS Project Manager** uses the following files.

Input Files              | Used for....
:------------------------|:---------------------------------
[Generic Software Packs](https://open-cmsis-pack.github.io/Open-CMSIS-Pack-Spec/main/html/cp_PackTutorial.html#cp_SWComponents) | ... provide re-usable software components that are typically configurable  towards a user application.
[DFP Software Packs](https://open-cmsis-pack.github.io/Open-CMSIS-Pack-Spec/main/html/cp_PackTutorial.html#createPack_DFP)     | ... device related information on the tool configuration. May refer an *.rzone file.
[BSP Software Packs](https://open-cmsis-pack.github.io/Open-CMSIS-Pack-Spec/main/html/cp_PackTutorial.html#createPackBoard)    | ... board specific configuration (i.e. memory). May refer to an *.rzone file that defines board components.
[*.rzone files](https://arm-software.github.io/CMSIS_5/Zone/html/xml_rzone_pg.html)                 | ... definition of memory and peripheral resources. If it does not exist, content is created from DFP.
*.csettings.yml          | ... [1.] setup of an environment (could be an IDE) to pre-define a toolchain or built-types (Debug, Release).
*.csolution.yml          | ... [2.] complete scope of the application and the build order of sub-projects.
*.cproject.yml           | ... [3.] content of an independent build (linker run) - directly relates to a *.cprj file.
*.clayer.yml             | ... [4.] set of source files along with pre-configured components for reuse in different applications.

**Note**: The values [*n.*] indicate the order of processing of the user input files.

Output Files             | Used for....
:------------------------|:---------------------------------
[Project Build Files](https://arm-software.github.io/CMSIS_5/Build/html/cprjFormat_pg.html) | ... project build information for a Open-CMSIS-Pack based tool environment.
Run-Time Environment (RTE)  | ... contains the user configured files of a project along with RTE_Components.h inventory file.
[Project Resource Files *.fzone](https://arm-software.github.io/CMSIS_5/Zone/html/GenDataModel.html)     | ... resource and partition data structure for template based code generators.

## Requirements

The CMSIS Pack repository must be present in the development environment.

- There are several ways to initialize and configure the pack repository, for example using the 
`cpackget` tool available from https://github.com/Open-CMSIS-Pack/cpackget
- Before running `csolution` the location of the pack repository shall be set via the environment variable
`CMSIS_PACK_ROOT` otherwise its default location (todo what is the default?) will be taken.

## Usage

```text
CMSIS Project Manager 0.0.0+gdd33bca (C) 2021 ARM
Usage:
  csolution <command> [<args>] [OPTIONS...]

Commands:
  list packs          Print list of installed packs
       devices        Print list of available device names
       components     Print list of available components
       dependencies   Print list of unresolved project dependencies
       contexts       Print list of contexts in a csolution.yml
  convert             Convert cproject.yml or csolution.yml in cprj files
  help                Print usage

Options:
  -p, --project arg   Input cproject.yml file
  -s, --solution arg  Input csolution.yml file
  -f, --filter arg    Filter words
  -o, --output arg    Output directory
  -h, --help          Print usage
```

### Commands

Print list of installed packs. The list can be filtered by words provided with the option `--filter`:

```text
list packs [--filter "<filter words>"]
```

Print list of available device names. The list can be filtered by words provided with the option `--filter`:

```text
list devices [--filter "<filter words>"]
```

Print list of available components. The list can be filtered by a selected device in the `cproject.yml` file with the option `--input` and/or by words provided with the option `--filter`:

```text
list components [--p <example.cproject.yml> --filter "<filter words>"]
```
todo: this does not work anymore

Print list of unresolved project dependencies. The device and components selection must be provided in the `cproject.yml` file with the option `--input`. The list can be filtered by words provided with the option `--filter`:

```text
list dependencies --input <example.cproject.yml> [--filter "<filter words>"]
```

Convert cproject.yml in cprj file:

```text
convert --p <example.cproject.yml>
```

# Project Examples

## Minimal Project Setup

Simple applications require just one self-contained file.

**Simple Project: `Sample.cproject.yml`**

```yml
project:
  compiler: AC6                    # Use Arm Compiler 6 for this project
  device: LPC55S69JEV98:cm33_core0 # Device name (exact name as defined in the DFP) optional with Pname for core selection
  
  optimize: size                   # Code optimization: emphasis code size
  debug: on                        # Enable debug symbols

  groups:                          # Define file groups of the project
    - group: My files           
      files:                       # Add source files
        - file: main.c

    - group: HAL
      files:
        - file: ./hal/driver1.c

  components:                      # Add software components
    - component: Device:Startup
```

## Software Layers

Software layers collect source files and software components along with configuration files for re-use in different projects.
The following diagram shows the various layers that are used to compose the IoT Cloud examples.

![Software Layers](./images/Layer.png "Target and Build Types")

The following example is a `Blinky` application that uses a `App`, `Board`, and `RTOS` layer to compose the application for a NUCELO-G474RE board.  Note, that the `device:` definition is is the `Board` layer.

**Example Project: `Blinky.cproject.yml`**

```yml
project:
  compiler: AC6

  layers:
    - layer: .\Layer\App\Blinky.clayer.yml
    - layer: .\Layer\RTOS\RTX.clayer.yml
    - layer: .\Layer\Board\Nucleo-G474RE.clayer.yml
```

**App Layer: `.\Layer\App\Blinky.clayer.yml`**

```yml
layer:
# type: RTOS
  name: RTX
  description: Keil RTX5 open-source real-time operating system with CMSIS-RTOS v2 API

  interfaces:
    - provides:
        - RTOS2:

  components:
    - component: CMSIS:RTOS2:Keil RTX5&Source
```

**RTOS Layer: `.\Layer\RTOS\RTX.clayer.yml`**

```yml
layer:
# type: RTOS
  name: RTX
  description: Keil RTX5 open-source real-time operating system with CMSIS-RTOS v2 API

  interfaces:
    - provides:
        - RTOS2:

  components:
    - component: CMSIS:RTOS2:Keil RTX5&Source
```

**Board Layer: `.\Layer+NUCELO-G474RE\Board\Nucleo-G474RE.clayer.yml`**

```yml
layer:
  name: NUCLEO-G474RE
# type: Board
  description: Board setup with interfaces
  device: STM32G474CBTx

  interfaces:
    - consumes:
        - RTOS2:
    - provides:
        - C_VIO:
        - A_IO9_I:
        - A_IO10_O:
        - C_VIO:
        - STDOUT:
        - STDIN:
        - STDERR:
        - Heap: 65536
  
  components:
    - component: CMSIS:CORE
    - component: CMSIS Driver:USART:Custom
    - component: CMSIS Driver:VIO:Board&NUCLEO-G474RE
    - component: Compiler&ARM Compiler:Event Recorder&DAP
    - component: Compiler&ARM Compiler:I/O:STDERR&User
    - component: Compiler&ARM Compiler:I/O:STDIN&User
    - component: Compiler&ARM Compiler:I/O:STDOUT&User
    - component: Board Support&NUCLEO-G474RE:Drivers:Basic I/O
    - component: Device&STM32CubeMX:STM32Cube Framework:STM32CubeMX
    - component: Device&STM32CubeMX:STM32Cube HAL
    - component: Device&STM32CubeMX:Startup
  
  groups:
    - group: Board IO
      files:
        - file: ./Board_IO/arduino.c
        - file: ./Board_IO/arduino.h
        - file: ./Board_IO/retarget_stdio.c
    - group: STM32CubeMX
      files:
        - file: ./RTE/Device/STM32G474RETx/STCubeGenerated/STCubeGenerated.ioc
```

## Project Setup for Multiple Targets and Builds

Complex examples require frequently slightly different targets and/or modifications during build, i.e. for testing. The picture below shows a setup during software development that supports:

- Unit/Integration Testing on simulation models (called Virtual Hardware) where Virtual Drivers implement the interface to simulated I/O.
- Unit/Integration Testing the same software setup on a physical board where Hardware Drivers implement the interface to physical I/O.
- System Testing where the software is combined with more software components that compose the final application.

![Target and Build Types](./images/TargetBuild-Types.png "Target and Build Types")

As the software may share a large set of common files, provisions are required to manage such projects.  The common way in other IDE's is to add:

- **target-types** that select a target system. In the example this would be:
  - `Virtual`: for Simulation Models.
  - `Board`: for a physical evaluation board.
  - `Production-HW`: for system integration test and the final product delivery.
- **build-types** add the flexibility to configure each target build towards a specific testing.  It might be:
  - `Debug`: for a full debug build of the software for interactive debug.
  - `Test`: for a specific timing test using a test interface with code maximal optimization.
  - `Release`: for the final code deployment to the systems.

It is required to generate reproducible builds that can deployed on independent CI/CD test systems. To achieve that, the CMSIS Project Manager generates *.cprj output files with the following naming conventions:

`<projectname>[.<build-type>][+target-type].cprj` this would generate for example: `Multi.Debug+Production-HW.cprj`

This enables that each target and/or build type can be identified and independently generated which provides the support for test automation. It is however not required to build every possible combination, this should be under user control.

**Flexible Builds for multi-target projects.

Currently multi-target projects require the setup of a `*.csolution.yml` file to define `target-types` and `build-types`. Note, that this is currently under review, but this documents the current status.

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
      optimize: debug
      debug: on

    - type: Test
      optimize: max
      debug: on

    - type: Release
      optimize: max
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

## Project Setup for Related Projects

A solution is the software view of the complete system. It combines projects that can be generated independently and therefore manages related projects. It may be also deployed to different targets during development as described in the previous section under [Project Setup for Multiple Targets and Test Builds](#project-setup-for-multiple-targets-and-test-builds).

The picture below shows a system that is composed of:

- Project A: that implements a time-critical control algorithm running on a independent processor #2.
- Project B: which is a diagram of a cloud connected IoT application with Machine Learning (ML) functionality.
- Project C: that is the data model of the Machine Learning algorithm and separate to allow independent updates.
- Project D: that implements the device security (for example with TF-M that runs with TrustZone in secure mode).

In addition such systems may have a boot-loader that can be also viewed as another independent project.

![Related Projects of an Embedded System](./images/Solution.png "Related Projects of an Embedded System")

To manage the complexity of such related a projects, the `*.csolution.yml` file is introduced. At this level the `target-types` and `build-types` may be managed, so that a common set is available across the system. However it should be also possible to add project specific `build-types` at project level.  (tdb: `target-types` might be only possible at solution level).

- `target-types` describe a different hardware target system and have therefore different API files for peripherals or a different hardware configuration.

- `build-types` describe a build variant of the same hardware target. All `build-types` share the same API files for peripherals and the same hardware configuration, but may compile a different variant (i.e. with test I/O enabled) of an application.

**Related Projects `iot-product.csolution.yml`**

```yml
solution:
  target-types:
    - type: Board
      board: NUCLEO-L552ZE-Q

    - type: Production-HW
      device: STM32U5X          # specifies device
      
  build-types:
    - type: Debug
      optimize: debug
      debug: on

    - type: Test
      optimize: max
      debug: on
    
  projects:
    - project: /security/TFM.cproject.yml
      type: .Release
    - project: /application/MQTT_AWS.cproject.yml
    - project: /bootloader/Bootloader.cproject.yml
      not-for-type: +Virtual
```

# Project Structure

## Directory Structure

ToDo: Impact analysis to legacy products that include CMSIS-Pack management.

This section describes how the files of `*.csolution.yml` should be organized to allow the scenarios described above:

Source Directory                    | Content
:-----------------------------------|:---------------
`.`                                 | Contains one or more `*.csolution.yml` files that describes an overall application.
`./<project>`                       | Each project has its own directory
`./<project>/RTE+<target>`          | Configurable files that are specific to a target have a specific directory.
`./<project>/RTE`                   | Configurable files that are common to all targets may have a common directory.
`./<project>/Layer+<target>/<name>` | `*.clayer.yml` and related source files of a layers that are specific to a target have a specific directory.
`./<project>/Layer/<name>`          | `*.clayer.yml` and related source files of a layers that are common to all targets may have a common directory.

**Notes:**

- `./<project>/RTE+<target>` contains the *.cprj file that is generated by `projmgr`
- Directory names `RTE` and `Layer` should become configurable.  ToDo: analyze impact.

The `./RTE` directory structure is maintained by tools and has the following structure. You should not modify the structure of this directory.

`RTE[+<target>]` Directory        | Content
:---------------------------------|:---------------
`.../.<build-type>`               | Contains the file `RTE_Components.h` that is specific to a `build-type`.
`.../<component class>`           | Configurable files for each component class are stored in sub-folders. The name of this sub-folder is derived from the component class name.
`.../<component class>/<Dname>`   | Configurable files of the component class that are device specific. It is generated when a component has a condition with a `Dname` attribute. (strictly speaking no longer needed, backward compatiblity to MDK?)
`.../Device/<Dname>`              | Configurable files of the component class Device. This should have always a condition with a `Dname` attribute.

Output Directory                              | Content
:---------------------------------------------|:---------------
`./<project>/Output+<target>`                 | Contains the final binary and symbol files of a project. Each `build-type` shares the same output directory.
`./<project>/.Interim+<target>/.<build-type>` | Contains interim files (`*.o`, `*.lst`) fore each `build-type`

**Note:**
- The content of the `Output` directory is generated by the `CBuild` step.

## Support for unnamed *.yml files

It is possible to use named and unnamed `*.csolution.yml` and `*.cproject.yml` files. If an explicit name is omitted, the name of the directory is used. However it is possible to overwrite the directory name with an explicit filename.

**Example:**
```c
./blinky/cproject.yml                     // project name: "blinky"
./blinky/blinky-test.cproject.yml         // project name: "blinky-test"
```

A solution template skeleton may be provided as shown below. The benefit is that renaming of the directory names changes also the project and solution names.

```c
./                                        // root directory of a workspace
./cdefaults.yml                           // contains the ‘cdefaults.yml’ file
./MySolution                              // the current directory used for invoking "projmgr"
./MySolution/csolution.yml                // unnamed "csolution.yml" file is the default input file
./MySolution/blinky                       // project directory for project "blinky"
./MySolution/blinky/cproject.yml          // unnamed "cproject.yml" that obtains the project name from directory name
```



## Software Components

Software components are re-usable library or source files that require no modification in the user application. Optionally, configurable source and header files are provided that allow to set parameters for the software component. 

- Configurable source and header files are copied to the project using the directory structure explained above.
- Libraries, source, and header files that are not configurable (and need no modification) are stored in the directory of the Software Component (typically part of CMSIS_Pack_ROOT) and get included directly from this location into the project.
- An Include Path to the header files of the Software Component is added to the C/C++ Compiler control string.

### PLM of configuration files

Configurable source and header files have a version information that is required during Project Lifetime Management (PLM) of a project. The version number is important when the underlying software pack changes and provides a newer configuration file version.

Depending on the PLM status of the application, the `csolution` performs for configuration files the following operation:

1. **Add** a software component for the first time: the related config file is copied twice into the related `RTE` project directory.  The first copy can be modified by the user with the parameters for the user application. The second copy is an unmodified hidden backup file that is appended with the version information.

    **Example:** A configuration file `ConfigFile.h` at version `1.2.0` is copied:

    ```c
    ./RTE/component_class/ConfigFile.h           // user editable configuration file
    ./RTE/component_class/.ConfigFile.h@1.2.0    // hidden backup used for version comparison
    ```
    
    The `csolution` outputs a user notification to indicate that files are added:

    ```text
    ./RTE/component_class/ConfigFile.h -  info: component 'name' added configuration file version '1.2.0'
    ```

2. **Upgrade** (or downgrade) a software component: if the version of the hidden backup is identical, no operation is performed. If the version differs, the new configuration file is copied with appended version information.

    **Example:** after configuration file `ConfigFile.h` to version `1.3.0` the directory contains these files:

    ```c
    ./RTE/component_class/ConfigFile.h           // user editable configuration file
    ./RTE/component_class/ConfigFile.h@1.3.0     // user editable configuration file
    ./RTE/component_class/.ConfigFile.h@1.2.0    // hidden backup used for version comparison
    ```

    The `csolution` outputs a user notification to indicate that configuration files have changed:

    ```text
    ./RTE/component_class/ConfigFile.h - warning: component 'name' upgrade for configuration file version '1.3.0' added, but file inactive
    ```

3. **User action to complete upgrade**: The user has now several options (outside of `csolution`) to merge the configuration file information.  A potential way could be to use a 3-way merge utility. After merging the configuration file, the hidden backup should be deleted and the unmodified new version should become the hidden backup.  The previous configuration file may be stored as backup as shown below.

    ```c
    ./RTE/component_class/ConfigFile.h           // new configuration file with merge configuration
    ./RTE/component_class/ConfigFile.h.bak       // previous configuration file stored as backup
    ./RTE/component_class/.ConfigFile.h@1.3.0    // hidden backup of unmodified config file, used for version comparison
    ```

- **Note: Multiple Instances of Configuration files**

   The system is also capable of handling multiple instances of configuration files as explained in the CMSIS-Pack specification under [Component Instances](https://open-cmsis-pack.github.io/Open-CMSIS-Pack-Spec/main/html/pdsc_components_pg.html#Component_Instances). In this case the instance %placeholder% is expanded as shown below:

   ```c
   . /RTE/component_class/.ConfigFile_0.h@1.2.0
   . /RTE/component_class/.ConfigFile_1.h@1.2.0
   ```

## RTE_Components.h

The file `./RTE/RTE_Components.h` is automatically created by the CMSIS Project Manager (during CONVERT). For each selected Software Component it contains `#define` statements required by the component. These statements are defined in the \*.PDSC file for that component. The following example shows a sample content of a RTE_Components.h file:

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

The typical usage of the `RTE_Components.h` file is in header files to control the inclusion of files that are related to other components of the same Software Pack.
```c
#include "RTE_Components.h"
#include  CMSIS_device_header

#ifdef  RTE_Network_Interface_ETH_0  // if component Network Interface ETH 0 is included
#include "Net_Config_ETH_0.h"        // add the related configuration file for this component
#endif
```



# YML Format

## Name Conventions

### Pack Name Conventions

The CMSIS Project Manager uses the following syntax to specify the `pack:` names in the `*.yml` files.

```text
[vendor::] pack-name [@[>=] version]
```

Element      | Description
:------------|:---------------------
`vendor`     | is the vendor name of the software pack (optional).
`pack-name`  | is the name of the software pack, for the key `filter:` wildcards (*, ?) can be used.
`version`    | is the version number of the software pack, with `@1.2.3` that must exactly match, or `@>=1.2.3` that allows any version higher or equal.

**Examples:**

```yml
- pack:   CMSIS@>5.5.0                                 # 'CMSIS' Pack (with version 5.5.0)
- pack:   Keil::MDK-Middleware@>=7.13.0                # 'MDK-Middleware' Software Pack from vendor Keil (with version 7.13.0 or higher)
- filter: AWS::*                                       # All Software Packs from vendor 'AWS'
- filter: Keil::STM*                                   # All Software Packs that start with 'STM' from vendor 'Keil'
```

### Component Name Conventions

The CMSIS Project Manager uses the following syntax to specify the `component:` names in the `*.yml` files.

```text
[Cvendor::] Cclass [&Cbundle] :Cgroup [:Csub] [&Cvariant] [@[>=]Cversion]
```

Element    | Description
:----------|:---------------------
`Cvendor`  | is the name of the component vendor defined in `<components>` element of the software pack (optional).
`Cclass`   | is the component class name  defined in `<components>` element of the software pack (required)
`Cbundle`  | is the bundle name of component class defined in `<bundle>` element of the software pack (optional).
`Cgroup`   | is the component group name  defined in `<components>` element of the software pack (required).
`Csub`     | is the component sub-group name  defined in `<components>` element of the software pack (optional).
`Cvariant` | is the component sub-group name  defined in `<components>` element of the software pack (optional).
`Cversion` | is the version number of the component, with `@1.2.3` that must exactly match, or `@>=1.2.3` that allows any version higher or equal.

**Notes:**

- The unique separator `::` allows to omit `Cvendor`
- When `Cvariant` is omitted, the default `Cvariant` is selected.

**Examples:**

```yml
- component: ARM::CMSIS:CORE                           # CMSIS Core component from vendor ARM (any version)
- component: ARM::CMSIS:CORE@5.5.0                     # CMSIS Core component from vendor ARM (with version 5.5.0)
- component: ARM::CMSIS:CORE@>=5.5.0                   # CMSIS Core component from vendor ARM (with version 5.5.0 or higher)

- component: Device:Startup                            # Device Startup component from any vendor

- component: CMSIS:RTOS2:Keil RTX5                     # CMSIS RTOS2 Keil RTX5 component with default variant (any version)
- component: ARM::CMSIS:RTOS2:Keil RTX5&Source@5.5.3   # CMSIS RTOS2 Keil RTX5 component with variant 'Source' and version 5.5.3

- component: Keil::USB&MDK-Pro:CORE&Release@6.15.1     # From bundle MDK-Pro, USB CORE component with variant 'Release' and version 6.15.1
```

### Device Name Conventions

The device specifies multiple attributes about the target that ranges from the processor architecture to Flash algorithms used for device programming. The following syntax is used to specify a `device:` value in the `*.yml` files.


```text
[Dvendor:: [device_name] ] [:Pname]
```

Element       | Description
:-------------|:---------------------
`Dvendor`     | is the name (without enum field) of the device vendor defined in `<devices><family>` element of the software pack (optional).
`device_name` | is the device name (Dname attribute) or when used the variant name (Dvariant attribute) as defined in the \<devices\> element.
`Pname`       | is the processor identifier (Pname attribute) as defined in the `<devices>` element.

**Notes:**

- All elements of a device name are optional which allows to supply additional information, such as the `:Pname` at different stages of the project. However the `device_name` itself is a mandatory element and must be specified in context of the various project files.
- `Dvendor::` must be used in combination with the `device_name`.  

**Examples:**

```yml
device: NXP::LPC1768                                # The LPC1788 device from NXP
device: LPC1788                                     # The LPC1788 device (vendor is evaluated from DFP)
device: LPC55S69JEV98                               # Device name (exact name as defined in the DFP)
device: LPC55S69JEV98:cm33_core0                    # Device name (exact name as defined in the DFP) with Pname specified
device: :cm33_core0                                 # Pname added to a previously defined device name (or a device derived from a board)
```

### Board Name Conventions

Evaluation Boards define indirectly a device via the related BSP.   The following syntax is used to specify a `board:` value in the `*.yml` files.

```text
[vendor::] board_name
```

Element      | Description
:------------|:---------------------
`vendor`     | is the name of the board vendor defined in `<boards><board>` element of the board support pack (BSP) (optional).
`board_name` | is the board name (name attribute) of the as defined in the \<board\> element of the BSP.

**Note:**

- When a `board:` is specified, the `device:` specification can be omitted, however it is possible to overwrite the device setting in the BSP with an explicit `device:` setting.

**Examples:**

```yml
board: Keil::MCB54110                             # The Keil MCB54110 board (with device NXP::LPC54114J256BD64) 
board: LPCXpresso55S28                            # The LPCXpresso55S28 board
```

## Access Sequences

The following **Access Sequences** allow to use arguments from the CMSIS Project Manager under `defines`, `add-paths`, `misc` and `files` values of the various `*.yml` files.

Access Sequence             | Description
:---------------------------|:--------------------------------------
`$Bname$`                   | Board name of the selected board.
`$Bpack$`                   | Path to the pack that defines the selected board (BSP).
`$Dname$`                   | Device name of the selected device.
`$Dpack$`                   | Path to the pack that defines the selected device (DFP).
`$PackRoot$`                | Path to the CMSIS Pack Root directory.
`$Pack(vendor.name)$`       | Path to specific pack [with latest version ToDo: revise wording]. Example: `$Pack(NXP.K32L3A60_DFP)$`.
`$Output(project[.build-type][+target-type])$` | Path to a related project's output file without extension
`$OutDir(project[.build-type][+target-type])$` | Path to a related project's output directory
`$Source(project[.build-type][+target-type])$` | Path to a related project's source directory

If `build-type` and/or `target-type` are omitted the access sequence will lead to the project context's `build-type` and `target-type` being processed.

ToDo: define directory structure

**Examples:**

```yml
groups:
  - group:  "Main File Group"
    defines:
      - $Dname$                           # Generate a #define 'device-name' for this file group
```

```yml
  - execute: Generate Image
    os: Windows                           # on Windows run from
    run: $DPack$/bin/gen_image.exe        # DFP the get_image tool
```

## Overall Structure

The **ProjMgr** uses The table below explains the top-level elements in each of the different `*.yml` input files.

Keyword          | Allowed for following files..                  | Description
:----------------|:-----------------------------------------------|:------------------------------------
`default:`       | `[defaults].csettings.yml`, `*.csolution.yml`  | Start of the default section with [General Properties](#general-properties)
`solution:`      | `*.csolution.yml`                              | Start of the [Collection of related Projects](#solution-collection-of-related-projects) along with build order.
`project:`       | `*.cproject.yml`                               | Start of a Project along with properties - tbd; used in `*.cproject.yml`.
`layer:`         | `*.clayer.yml`                                 | Start of a software layer definition that contains pre-configured software components along with source files.

### Structure of *.cdefaults.yml 

The following top-level *key values* are supported in a *.cdefaults.yml file.

```yml
default:                 # Start of default settings
  packs:                 # Defines local packs and/or scope of packs that are used
  build-types:           # Default list of build-types (i.e. Release, Debug)
  compiler:              # Default selection of the compiler
```

### Structure of *.csolution.yml 

The following top-level *key values* are supported in a `*.csolution.yml` file.

```yml
solution:
  packs:                 # Defines local packs and/or scope of packs that are used
  target-types:          # List of build-types
  build-types:           # List of build-types

  compiler:              # Default selection of the compiler

  projects:              # Start of a project list that are related
```

### Structure of *.cproject.yml 

The following top-level *key values* are supported in a `*.cproject.yml` file.
```yml
project:
  description:           # project description (optional)
  compiler:              # toolchain specification (optional) 
  output-type: lib | exe # generate executable (default) or library

  optimize:              # optimize level for code generation (optional)
  debug:                 # generation of debug information (optional)
  defines:               # define symbol settings for code generation (optional).
  undefines:             # remove define symbol settings for code generation (optional).
  add-paths:             # additional include file paths (optional).
  del-paths:             # remove specific include file paths (optional). 
  misc:                  # Literal tool-specific controls

  board:                 # board specification (optional)
  device:                # device specification (optional)         
  processor:             # processor specific settings (optional)

  groups:                # List of source file groups along with source files
  components:            # List of software components used
  layers:                # List of software layers that belong to the project
```

### Structure of *.clayer.yml 

The following top-level *key values* are supported in a `*.clayer.yml` file.

```yml
layer:                   # Start of a layer
  description:           # layer description (optional)
  interfaces:            # List of consumed and provided interfaces
  groups:                # List of source file groups along with source files
  components:            # List of software components used
```

## Source Code Management 

Keyword          | Allowed for following files..                  | Description
:----------------|:-----------------------------------------------|:------------------------------------
`target-types:`  | `*.csolution.yml`                              | Start of the [Target type declaration list](#target-and-build-types) that allow to switch between [different targets](#project-setup-for-multiple-targets-and-test-builds).
`build-types:`   | `[defaults].csettings.yml`, `*.csolution.yml`  | Start of the [Build type declaration list](#target-and-build-types) that allow to switch between different build settings such as: Release, Debug, Test.
`groups:`        | `*.cproject.yml`, `*.clayer.yml`               | Start of a list that adds [source groups and files](#groups-and-files) to a project or layer.
`layers:`        | `*.cproject.yml`                               | Start of a list that adds software layers to a project.
`components:`    | `*.cproject.yml`, `*.clayer.yml`               | Start of a list that adds software components to a project or layer.


## General Properties

The keywords below can be used at various levels in this file types: `[defaults].csettings.yml`, `*.csolution.yml`, and `*.cproject.yml`. 

Keyword         | Description
:---------------|:------------------------------------
`compiler:`     | Selection of the compiler toolchain used for the project, i.e. `GCC`, `AC6`, `IAR`, optional with version, i.e AC6@6.16-LTS
`device:`       | [Unique device name](#device-name-conventions), optionally with vendor and core.  A device is derived from the `board:` setting, but `device:` overrules this setting.
`board:`        | [Unique board name](#board-name-conventions), optionally with vendor.
`optimize:`     | Generic optimize levels (max, size, speed, debug), mapped to the toolchain by CMSIS-Build.
`debug:`        | Generic control for the generation of debug information (on, off), mapped to the compiler toolchain by CMSIS-Build.
`warnings:`     | Control warnings (could be: no, all, Misra, AC5-like), mapped to the toolchain by CMSIS-Build.
`defines:`      | [#define symbol settings](#defines) for the compiler toolchain (passed as command-line option).
`undefines:`    | [Remove #define symbol settings](#undefines) for the compiler toolchain.
`add-paths:`    | [Add include path settings](#add-paths) for the compiler toolchain.
`del-paths:`    | [Remove include path settings](#del-paths) for the code generation tools.
`misc:`         | [Miscellaneous literal tool-specific controls](#misc) that are passed directly to the compiler toolchain depending on the file type.

**Notes:**

- `defines:`, `add-paths:`, `del-paths:`  and `misc:` are additive. All other keywords overwrite previous settings.

## Pack selection

The `packs:` section that can be specified in the `*.csolution.yml` and `*.cdefault.yml` file allows you to:
  
  - reduce the scope of software packs that are available for a project.
  - specify a specific software pack optional with a version specification.
  - provide a paths to a local installation of a software pack that is for example project specific or under development.

The pack names use the [Pack Name Conventions](#pack-name-conventions) to specify the name of the software packs.
The `pack` definition can be specific to [target and build types](#target-and-build-types) and may provide a local path to a development repository of a software pack.


**YML structure:**

```yml
packs:                 # Start a list for section of software packs
  - filter:            # pack filter, optional with wildcards (additive)
    
  - pack:              # explicit pack specification, overrules a pack filter
    path:              # a explicit path name that stores the software pack
    for-type:          # include pack for a list of *build* and *target* types.
    not-for-type:      # exclude pack for a list of *build* and *target* types.
```

**Example:**
```yml
packs:                                  # start section that specifics software packs
  - filter: AWS::                       # use packs from AWS
  - filter: NXP::*K32L*                 # use packs from NXP that relate to K32L series (would match K32L3A60_DFP + FRDM-K32L3A6_BSP)
  - filter: ARM::                       # use packs from Arm

  - pack: Keil::Arm_Compiler            # add always Keil::Arm_Compiler pack (bypasses filter)
  - pack: Keil::MDK-Middleware@7.13.0   # add Keil::MDK-Middleware pack at version 7.13.0

  - pack: NXP::K32L3A60_DFP             # add pack for NXP device 
    path: ./local/NXP/K32L3A60_DFP      # with path to the pack (local copy, repo, etc.)

  - path: AWS::coreHTTP                 # add pack
    path: ./development/AWS/coreHTTP    # with path to development source directory
    for-type: +DevTest                  # pack is only used for target-type "DevTest"
```

## Target and Build Types

The section [Project setup for multiple targets and test builds](#project-setup-for-multiple-targets-and-test-builds) describes the concept of  `target-types` and `build-types`.  These *types* can be defined in the following files in the following order:

1. `default.csettings.yml`  where it defines global *types*, such as *Debug* and *Release* build.
2. `*.csolution.yml` where it specifies the build and target *types* of the complete system.
3. `*.cproject.yml` where it may add specific *types* for an project (tbd are target types allowed when part of a solution?)

The *`target-type`* and *`build-type`* definitions are additive, but an attempt to redefine an already existing type results in an error.

The settings of the *`target-type`* are processed first; then the settings of the *`build-type`* that potentially overwrite the *`target-type`* settings.

**YML structure:**

```yml
target-types:          # Start a list of target type declarations
  - type:              # name of the target type (required)
    board:             # board specification (optional)
    device:            # device specification (optional)         
    processor:         # processor specific settings (optional)
    compiler:          # toolchain specification (optional) 
    optimize:          # optimize level for code generation (optional)
    debug:             # generation of debug information (optional)
    defines:           # define symbol settings for code generation (optional).
    undefines:         # remove define symbol settings for code generation (optional).
    add-paths:         # additional include file paths (optional).
    del-paths:         # remove specific include file paths (optional). 
    misc:              # Literal tool-specific controls

build-types:           # Start a list of build type declarations
  - type:              # name of the build type (required)
    processor:         # processor specific settings (optional)
    compiler:          # toolchain specification (optional) 
    optimize:          # optimize level for code generation (optional)
    debug:             # generation of debug information (optional)
    defines:           # define symbol settings for code generation (optional).
    undefines:         # remove define symbol settings for code generation (optional).
    add-paths:         # additional include file paths (optional).
    del-paths:         # remove specific include file paths (optional). 
    misc:              # Literal tool-specific controls
```

**Example:**

```yml
target-types:
  - type: Board
    board: NUCLEO-L552ZE-Q       # board specifies indirectly also the device

  - type: Production-HW
    device: STM32L552RC          # specifies device
      
build-types:
  - type: Debug
    optimize: debug
    debug: on

  - type: Test
    optimize: max
    debug: on
```

The `board:`, `device:`, and `processor:` settings are used to configure the code translation for the toolchain.  These settings are processed in the following order:

1. `board:` relates to a BSP software pack that defines board parameters, including the [mounted device](https://arm-software.github.io/CMSIS_5/Pack/html/pdsc_boards_pg.html#element_board_mountedDevice).  If `board:` is not specified, a `device:` most be specified.
2. `device:` defines the target device.  If `board:` is specified, the `device:` setting can be used to overwrite the device or specify the processor core used.
3. `processor:` overwrites default settings for code generation, such as endianess, TrustZone mode, or disable Floating Point code generation.

**Examples:**

```yml
target-types:
  - type: Production-HW
    board: NUCLEO-L552ZE-Q    # hardware is similar to a board (to use related software layers)
    device: STM32L552RC       # but uses a slightly different device
    processor: 
      trustzone: off          # TrustZone disabled for this project
```

```yml
target-types:
  - type: Production-HW
    board: FRDM-K32L3A6       # NXP board with K32L3A6 device
    device: :cm0plus          # use the Cortex-M0+ processor
```

## Build/Target-Type control

Depending on a *`target-type`* or *`build-type`* selection it is possible to include or exclude *items* in the build process.

Keyword         | Description
:---------------|:------------------------------------
`for-type:`     | A *type list* that adds an *item* for a specific target or build *type* or a list of *types*.
`not-for-type:` | A *type list* that removes an *item* for a specific target or build *type* or a list of *types*.

It is possible to specify only a `<build-type>`, only a `<target-type>` or a combination of `<build-type>` and `<target-type>`. It is also possible to provide a list of *build* and *target* types. The *type list syntax* for `for-type:` or `not-for-type:` is:

`[.<build-type>][+<target-type>] [, [.<build-type>]+[<target-type>]] [, ...]`

**Examples:**

```yml
for-type:      .Test                            # add item for build-type: Test (any target-type)
not-for-type:  +Virtual                         # remove item for target-type: Virtual (any build-type)
not-for-type:  .Release+Virtual                 # remove item for build-type: Release with target-type: Virtual
for-type:      .Debug, .Release+Production-HW   # add for build-type: Debug and build-type: Release / target-type: Production-HW
```

The keyword `for-type:` or `not-for-type:` can be applied to the following list keywords:

Keyword      | Description
:------------|:------------------------------------
`project:`   | At `solution:` level it is possible to control inclusion of project.
`group:`     | At `group:` level it is possible to control inclusion of a file group.
`file:`      | At `file:` level it is possible to control inclusion of a file.
`layer:`     | At `layer:` level it is possible to control inclusion of a software layer.
`component:` | At `component:` level it is possible to control inclusion of a software component.

## Groups and Files

**YML structure:**

```yml
groups:                # Start a list of groups
  - group:             # name of the group
    for-type:          # include group for a list of *build* and *target* types.
    not-for-type:      # exclude group for a list of *build* and *target* types.
    optimize:          # optimize level for code generation (optional)
    debug:             # generation of debug information (optional)
    defines:           # define symbol settings for code generation (optional).
    undefines:         # remove define symbol settings for code generation (optional).
    add-paths:         # additional include file paths (optional).
    del-paths:         # remove specific include file paths (optional). 
    misc:              # Literal tool-specific controls
    groups:            # Start a nested list of groups
      - group:         # name of the group
         :
    files:             # Start a nested list of files
      - file:          # file name along with path
        for-type:      # include group for a list of *build* and *target* types.
        not-for-type:  # exclude group for a list of *build* and *target* types.
        optimize:      # optimize level for code generation (optional)
        debug:         # generation of debug information (optional)
        defines:       # define symbol settings for code generation (optional).
        undefines:     # remove define symbol settings for code generation (optional).
        add-paths:     # additional include file paths (optional).
        del-paths:     # remove specific include file paths (optional). 
        misc:          # Literal tool-specific controls.
```

**Example:**

Add source files to a project or a software layer.  Used in `*.cproject.yml` and `*.clayer.yml` files.

```yml
groups:
  - group:  "Main File Group"
    not-for-type: .Release+Virtual, .Test-DSP+Virtual, +Board
    files: 
      - file: file1a.c
      - file: file1b.c
        defines:
          - a: 1
        undefines:
          - b
        optimize: size

  - group: "Other File Group"
    files:
      - file: file2a.c
        for-type: +Virtual
        defines: 
          - test: 2
      - file: file2a.c
        not-for-type: +Virtual
      - file: file2b.c

  - group: "Nested Group"
    groups:
      - group: Subgroup1
        files:
          - file: file-sub1-1.c
          - file: file-sub1-2.c
      - group: Subgroup2
        files:
          - file: file-sub2-1.c
          - file: file-sub2-2.c
```

## Layers 

Add a software layer to a project.  Used in `*.cproject.yml` files.

**YML structure:**

```yml
layers:                # Start a list of layers
  - layer:             # path to the `*.clayer.yml` file that defines the layer (required).
    for-type:          # include layer for a list of *build* and *target* types (optional).
    not-for-type:      # exclude layer for a list of *build* and *target* types (optional).
    optimize:          # optimize level for code generation (optional).
    debug:             # generation of debug information (optional).
    defines:           # define symbol settings for code generation (optional).
    undefines:         # remove define symbol settings for code generation (optional).
    add-paths:         # additional include file paths (optional).
    del-paths:         # remove specific include file paths (optional). 
    misc:              # Literal tool-specific controls.
```

## Components

Add software components to a project or a software layer.  Used in `*.cproject.yml` and `*.clayer.yml` files.

**YML structure:**

```yml
components:            # Start a list of layers
  - component:         # name of the software component.
    for-type:          # include layer for a list of *build* and *target* types (optional).
    not-for-type:      # exclude layer for a list of *build* and *target* types (optional).
    optimize:          # optimize level for code generation (optional).
    debug:             # generation of debug information (optional).
    defines:           # define symbol settings for code generation (optional).
    undefines:         # remove define symbol settings for code generation (optional).
    add-paths:         # additional include file paths (optional).
    del-paths:         # remove specific include file paths (optional). 
    misc:              # Literal tool-specific controls.
```

**NOTE:**

- The name of the software component is specified as described under [Name Conventions - 
Component Names](#Component_Names)

## Defines

Add symbol #define statements to the command line of the development tools.

**YML structure:**

```yml
defines:                   # Start a list of define statements
  - name: value            # add a symbol with optional value.
  - name:
```

## Undefines

Remove symbol #define statements from the command line of the development tools.

**YML structure:**

```yml
undefines:                 # Start a list of undefine statements
  - name                   # remove symbol from the list of define statements.
  - name                   # remove a symbol.
```

## Add-Paths

Add include paths to the command line of the development tools.

**YML structure:**
```yml
add-paths:                 # Start a list path names that should be added to the include file search
  - path                   # add path name
  - path
```

## Del-Paths

Remove include paths (that are defined at the cproject level) from the command line of the development tools.

**YML structure:**
```yml
del-paths:                 # Start a list of path names that should be removed from the include file search
  - path                   # remove path name
  - *                      # remove all paths
```



## Misc

Add tool specific controls as literal strings that are directly passed to the individual tools.

**YML structure:**
```yml
misc:                      # Start a list of literal control strings that are directly passed to the tools.
  - compiler:              # select the toolchain that the literal control string applies too (AC6, IAR, GCC).
    C: string              # applies to *.c files only.
    CPP: string            # applies to *.cpp files only.
    C*: string             # applies to *.c and *.cpp files.
    ASM: string            # applies to assembler source files
    Link: string           # applies to the linker
    Lib: string            # applies to the library manager or archiver
```


## Solution: Collection of related Projects

The section [Project setup for related projects](#project-setup-for-related-projects) describes the collection of related projects.  The file `*.csolution.yml` describes the relationship of this projects.  This file may also define [Target and Build Types](#target-and-build-types) before the section `solution:`.

The YML structure of the section `solution:` is:

```yml
solution:                  # Start a list of projects.
  - project:               # path to the project file (required).
    for-type:              # include project for a list of *build* and *target* types (optional).
    not-for-type:          # exclude project for a list of *build* and *target* types (optional).
    compiler:              # specify a specific compiler
    optimize:              # optimize level for code generation (optional)
    debug:                 # generation of debug information (optional)
    defines:               # define symbol settings for code generation (optional).
    undefines:             # remove define symbol settings for code generation (optional).
    add-paths:             # additional include file paths (optional).
    del-paths:             # remove specific include file paths (optional). 
    misc:                  # Literal tool-specific controls
```

## Project Definition

Start of a project definition in a `*.cproject.yml` file (optional section)

The YML structure of the section `project:` is:
```yml
project:                     # do we need this section at all, perhaps for platform?
  compiler: name             # specify compiler (AC6, GCC, IAR)
  description:               # project description (optional)
  output-type: lib | exe     # generate executable (default) or library
  processor:                 # specify processor attributes 
  groups:                    # source files organized in groups
  layers:                    # software layers of pre-configured software components
  components:                # software components
```  


## Processor Attributes

The `processor:` keyword defines attributes such as TrustZone and FPU register usage.

```yml
  processor:               # processor specific settings
    trustzone: secure      # TrustZone mode: secure | non-secure | off
    fpu: on | off          # control usage of FPU registers (S-Register for Helium/FPU) (default: on for devices with FPU registers)
    endian: little | big   # select endianess (only available when devices are configureable)
```

**Note:**
- Default for `trustzone:` 
   - `off` for devices that support this option.
   - `non-secure` for devices that have TrustZone enabled.
   

# Pre/Post build steps

Tbd: potentially map to CMake add_custom_command.

```yml
- execute: description      # execute an external command with description
  os: Linux                 # executed on which operating systems (if omitted it is OS independent)
  run:                      # tool name that should be executed, optionally with path to the tool
  args:                     # tool arguments
  stop:                     # stop on exit code
```

Potential usage before/after build

```yml
solution:
  - execute: Generate Keys for TF-M
    os: Linux
    run: KeyGen.exe
  - project: /security/TFM.cproject.yml
  - project: /application/MQTT_AWS.cproject.yml
  - execute: Copy output files
    run: cp *.out .\output
```

Potential usage during build steps
```yml
groups:
  - group:  "Main File Group"
    files: 
      - execute: Generate file1a.c
        run: xyz.exe
        ....
      - file: file1a.c
```

# Layer

Start of a layer definition in a `*.clayer.yml` file.


**Example:**



```yml
project:
  compiler: AC6

  layers:
    - layer: ..\layer\App\Blinky.clayer.yml
    - layer: ..\layer\Board+NUCELO-G474RE\Nucelo-G474RE.clayer.yml      # 'NUCLEO-G474RE' is target-type
    - layer: ..\layer\RTOS\RTX.clayer.yml
```    



```yml
layer: App
# name: Blinky
#  description: Simple example

# interfaces:
#   consumes:
#     - C_VIO:
#     - RTOS2:

  groups:
    - group: App
      files:
        - file: "./Blinky.c"
    - group: Documentation
      files:
        - file: name="./README.md"
```

```yml
layer:
  type: RTOS
  description: Keil RTX5 open-source real-time operating system with CMSIS-RTOS v2 API
  
interface:
  provides:
    - RTOS2:
    
components:
  component: CMSIS:RTOS2:Keil RTX5&Source
# open how do we capture versions of config files?
```

```yml
layer: Board
  name: NUCLEO-G474RE
  description: Board setup with interfaces 
  
interfaces:
  consumes:
    - RTOS2:
  provides:
    - C_VIO:
    - A_IO9_I:
    - A_IO10_O:
    - C_VIO:
    - STDOUT:
    - STDIN:
    - STDERR:
    - Heap: 65536

target-types:
  - type: NUCLEO-G474RE
    board:NUCLEO-G474RE

components:
  component: CMSIS:CORE
  component: CMSIS Driver:USART:Custom
  component: CMSIS Driver:VIO:Board&NUCLEO-G474RE
  component: Compiler&ARM Compiler:Event Recorder&DAP
    # Config/EventRecorderConf.h version="1.1.0"
  component: Compiler&ARM Compiler:I/O:STDERR&User
  component: Compiler&ARM Compiler:I/O:STDIN&User
  component: Compiler&ARM Compiler:I/O:STDOUT&User
  component: Board Support&NUCLEO-G474RE:Drivers:Basic I/O
    # Drivers/Config/stm32g4xx_nucleo_conf.h version="1.0.0"
  component: Device&STM32CubeMX:STM32Cube Framework:STM32CubeMX
  component: Device&STM32CubeMX:STM32Cube HAL
  component: Device&STM32CubeMX:Startup

groups:
  - group: Board IO
    files:
      - file: ./Board_IO/arduino.c
      - file: ./Board_IO/arduino.h
      - file: ./Board_IO/retarget_stdio.c
  - group: STM32CubeMX
    files:
      - file: ./RTE/Device/STM32G474RETx/STCubeGenerated/STCubeGenerated.ioc
```



Todo: work on this

The YML structure of the section `layer:` is:
```yml
layer:
  description:
  ....
```  


# Proposals


## Output versions to *.cprj

The ProjMgr should always generate *.cprj files that contain version information about the used software packs.

## CMSIS-Zone Integration

Add to the project the possiblity to specify.  The issue might be that the project files become overwhelming, alternative is to keep partitioning in separate files.

```yml
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
        phase: Boot
        size: 128k
    
peripherals:           # specifies the requried peripherals
  - peripheral: I2C0
    permission: rw, s
```

## Layer Interface Defintions

A software layer could specify the interfaces that it provides.  The interface specification indicates also the configuration of the layer.  Issue might be that a standardization across the industry is required.

```yml
interfaces:
  consumes:
    - RTOS2:     # requires CMSIS-RTOS2 features
  provides:
    - C_VIO:     # provides CMSIS-VIO interface for LEDs
    - A_IO9_I:   # provides Audrino connector pin I09 configured as input
    - A_IO10_O:  # provides Audrino connector pin I10 configured as output
    - STDOUT:    # redirects STDIO
    - STDIN:
    - STDERR:
    - Heap: 65536 # provides 64K heap
```

## CMSIS-Pack extensions

### Board condition

It should be possible to use conditions to filter against a selected board name.

### Layers in packs

A layer is a set of pre-configured software components. It should be possible to store a layer in a pack and apply filter conditions to it.  In combination with interfaces specifications, an interactive IDE should be able to display suitable layers that could be added to an application.


