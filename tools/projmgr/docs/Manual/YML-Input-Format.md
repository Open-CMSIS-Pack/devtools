# YML Input Format

<!-- markdownlint-disable MD009 -->
<!-- markdownlint-disable MD013 -->
<!-- markdownlint-disable MD036 -->

The following chapter explains the YML format that is used to describe the YML input files for the **CSolution**
Project Manager.

**Table of Contents**

- [YML Input Format](#yml-input-format)
  - [Name Conventions](#name-conventions)
    - [Pack Name Conventions](#pack-name-conventions)
    - [Component Name Conventions](#component-name-conventions)
    - [Device Name Conventions](#device-name-conventions)
    - [Board Name Conventions](#board-name-conventions)
  - [Access Sequences](#access-sequences)
  - [Overall File Structure](#overall-file-structure)
    - [`default:`](#default)
    - [`solution:`](#solution)
    - [`project:`](#project)
    - [`layer:`](#layer)
  - [List Nodes](#list-nodes)
  - [Directory Control (Proposal)](#directory-control-proposal)
    - [`output-dirs:`](#output-dirs)
    - [`rte-dirs:`](#rte-dirs)
    - [`output-type:`](#output-type)
    - [`linker:`](#linker)
    - [`for-compiler:`](#for-compiler)
  - [Translation Control](#translation-control)
    - [`language-C:`](#language-c)
    - [`language-CPP:`](#language-cpp)
    - [`optimize:`](#optimize)
    - [`debug:`](#debug)
    - [`warnings:`](#warnings)
    - [`define:`](#define)
    - [`undefine:`](#undefine)
    - [`add-path:`](#add-path)
    - [`del-path:`](#del-path)
    - [`misc:`](#misc)
  - [Project Setups](#project-setups)
    - [`setups:`](#setups)
  - [Pack Selection](#pack-selection)
    - [`packs:`](#packs)
    - [`pack:`](#pack)
  - [Target Selection](#target-selection)
    - [`board:`](#board)
    - [`device:`](#device)
  - [Processor Attributes](#processor-attributes)
    - [`processor:`](#processor)
  - [Target and Build Types](#target-and-build-types)
    - [`target-types:`](#target-types)
    - [`build-types:`](#build-types)
  - [Build/Target-Type control](#buildtarget-type-control)
    - [`for-type:`](#for-type)
    - [`not-for-type:`](#not-for-type)
      - [type list](#type-list)
      - [list nodes](#list-nodes-1)
  - [Related Projects](#related-projects)
    - [`projects:`](#projects)
  - [Source File Management](#source-file-management)
    - [`groups:`](#groups)
    - [`files:`](#files)
    - [`layers:`](#layers)
      - [Proposals for `layers:`](#proposals-for-layers)
    - [`components:`](#components)
    - [`instances:`](#instances)
  - [Pre/Post build steps](#prepost-build-steps)
    - [`execute:`](#execute)
  - [`interface:`](#interface)
    - [`provides:`](#provides)
      - [Proposal: Export symbols to RTE_components.h](#proposal-export-symbols-to-rte_componentsh)
    - [`consumes:`](#consumes)
      - [Proposal: for interface matching](#proposal-for-interface-matching)
  - [Generator (Proposal)](#generator-proposal)
    - [Workflow assumptions](#workflow-assumptions)
    - [Steps for component selection and configuration](#steps-for-component-selection-and-configuration)
    - [Enhance Usability](#enhance-usability)
    - [Workflow](#workflow)
      - [Example Content of `*.cgen.json` (in this case `STM32CubeMX.cgen.json`)](#example-content-of-cgenjson-in-this-case-stm32cubemxcgenjson)
    - [Changes to the *.GPDSC file](#changes-to-the-gpdsc-file)
    - [Changes to the *.PDSC file](#changes-to-the-pdsc-file)
  - [Resource Management (Proposal)](#resource-management-proposal)
    - [`resources:`](#resources)
      - [`- import:`](#--import)
      - [`- split:`](#--split)
      - [`- combine`](#--combine)
    - [`phases:`](#phases)
    - [`project-zones:`](#project-zones)
      - [`requires:`](#requires)
        - [`- memory:`](#--memory)
        - [`- peripheral:`](#--peripheral)

## Name Conventions

### Pack Name Conventions

The CMSIS Project Manager uses the following syntax to specify the `pack:` names in the `*.yml` files.

```text
vendor [:: pack-name [@[~ | >=] version] ]
```

Element      |              | Description
:------------|--------------|:---------------------
`vendor`     | **Required** | Vendor name of the software pack.
`pack-name`  | Optional     | Name of the software pack; wildcards (\*, ?) can be used.
`version`    | Optional     | Version number of the software pack, with `@1.2.3` that must exactly match, `@~1.2`/`@~1` that matches with semantic versioning, or `@>=1.2.3` that allows any version higher or equal.

**Examples:**

```yml
- pack:   ARM::CMSIS@5.5.0                  # 'CMSIS' Pack (with version 5.5.0)
- pack:   Keil::MDK-Middleware@>=7.13.0     # 'MDK-Middleware' Software Pack from vendor Keil (with version 7.13.0 or higher)
- pack:   AWS                               # All Software Packs from vendor 'AWS'
- pack:   Keil::STM*                        # All Software Packs that start with 'STM' from vendor 'Keil'
```

### Component Name Conventions

The CMSIS Project Manager uses the following syntax to specify the `component:` names in the `*.yml` files.

```text
[Cvendor::] Cclass [&Cbundle] :Cgroup [:Csub] [&Cvariant] [@[~ | >=]Cversion]
```

Components are defined using the [Open-CMSIS-Pack - `<component>` element](https://open-cmsis-pack.github.io/Open-CMSIS-Pack-Spec/main/html/pdsc_components_pg.html#element_component). Several parts of a `component` are optional.  For example it is possible to just define a component using `Cclass` and `Cgroup` name. All elements of a component name are summarized in the following table.

Element    |              | Description
:----------|--------------|:---------------------
`Cvendor`  | Optional     | Name of the component vendor as defined in `<components>` element or by the package vendor of the software pack.
`Cclass`   | **Required** | Component class name as defined in `<components>` element of the software pack.
`Cbundle`  | Optional     | Bundle name of the component class as defined in `<bundle>` element of the software pack.
`Cgroup`   | **Required** | Component group name as defined in `<components>` element of the software pack.
`Csub`     | Optional     | Component sub-group name as defined in `<components>` element of the software pack.
`Cvariant` | Optional     | Component sub-group name as defined in `<components>` element of the software pack.
`Cversion` | Optional     | Version number of the component, with `@1.2.3` that must exactly match, `@~1.2`/`@~1` that matches with semantic versioning, or `@>=1.2.3` that allows any version higher or equal.

**Partly defined components**

A component can be partly defined in `csolution` input files (`*.cproject.yml`, `*.clayer.yml`, `*.genlayer.yml`) by omitting `Cvendor`, `Cvariant`, and `Cversion`, even when this are part of the `components` element of the software pack. The component select algorithm resolves this to a fully defined component by:

- verify that the partly specified component resolves to only one possible choice, otherwise an *error* is issued.
- extended the partly specified component by:
  - version information from the software pack.
  - default variant definition from the software pack.

The fully resolved component name is shown in the [`*.cbuild.yml`](YML-CBuild-Format.md) output file.

**Multiple component definitions are rejected**

- If a component is added more then once in the `csolution` input files and an *error* is issued.
- An attempt to select multiple variants (using `Cvariant`) of a component results in an *error*.

**Examples:**

```yml
- component: CMSIS:CORE                               # CMSIS Core component (vendor selected by `csolution` ARM)
- component: ARM::CMSIS:CORE                          # CMSIS Core component from vendor ARM (any version)
- component: ARM::CMSIS:CORE@5.5.0                    # CMSIS Core component from vendor ARM (with version 5.5.0)
- component: ARM::CMSIS:CORE@>=5.5.0                  # CMSIS Core component from vendor ARM (with version 5.5.0 or higher)

- component: Device:Startup                           # Device Startup component from any vendor

- component: CMSIS:RTOS2:Keil RTX5                    # CMSIS RTOS2 Keil RTX5 component with default variant (any version)
- component: ARM::CMSIS:RTOS2:Keil RTX5&Source@5.5.3  # CMSIS RTOS2 Keil RTX5 component with variant 'Source' and version 5.5.3

- component: Keil::USB&MDK-Pro:CORE&Release@6.15.1    # USB CORE component from bundle MDK-Pro in variant 'Release' and version 6.15.1
```

### Device Name Conventions

The device specifies multiple attributes about the target that ranges from the processor architecture to Flash
algorithms used for device programming. The following syntax is used to specify a `device:` value in the `*.yml` files.

```text
[Dvendor:: [device_name] ] [:Pname]
```

Element       |          | Description
:-------------|----------|:---------------------
`Dvendor`     | Optional | Name (without enum field) of the device vendor defined in `<devices><family>` element of the software pack.
`device_name` | Optional | Device name (Dname attribute) or when used the variant name (Dvariant attribute) as defined in the \<devices\> element.
`Pname`       | Optional | Processor identifier (Pname attribute) as defined in the `<devices>` element.

> **Note:**
>
> - All elements of a device name are optional which allows to supply additional information, such as the `:Pname` at
>   different stages of the project. However the `device_name` itself is a mandatory element and must be specified in
>   context of the various project files.
> - `Dvendor::` must be used in combination with the `device_name`.

**Examples:**

```yml
device: NXP::LPC1768                       # The LPC1788 device from NXP
device: LPC1788                            # The LPC1788 device (vendor is evaluated from DFP)
device: LPC55S69JEV98                      # Device name (exact name as defined in the DFP)
device: LPC55S69JEV98:cm33_core0           # Device name (exact name as defined in the DFP) with Pname specified
device: :cm33_core0                        # Pname added to a previously defined device name (or a device derived from a board)
```

### Board Name Conventions

Evaluation Boards define indirectly a device via the related BSP. The following syntax is used to specify a `board:`
value in the `*.yml` files.

```text
[vendor::] board_name [:revision] 
```

Element      |              | Description
:------------|--------------|:---------------------
`vendor`     | Optional     | Name of the board vendor defined in `<boards><board>` element of the board support pack (BSP).
`board_name` | **Required** | Board name (name attribute) as defined in the \<board\> element of the BSP.
`revision`   | Optional     | Board revision (revision attribute) as defined in the \<board\> element of the BSP.

> **Note:** When a `board:` is specified, the `device:` specification can be omitted, however it is possible to
  overwrite the device setting in the BSP with an explicit `device:` setting.

**Examples:**

```yml
board: Keil::MCB54110                             # The Keil MCB54110 board (with device NXP::LPC54114J256BD64) 
board: LPCXpresso55S28                            # The LPCXpresso55S28 board
board: STMicroelectronics::NUCLEO-L476RG:Rev.C    # A board with revision specification
```

## Access Sequences

The following **access sequences** allow to use arguments from the CMSIS Project Manager as arguments of the various
`*.yml` files in the key values for `define:`, `add-path:`, `misc:`, `files:`, and `execute:`. The **access sequences**
can refer in a different project and provide therefore a method to describe project dependencies.

Access Sequence                                | Description
:----------------------------------------------|:--------------------------------------
`$Bname$`                                      | Board name of the selected board.
`$Dname$`                                      | Device name of the selected device.
`$Project$`                                    | Project name of the currently process project.
`$BuildType$`                                  | [Build-type](#build-types) name of the currently process project.
`$TargetType$`                                 | [Target-type](#target-types) name of the currently process project.
`$Compiler$`                                   | [Compiler](#compiler) name of the compiler used in this project context.
`$Output(project[.build-type][+target-type])$` | Path to the output file of a related project that is defined in the `*.csolution.yml` file.
`$OutDir(project[.build-type][+target-type])$` | Path to the output directory of a related project that is defined in the `*.csolution.yml` file.
`$Source(project[.build-type][+target-type])$` | Path to the source directory of a related project that is defined in the `*.csolution.yml` file.

The `.build-type` and `+target-type` can be explicitly specified. When omitted the `.build-type` and/or `+target-type`
of the current processed context is used.

> **Note:** The access sequences below are not completed yet, as they require a change to CMSIS-Build.

Access Sequence                                | Description
:----------------------------------------------|:--------------------------------------
`$Bpack$`                                      | Path to the pack that defines the selected board (BSP).
`$Dpack$`                                      | Path to the pack that defines the selected device (DFP).
`$PackRoot$`                                   | Path to the CMSIS Pack Root directory.
`$Pack(vendor.name)$`                          | Path to specific pack [with latest version ToDo: revise wording]. Example: `$Pack(NXP.K32L3A60_DFP)$`.

**Example:**

For the example below we assume the following `build-type`, `target-type`, and `projects` definitions.

```yml
solution:
  target-types:
    - type: Board
      board: NUCLEO-L552ZE-Q    # specifies board

    - type: Production-HW
      device: STM32U5X          # specifies device
      
  build-types:
    - type: Debug
      optimize: none
      debug: on

    - type: Release
      optimize: size

  projects:
    - project: ./bootloader/Bootloader.cproject.yml           # relative path
    - project: /MyDevelopmentTree/security/TFM.cproject.yml   # absolute path
    - project: ./application/MQTT_AWS.cproject.yml            # relative path
```

The `project: /application/MQTT_AWS.cproject.yml` can now use **Access Sequences** to reference files or directories in
other projects that belong to a solution. For example, these references are possible in the file `MQTT_AWS.cproject.yml`.

The example below uses the `build-type` and `target-type` of the current processed context. In practice this means that
the same `build-type` and `target-type` is used as for the `MQTT_AWS.cproject.yml` project.

```yml
    files:
    - file: `$Output(TFM)$`.o                             # use the symbol output file of the TFM Project
```

The example below uses from the TFM project always `build-type: Debug` and the `target-type: Production-HW`.

```yml
    files:
    - file: `$Output(TFM.Release+Production-HW)$`.o       # use the symbol output file of the TFM Project
```

The example below uses the `build-type: Debug`. The `target-type` of the current processed context is used.

```yml
  - execute: Generate Image
    os: Windows                           # on Windows run from
    run: $DPack$/bin/gen_image.exe        # DFP the get_image tool
    arg: -input $Output(TFM.Debug).axf -output $Output(TFM.Debug)
```

The example below creates a `define` that uses the device name.

```yml
groups:
  - group:  "Main File Group"
    define:
      - $Dname$                           # Generate a #define 'device-name' for this file group
```

## Overall File Structure

The table below explains the top-level elements in each of the different `*.yml` input files.

Keyword                          | Description
:--------------------------------|:------------------------------------
[`default:`](#default)           | Start of `*.cdefault.yml` file that provides global settings.
[`solution:`](#solution)         | Start of `*.csolution.yml` file that [collects related projects](Overview.md#solution-collection-of-related-projects) along with build order.
[`project:`](#project)           | Start of `*.cproject.yml` file that that defines a project that can be independently generated.
[`layer:`](#layer)               | Start of `*.clayer.yml` file that contains pre-configured software components along with source files.

### `default:`

The `default:` node is the start of a `*.cdefaults.yml` file and contain the following.

`default:`                                            | Content
:-----------------------------------------------------|:------------------------------------
&nbsp;&nbsp; [`compiler:`](#compiler)                 | Default toolchain selection.
&nbsp;&nbsp; [`build-types:`](#build-types)           | List of build-types (i.e. Release, Debug, Test).
&nbsp;&nbsp; [`language-C:`](#language-c)             | Set the language standard for C source file compilation.
&nbsp;&nbsp; [`language-CPP:`](#language-cpp)         | Set the language standard for C++ source file compilation.

**Example:**

```yml
default:
  compiler: AC6                   # use Arm Compiler 6 as default
```

### `solution:`

The `solution:` node is the start of a `*.csolution.yml` file that collects related projects as described in the section
["Project setup for related projects"](Overview.md#project-setup-for-related-projects).

`solution:`                                           | Content
:-----------------------------------------------------|:------------------------------------
&nbsp;&nbsp; [`packs:`](#packs)                       | Defines local packs and/or scope of packs that are used.
&nbsp;&nbsp; [`output-dirs:`](#output-dirs)           | Control the output directories for generating the csolution.
&nbsp;&nbsp; [`language-C:`](#language-c)             | Set the language standard for C source file compilation.
&nbsp;&nbsp; [`language-CPP:`](#language-cpp)         | Set the language standard for C++ source file compilation.
&nbsp;&nbsp; [`compiler:`](#compiler)                 | Overall toolchain selection for the solution.
&nbsp;&nbsp; [`target-types:`](#target-types)         | List of target-types that define the target system (device or board).
&nbsp;&nbsp; [`build-types:`](#build-types)           | List of build-types (i.e. Release, Debug, Test).
&nbsp;&nbsp; [`projects:`](#projects)                 | List of projects that belong to the solution.

**Example:**

```yml
solution:
  compiler: GCC                 # overwrite compiler definition in 'cdefaults.yml'

  packs: 
    - pack: ST                  # add ST packs in 'cdefaults.yml'

  build-types:                  # additional build types
    - type: Test
      optimize: none
      debug: on
      packs:                    # with explicit pack specification
        - pack: ST::TestSW
          path: .\MyDev\TestSW    

  target-types:
    - type: Board
      board: NUCLEO-L552ZE-Q

    - type: Production-HW
      device: STM32U5X          # specifies device
      
  projects:
    - project: ./blinky/Bootloader.cproject.yml
    - project: /security/TFM.cproject.yml
    - project: /application/MQTT_AWS.cproject.yml
```

### `project:`

The `project:` node is the start of a `*.cproject.yml` file and can contain the following:

`project:`                                          |              | Content
:---------------------------------------------------|:-------------|:------------------------------------
&nbsp;&nbsp; `description:`                         |  Optional    | Project description.
&nbsp;&nbsp; [`language-C:`](#language-c)           |  Optional    | Set the language standard for C source file compilation.
&nbsp;&nbsp; [`language-CPP:`](#language-cpp)       |  Optional    | Set the language standard for C++ source file compilation.
&nbsp;&nbsp; [`output-type:`](#output-type)         |  Optional    | Generate executable (default) or library.
&nbsp;&nbsp; [`optimize:`](#optimize)               |  Optional    | Optimize level for code generation.
&nbsp;&nbsp; [`linker:`](#linker)                   | **Required** | Instructions for the linker.
&nbsp;&nbsp; [`debug:`](#debug)                     |  Optional    | Generation of debug information.
&nbsp;&nbsp; [`define:`](#define)                   |  Optional    | Preprocessor (#define) symbols for code generation.
&nbsp;&nbsp; [`undefine:`](#undefine)               |  Optional    | Remove preprocessor (#define) symbols.
&nbsp;&nbsp; [`add-path:`](#add-path)               |  Optional    | Additional include file paths.
&nbsp;&nbsp; [`del-path:`](#del-path)               |  Optional    | Remove specific include file paths.
&nbsp;&nbsp; [`misc:`](#misc)                       |  Optional    | Literal tool-specific controls.
&nbsp;&nbsp; [`processor:`](#processor)             |  Optional    | Processor specific settings.
&nbsp;&nbsp; [`groups:`](#groups)                   | **Required** | List of source file groups along with source files.
&nbsp;&nbsp; [`components:`](#components)           |  Optional    | List of software components used.
&nbsp;&nbsp; [`layers:`](#layers)                   |  Optional    | List of software layers that belong to the project.
&nbsp;&nbsp; [`interface:`](#interface)             |  Optional    | List of consumed and provided interfaces.

**Example:**

```yml
project:
  misc:
    - compiler: AC6                          # specify misc controls for Arm Compiler 6
      C: [-fshort-enums, -fshort-wchar]      # set options for C files
  add-path:
    - $OutDir(Secure)$                       # add the path to the secure project's output directory
  components:
    - component: Startup                     # Add startup component
    - component: CMSIS CORE
    - component: Keil RTX5 Library_NS
  groups:
    - group: Non-secure Code                 # Create group
      files: 
        - file: main_ns.c                    # Add files to group
        - file: $Source(Secure)$interface.h
        - file: $Output(Secure)$_CMSE_Lib.o
```

### `layer:`

The `layer:` node is the start of a `*.clayer.yml` file and defines a [Software Layer](./Overview.md#software-layers). It can contain the following nodes:

`layer:`                                       |              | Content
:----------------------------------------------|:-------------|:------------------------------------
&nbsp;&nbsp; `type:`                           |  Optional    | Layer type for combining layers.
&nbsp;&nbsp; `name:`                           |  Optional    | Brief descriptive name of the layer
&nbsp;&nbsp; `description:`                    |  Optional    | Detailed layer description.
&nbsp;&nbsp; `for-device:`                     |  Optional    | Device information, used for consistency check (device selection is in `*.csolution.yml`).
&nbsp;&nbsp; `for-board:`                      |  Optional    | Board information, used for consistency check (board selection is in `*.csolution.yml`).
&nbsp;&nbsp; [`interface:`](#interface)        |  Optional    | List of consumed and provided interfaces.
&nbsp;&nbsp; [`processor:`](#processor)        |  Optional    | Processor specific settings.
&nbsp;&nbsp; [`groups:`](#groups)              |  Optional    | List of source file groups along with source files.
&nbsp;&nbsp; [`components:`](#components)      |  Optional    | List of software components used.

**Example:**

```yml
layer:
  type: Board
  name: AVH_MPS3_Cortsone-300
  description: Board setup with Ethernet and WiFi interface
  processor:
    trustzone: secure        # set processor to secure
  components:
    - component: Startup
    - component: CMSIS CORE
  groups:
    - group: Secure Code
      files: 
        - file: main_s.c
    - group: CMSE
      files: 
        - file: interface.c
        - file: interface.h
        - file: tz_context.c
```

## List Nodes

The key/value pairs in a list node can be in any order.  The two following list nodes are logically identical. This
might be confusing for `yml` files that are generated by an IDE.

```yml
build-types:
    - type: Release         # build-type name
      optimize: size        # optimize for size
      debug: off            # generate no debug information for the release build
```

```yml
build-types:
    - debug: off            # generate no debug information for the release build
      optimize: size        # optimize for size
      type: Release         # build-type name
```

## Directory Control (Proposal)

The following node allows to control the directories used to generate the output files.  

>**Note:** This control is only possible at `csolution.yml` level.

### `output-dirs:`

`output-dirs:`               |              | Content
:----------------------------|--------------|:------------------------------------
&nbsp;&nbsp; [`cprjdir:`]    |  Optional    | Specifies the directory for the *.CPRJ files.
&nbsp;&nbsp; [`rtedir:`]     |  Optional    | Specifies the directory for the RTE files.
&nbsp;&nbsp; [`intdir:`]     |  Optional    | Specifies the directory for the interim files.
&nbsp;&nbsp; [`outdir:`]     |  Optional    | Specifies the directory for the build output files.

The default setting for the `output-dirs:` are:

```yml
cprjdir: <cproject.yml base directory>
rtedir:  <cproject.yml base directory>/RTE
intdir:  <csolution.yml base directory>/tmp/$Project$/$TargetType$/$BuildType$
outdir:  <csolution.yml base directory>/out/$Project$/$TargetType$/$BuildType$
```

**Example:**

```yml
output-dirs:
  cprjdir: ./cprj                        # relative path to csolution.yml file
  rtedir: ./$Project$/RTE2               # alternative path for RTE files
  outdir: ./out/$Project$/$TargetType$   # $BuildType$ no longer part of the outdir    
```

### `rte-dirs:`

**(Proposal)**
The `rte-dirs:` list allows to control the location of configuration files for each [component `Cclass`](#component-name-conventions).  A list of `Cclass` names can be assigned to specific directories that store the related configuration files.

**Example:**

```yml
  rte-dirs:
    - Board_Support: ..\common\RTE\Board
    - CMSIS Driver:  ..\common\RTE\CMSIS_Driver
    - Device:        ..\common\RTE\Device\Core1
    - Compiler:      ..\RTE\Compiler\Debug
      for-type:      .Debug

## Toolchain Options

The following code translation options may be used at various places such as:

- [`solution:`](#solution) level to specify options for a collection of related projects
- [`project:`](#projects) level to specify options for a project

### `compiler:`

Selects the compiler toolchain used for code generation.
Optionally the compiler can have a version number specification.

Value                                                 | Supported Compiler
:-----------------------------------------------------|:------------------------------------
`AC6`                                                 | Arm Compiler version 6
`GCC`                                                 | GCC Compiler
`IAR`                                                 | IAR Compiler

**Example:**

```yml
compiler: GCC              # Select GCC Compiler
```

```yml
compiler: AC6@6.18.0       # Select Arm Compiler version 6
```

### `output-type:`

Selects the output type for code generation.

Value                                                 | Generated Output
:-----------------------------------------------------|:------------------------------------
`exe`                                                 | Executable in ELF format
`lib`                                                 | Library or archive

**Example:**

```yml
output-type: lib            # Generate a library
```

### `linker:`

The `linker:` list node controls the linker operation.

`linker:`                                             |              |  Content
:-----------------------------------------------------|:-------------|:--------------------------------
`- script:`                                           |              | Explicit file name of the linker script
&nbsp;&nbsp; [`for-compiler:`](#for-compiler)         |   Optional   | Include script for the specified toolchain.
&nbsp;&nbsp; [`for-type:`](#for-type)                 |   Optional   | Include script for a list of *build* and *target* types.
&nbsp;&nbsp; [`not-for-type:`](#not-for-type)         |   Optional   | Exclude script for a list of *build* and *target* types.

**Example:**

```yml
linker:                      # Control linker operation
  - script: .\MyProject.sct  # Explicit scatter file
    for-type: .Debug  
```

### `for-compiler:`

Depending on a compiler toolchain it is possible to include *compiler-specific nodes* in the build process.

**Examples:**

```yml
for-compiler: AC6@6.16               # add item for Arm Compiler version 6.16 only      

for-compiler:                        # add item
  - AC6                              # for Arm Compiler (any version)
  - GCC                              # for GCC Compiler
```

The keyword `for-compiler:` can be applied to the following nodes:

List Node                                  | Description
:------------------------------------------|:------------------------------------
[`- group:`](#groups)                      | At `group:` node it is possible to control inclusion of a file group.
[`- file:`](#files)                        | At `file:` node it is possible to control inclusion of a file.
[`- setup:`](#setups)                      | At `setup:` node to define compiler-specific toolchain settings.
[`- misc:`](#misc)                         | At `misc:` node that allows to add miscellaneous compiler-specific controls.
[`- script:`](#linker)                     | At `linker:` node to control the usage of linker script files.

The inclusion of a *compiler-specific node* is processed when the specified compiler(s) matches.

## Translation Control

The following translation control options may be used at various places such as:

- [`solution:`](#solution) level to specify options for a collection of related projects
- [`project:`](#projects) level to specify options for a project
- [`groups:`](#groups) level to specify options for a specify source file group
- [`files:`](#files) level to specify options for a specify source file

> **Note:** `define:`, `add-path:`, `del-path:`  and `misc:` are additive. All other keywords overwrite previous
  settings.

### `language-C:`

Set the language standard for C source file compilation.

Value                                                 | Select C Language Standard
:-----------------------------------------------------|:------------------------------------
`c90`                                                 | compile C source files as defined in C90 standard (ISO/IEC 9899:1990).
`gnu90`                                               | same as `c90` but with additional GNU extensions.
`c99` (default)                                       | compile C source files as defined in C99 standard (ISO/IEC 9899:1999).
`gnu99`                                               | same as `c99` but with additional GNU extensions.
`c11`                                                 | compile C source files as defined in C11 standard (ISO/IEC 9899:2011).
`gnu11`                                               | same as `c11` but with additional GNU extensions.

### `language-CPP:`

Set the language standard for C++ source file compilation.

Value                                                 | Select C++ Language Standard
:-----------------------------------------------------|:------------------------------------
`c++98`                                               | compile C++ source files as defined in C++98 standard (ISO/IEC 14882:1998).
`gnu++98`                                             | same as `c++98` but with additional GNU extensions.
`c++03`                                               | compile C++ source files as defined in C++03 standard (ISO/IEC 14882:2003).
`gnu++03`                                             | same as `c++03` but with additional GNU extensions.
`c++11`                                               | compile C++ source files as defined in C++11 standard (ISO/IEC 14882:2011).
`gnu++11`                                             | same as `c++11` but with additional GNU extensions.
`c++14` (default)                                     | compile C++ source files as defined in C++14 standard (ISO/IEC 14882:2014).
`gnu++14`                                             | same as `c++14` but with additional GNU extensions.
`c++17`                                               | compile C++ source files as defined in C++17 standard (ISO/IEC 14882:2014).
`gnu++17`                                             | same as `c++17` but with additional GNU extensions.
`c++20`                                               | compile C++ source files as defined in C++20 standard (ISO/IEC 14882:2020).
`gnu++20`                                             | same as `c++20` but with additional GNU extensions.

### `optimize:`

Generic optimize levels for code generation.

Value                                                 | Code Generation
:-----------------------------------------------------|:------------------------------------
`balanced`                                            | Balanced optimization (default)
`size`                                                | Optimized for code size
`speed`                                               | Optimized for execution speed
`none`                                                | No optimization (provides better debug illusion)

**Example:**

```yml
groups:
  - group:  "Main File Group"
    optimize: none          # optimize this file group for debug illusion
    files:
      - file: file1a.c
      - file: file1b.c
```

### `debug:`

Control the generation of debug information.

Value                                                 | Code Generation
:-----------------------------------------------------|:------------------------------------
`on`                                                  | Generate debug information (default)
`off`                                                 | Generate no debug information

**Example:**

```yml
 build-types:
    - type: Release
      optimize: size        # optimize for size
      debug: off            # generate no debug information for the release build
```

### `warnings:`

Control warning level for compiler diagnostics.

Value                                                 | Control diagnostic messages (warnings)
:-----------------------------------------------------|:------------------------------------
`on`                                                  | Generate warning messages
`off`                                                 | No warning messages generated

### `define:`

>**Note:** For a transition period `defines:` is also accepted.  However, this is deprecated and will be removed in a
 future update.

Contains a list of symbol #define statements that are passed via the command line to the development tools.

`define:`                                             | Content
:-----------------------------------------------------|:------------------------------------
&nbsp;&nbsp; `- <symbol-name>`                        | #define symbol passed via command line
&nbsp;&nbsp; `- <symbol-name> = <value>`              | #define symbol with value passed via command line

**Example:**

```yml
define:                    # Start a list of define statements
  - TestValue = 12         # add symbol 'TestValue' with value 12
  - TestMode               # add symbol 'TestMode'
```

### `undefine:`

>**Note:** For a transition period `undefines:` is also accepted.  However, this is deprecated and will be removed in a
 future update.

Remove symbol #define statements from the command line of the development tools.

`undefine:`                                          | Content
:----------------------------------------------------|:------------------------------------
&nbsp;&nbsp; `- <symbol-name>`                       | Remove #define symbol

**Examples:**

```yml
groups:
  - group:  "Main File Group"
    undefine:
      - TestValue           # remove define symbol `TestValue` for this file group
    files: 
      - file: file1a.c
        undefine:
         - TestMode         # remove define symbol `TestMode` for this file
      - file: file1b.c
```

### `add-path:`

>**Note:** For a transition period `add-paths:` is also accepted.  However, this is deprecated and will be removed in a
 future update.

Add include paths to the command line of the development tools.

`add-paths:`                                         | Content
:----------------------------------------------------|:------------------------------------
&nbsp;&nbsp; `- <path-name>`                         | Named path to be added

**Example:**

```yml
project:
  misc:
    - for-compiler: AC6
      C: [-fshort-enums, -fshort-wchar]
    - for-compiler: GCC
      C: [-fshort-enums, -fshort-wchar]

  add-path:
    - $OutDir(Secure)$                   # add path to secure project's output directory
```

### `del-path:`

>**Note:** For a transition period `del-paths:` is also accepted.  However, this is deprecated and will be removed in a
 future update.

Remove include paths (that are defined at the cproject level) from the command line of the development tools.

`del-paths:`                                         | Content
:----------------------------------------------------|:------------------------------------
&nbsp;&nbsp; `- <path-name>`                         | Named path to be removed; `*` for all

**Examle:**

```yml
  target-types:
    - type: CM3
      device: ARMCM3
      del-paths:
        - /path/solution/to-be-removed
```

### `misc:`

Add miscellaneous literal tool-specific controls that are directly passed to the individual tools depending on the file
type.

`misc:`                              |              | Content
:------------------------------------|--------------|:------------------------------------
[`- for-compiler:`](#for-compiler)   |   Optional   | Name of the toolchain that the literal control string applies to.
&nbsp;&nbsp; `C-CPP:`                |   Optional   | Applies to `*.c` and `*.cpp` files (added before `C` and `CPP:`).
&nbsp;&nbsp; `C:`                    |   Optional   | Applies to `*.c` files only.
&nbsp;&nbsp; `CPP:`                  |   Optional   | Applies to `*.cpp` files only.
&nbsp;&nbsp; `ASM:`                  |   Optional   | Applies to assembler source files only.
&nbsp;&nbsp; `Link:`                 |   Optional   | Applies to the linker (added before `Link-C:` or `Link-CPP:`).
&nbsp;&nbsp; `Link-C:`               |   Optional   | Applies to the linker; added when no C++ files are part of the project.
&nbsp;&nbsp; `Link-CPP:`             |   Optional   | Applies to the linker; added when C++ files are part of the project.
&nbsp;&nbsp; `Lib:`                  |   Optional   | Applies to the library manager or archiver.

**Example:**

```yml
  build-types:
    - type: Debug
      misc:
        - for-compiler: AC6
          C-CPP:
            - -O1
            - -g
        - for-compiler: GCC
          C-CPP:
            - -Og

    - type: Release
      compiler: AC6
      misc:
        C:
          - -O3
```

## Project Setups

The `setups:` node can be used to create setups that are specific to a compiler, target-type, and/or built-type.

### `setups:`

The `setups:` node collects a list of `setup:` notes.  For each context, only one setup will be selected.

The result is a `setup:` that collects various toolchain options and that is valid for all files and components in the
project. It is however possible to change that `setup:` settings on a [`group:`](#groups) or [`file:`](#files) level.

`setups:`                                            |              | Content
:----------------------------------------------------|:-------------|:------------------------------------
`- setup:`                                           | **Required** | Description of the setup
&nbsp;&nbsp;&nbsp; [`for-type:`](#for-type)          |   Optional   | Include group for a list of *build* and *target* types.
&nbsp;&nbsp;&nbsp; [`not-for-type:`](#not-for-type)  |   Optional   | Exclude group for a list of *build* and *target* types.
&nbsp;&nbsp;&nbsp; [`for-compiler:`](#for-compiler)  |   Optional   | Include group for a list of compilers.
&nbsp;&nbsp;&nbsp; [`optimize:`](#optimize)          |   Optional   | Optimize level for code generation.
&nbsp;&nbsp;&nbsp; [`debug:`](#debug)                |   Optional   | Generation of debug information.
&nbsp;&nbsp;&nbsp; [`warnings:`](#warnings)          |   Optional   | Control generation of compiler diagnostics.
&nbsp;&nbsp;&nbsp; [`define:`](#define)              |   Optional   | Define symbol settings for code generation.
&nbsp;&nbsp;&nbsp; [`undefine:`](#undefine)          |   Optional   | Remove define symbol settings for code generation.
&nbsp;&nbsp;&nbsp; [`add-path:`](#add-path)          |   Optional   | Additional include file paths.
&nbsp;&nbsp;&nbsp; [`del-path:`](#del-path)          |   Optional   | Remove specific include file paths.
&nbsp;&nbsp;&nbsp; [`linker:`](#linker)              |   Optional   | Remove specific include file paths.
&nbsp;&nbsp;&nbsp; [`misc:`](#misc)                  |   Optional   | Literal tool-specific controls.
&nbsp;&nbsp;&nbsp; [`processor:`](#processor)        |   Optional   | Processor configuration.

```yml
project:
  setups:
    - setup: Arm Compiler 6 project setup
      for-compiler: AC6
      linker:
        - script: my-project.sct
      define:
        - test: 12

   - setup: GCC project setup
     for-compiler: GCC
     linker:
       - script: my-project.inc
     define:
       - test: 11
```

## Pack Selection

The `packs:` node can be specified in the `*.csolution.yml` and `*.cdefault.yml` file allows you to:
  
- Reduce the scope of software packs that are available for projects.
- Add specific software packs optional with a version specification.
- Provide a path to a local installation of a software pack that is for example project specific or under development.

The  [Pack Name Conventions](#pack-name-conventions) are used to specify the name of the software packs.
The `pack:` definition can be specific to [target and build types](#target-and-build-types) and may provide a local path
to a development repository of a software pack.

>**NOTE:** By default, the `csolution` project manager only loads the latest version of the installed software packs. It is however possible to request specific versions using the `- pack:` node.

### `packs:`

The `packs:` node is the start of a pack selection.

`packs:`                                              | Content
:-----------------------------------------------------|:------------------------------------
&nbsp;&nbsp; [`- pack:`](#pack)                       | Explicit pack specification (additive)

### `pack:`

The `pack:` list allows to add specific software packs, optional with a version specification. The version number can
have also the format `@~1.2`/`@~1` that matches with semantic versioning.

`pack:`                                               | Content
:-----------------------------------------------------|:------------------------------------
&nbsp;&nbsp; `path:`                                  | Explicit path name that stores the software pack
&nbsp;&nbsp; [`for-type:`](#for-type)                 | Include pack for a list of *build* and *target* types.
&nbsp;&nbsp; [`not-for-type:`](#not-for-type)         | Exclude pack for a list of *build* and *target* types.

**Example:**

```yml
packs:                                  # start section that specifics software packs
  - pack: AWS                           # use packs from AWS
  - pack: NXP::*K32L*                   # use packs from NXP relating to K32L series (would match K32L3A60_DFP + FRDM-K32L3A6_BSP)
  - pack: ARM                           # use packs from Arm

  - pack: Keil::Arm_Compiler            # add always Keil::Arm_Compiler pack
  - pack: Keil::MDK-Middleware@7.13.0   # add Keil::MDK-Middleware pack at version 7.13.0
  - pack: ARM::CMSIS-FreeRTOS@~10.4     # add CMSIS-FreeRTOS with version 10.4.x

  - pack: NXP::K32L3A60_DFP             # add pack for NXP device 
    path: ./local/NXP/K32L3A60_DFP      # with path to the pack (local copy, repo, etc.)

  - pack: AWS::coreHTTP                 # add pack
    path: ./development/AWS/coreHTTP    # with path to development source directory
    for-type: +DevTest                  # pack is only used for target-type "DevTest"
```

## Target Selection

### `board:`

Specifies a [unique board name](#board-name-conventions), optionally with vendor that must be defined in software packs.
This information is used to define the `device:` along with basic toolchain settings.

### `device:`

Specifies a [unique device name](#device-name-conventions), optionally with vendor that must be defined in software
packs. This information is used to define the `device:` along with basic toolchain settings.

A `device:` is derived from the `board:` setting, but an explicit `device:` setting overrules the `board:` device.

If `device:` specifies a device with a multi-core processor, and no explicit `pname` for the processor core selection is specified, the default `pname` of the device is used.

## Processor Attributes

### `processor:`

The `processor:` keyword defines attributes such as TrustZone and FPU register usage or a processor core selection for multi-core devices.

`processor:`                   | Content
:------------------------------|:------------------------------------
&nbsp;&nbsp; `name:`           | Specifies the `pname` to select a processor core. Note: overwrites a `pname` selection at [`device:`](#device) level.
&nbsp;&nbsp; `trustzone:`      | TrustZone mode: secure \| non-secure \| off.
&nbsp;&nbsp; `fpu:`            | Control usage of FPU registers (S-Register for Helium/FPU) (default: on for devices with FPU registers).
&nbsp;&nbsp; `endian:`         | Select endianness: little \| big (only available when devices are configurable).

```yml
project:
  processor:
    trustzone: secure
```

> **Note:**
>
> - Default for `trustzone:`
>   - `off` for devices that support this option.
>   - `non-secure` for devices that have TrustZone enabled.

## Target and Build Types

The section
[Project setup for multiple targets and test builds](Overview.md#project-setup-for-multiple-targets-and-builds)
describes the concept of  `target-types` and `build-types`. These *types* can be defined in the following files in the
following order:

- `default.csettings.yml`  where it defines global *types*, such as *Debug* and *Release* build.
- `*.csolution.yml` where it specifies the build and target *types* of the complete system.

The *`target-type`* and *`build-type`* definitions are additive, but an attempt to redefine an already existing type
results in an error.

The settings of the *`target-type`* are processed first; then the settings of the *`build-type`* that potentially
overwrite the *`target-type`* settings.

### `target-types:`

The `target-types:` node may include [toolchain options](#toolchain-options), [target selection](#target-selection), and
[processor attributes](#processor-attributes):

`target-types:`                                    |              | Content
:--------------------------------------------------|--------------|:------------------------------------
`- type:`                                          | **Required** | Name of the target-type.
&nbsp;&nbsp;&nbsp; [`compiler:`](#compiler)        |   Optional   | Toolchain selection.
&nbsp;&nbsp;&nbsp; [`output-type:`](#output-type)  |   Optional   | Generate executable (default) or library.
&nbsp;&nbsp;&nbsp; [`optimize:`](#optimize)        |   Optional   | Optimize level for code generation.
&nbsp;&nbsp;&nbsp; [`debug:`](#debug)              |   Optional   | Generation of debug information.
&nbsp;&nbsp;&nbsp; [`warnings:`](#warnings)        |   Optional   | Control Generation of debug information.
&nbsp;&nbsp;&nbsp; [`define:`](#define)            |   Optional   | Preprocessor (#define) symbols for code generation.
&nbsp;&nbsp;&nbsp; [`undefine:`](#undefine)        |   Optional   | Remove preprocessor (#define) symbols.
&nbsp;&nbsp;&nbsp; [`add-path:`](#add-path)        |   Optional   | Additional include file paths.
&nbsp;&nbsp;&nbsp; [`del-path:`](#del-path)        |   Optional   | Remove specific include file paths.
&nbsp;&nbsp;&nbsp; [`misc:`](#misc)                |   Optional   | Literal tool-specific controls.
&nbsp;&nbsp;&nbsp; [`board:`](#board)              |   Optional   | Board specification.
&nbsp;&nbsp;&nbsp; [`device:`](#device)            |   Optional   | Device specification.
&nbsp;&nbsp;&nbsp; [`processor:`](#processor)      |   Optional   | Processor specific settings.

### `build-types:`

The `build-types:` node may include [toolchain options](#toolchain-options):

`build-types:`                                        |              | Content
:-----------------------------------------------------|--------------|:------------------------------------
`- type:`                                             | **Required** | Name of the target-type.
&nbsp;&nbsp;&nbsp; [`compiler:`](#compiler)           |   Optional   | Toolchain selection.
&nbsp;&nbsp;&nbsp; [`output-type:`](#output-type)     |   Optional   | Generate executable (default) or library.
&nbsp;&nbsp;&nbsp; [`optimize:`](#optimize)           |   Optional   | Optimize level for code generation.
&nbsp;&nbsp;&nbsp; [`debug:`](#debug)                 |   Optional   | Generation of debug information.
&nbsp;&nbsp;&nbsp; [`define:`](#define)               |   Optional   | Preprocessor (#define) symbols for code generation.
&nbsp;&nbsp;&nbsp; [`undefine:`](#undefine)           |   Optional   | Remove preprocessor (#define) symbols.
&nbsp;&nbsp;&nbsp; [`add-path:`](#add-path)           |   Optional   | Additional include file paths.
&nbsp;&nbsp;&nbsp; [`del-path:`](#del-path)           |   Optional   | Remove specific include file paths.
&nbsp;&nbsp;&nbsp; [`misc:`](#misc)                   |   Optional   | Literal tool-specific controls.

**Example:**

```yml
target-types:
  - type: Board
    board: NUCLEO-L552ZE-Q       # board specifies indirectly also the device

  - type: Production-HW
    device: STM32L552RC          # specifies device
      
build-types:
  - type: Debug
    optimize: none               # specifies code optimization level
    debug: on                    # generates debug information

  - type: Test
    optimize: size
    debug: on
```

The `board:`, `device:`, and `processor:` settings are used to configure the code translation for the toolchain. These
settings are processed in the following order:

1. `board:` relates to a BSP software pack that defines board parameters, including the
   [mounted device](https://arm-software.github.io/CMSIS_5/Pack/html/pdsc_boards_pg.html#element_board_mountedDevice).
   If `board:` is not specified, a `device:` most be specified.
2. `device:` defines the target device. If `board:` is specified, the `device:` setting can be used to overwrite the
   device or specify the processor core used.
3. `processor:` overwrites default settings for code generation, such as endianess, TrustZone mode, or disable Floating
   Point code generation.

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

Depending on a *`target-type`* or *`build-type`* selection it is possible to include or exclude *items* in the build
process.

### `for-type:`

A [*type list*](#type-list) that adds a [*list-node*](#list-nodes) for a specific target or build *type* or a list of
*types*.

### `not-for-type:`

A [*type list*](#type-list) that removes an [*list-node*](#list-nodes) for a specific target or build *type* or a list
of *types*.

#### type list

It is possible to specify only a `<build-type>`, only a `<target-type>` or a combination of `<build-type>` and
`<target-type>`.

It is also possible to provide a list of *build* and *target* types. The *type list syntax* for `for-type:` or
`not-for-type:` is:

```yml
  - [.<build-type>][+<target-type>]
  - [.<build-type>][+<target-type>]
```

**Examples:**

```yml
for-type:      
  - .Test                            # add item for build-type: Test (any target-type)

for-type:                            # add item
  - .Debug                           # for build-type: Debug and 
  - .Release+Production-HW           # build-type: Release / target-type: Production-HW

not-for-type:  +Virtual              # remove item for target-type: Virtual (any build-type)
not-for-type:  .Release+Virtual      # remove item for build-type: Release with target-type: Virtual
```

> **Note:** `for-type` and `not-for-type` are mutually exclusive, only one occurrence can be specified for a
  *list node*.

#### list nodes

The keyword `for-type:` or `not-for-type:` can be applied to the following list nodes:

List Node                                  | Description
:------------------------------------------|:------------------------------------
[`- project:`](#projects)                  | At `projects:` level it is possible to control inclusion of project.
[`- layer:`](#layers)                      | At `layers:` level it is possible to control inclusion of a software layer.
[`- component:`](#components)              | At `components:` level it is possible to control inclusion of a software component.
[`- group:`](#groups)                      | At `groups:` level it is possible to control inclusion of a file group.
[`- setup:`](#setups)                      | At `setups:` level it is define toolchain specific options that apply to the whole project.
[`- file:`](#files)                        | At `files:` level it is possible to control inclusion of a file.

The inclusion of a *list node* is processed for a given *project context* respecting its hierarchy from top to bottom:
`project` > `layer` > `component`/`group` > `file`

In other words, the restrictions specified by a *type list* for a *list node* are automatically applied to its children
nodes. Children *list nodes* inherit the restrictions from their parent and consequently it must be considered when
processing their *type lists*.

## Related Projects

The section [Project setup for related projects](Overview.md#project-setup-for-related-projects) describes the
collection of related projects. The file `*.csolution.yml` describes the relationship of this projects. This file may
also define [Target and Build Types](#target-and-build-types) before the section `solution:`.

### `projects:`

The YML structure of the section `projects:` is:

`projects:`                                    |              | Content
:----------------------------------------------|--------------|:------------------------------------
[`- project:`](#project)                       | **Required** | Path to the project file.
&nbsp;&nbsp; [`for-type:`](#for-type)          |   Optional   | Include project for a list of *build* and *target* types.
&nbsp;&nbsp; [`not-for-type:`](#not-for-type)  |   Optional   | Exclude project for a list of *build* and *target* types.
&nbsp;&nbsp; [`compiler:`](#compiler)          |   Optional   | Specify a specific compiler.
&nbsp;&nbsp; [`optimize:`](#optimize)          |   Optional   | Optimize level for code generation.
&nbsp;&nbsp; [`debug:`](#debug)                |   Optional   | Generation of debug information.
&nbsp;&nbsp; [`warnings:`](#warnings)          |   Optional   | Control generation of compiler diagnostics.
&nbsp;&nbsp; [`define:`](#define)              |   Optional   | Define symbol settings for code generation.
&nbsp;&nbsp; [`undefine:`](#undefine)          |   Optional   | Remove define symbol settings for code generation.
&nbsp;&nbsp; [`add-path:`](#add-path)          |   Optional   | Additional include file paths.
&nbsp;&nbsp; [`del-path:`](#del-path)          |   Optional   | Remove specific include file paths.
&nbsp;&nbsp; [`misc:`](#misc)                  |   Optional   | Literal tool-specific controls.

**Example:**

```yml
  projects:
    - project: ./CM0/CM0.cproject.yml      # specify cproject.yml file
      for-type: +CM0                       # specify use case
      for-compiler: GCC
      define:
        - test: 12

    - project: ./CM0/CM0.cproject.yml      # specify cproject.yml file
      for-type: +CM0                       # specify use case
      for-compiler: AC6
      define:
        - test: 9

    - project: ./Debug/Debug.cproject.yml  # specify cproject.yml file
      not-for-type: .Release               # specify not to use case
```

## Source File Management

Keyword          | Used in files                    | Description
:----------------|:---------------------------------|:------------------------------------
`groups:`        | `*.cproject.yml`, `*.clayer.yml` | Start of a list that adds [source groups and files](#source-file-management) to a project or layer.
`layers:`        | `*.cproject.yml`                 | Start of a list that adds software layers to a project.
`components:`    | `*.cproject.yml`, `*.clayer.yml` | Start of a list that adds software components to a project or layer.

### `groups:`

The `groups:` keyword specifies a list that adds [source groups and files](#source-file-management) to a project or layer:

`groups:`                                      |              | Content
:----------------------------------------------|--------------|:------------------------------------
`- group:`                                     | **Required** | Name of the group.
&nbsp;&nbsp; [`for-type:`](#for-type)          |   Optional   | Include group for a list of *build* and *target* types.
&nbsp;&nbsp; [`not-for-type:`](#not-for-type)  |   Optional   | Exclude group for a list of *build* and *target* types.
&nbsp;&nbsp; [`for-compiler:`](#for-compiler)  |   Optional   | Include group for a list of compilers.
&nbsp;&nbsp; [`optimize:`](#optimize)          |   Optional   | Optimize level for code generation.
&nbsp;&nbsp; [`debug:`](#debug)                |   Optional   | Generation of debug information.
&nbsp;&nbsp; [`warnings:`](#warnings)          |   Optional   | Control generation of compiler diagnostics.
&nbsp;&nbsp; [`define:`](#define)              |   Optional   | Define symbol settings for code generation.
&nbsp;&nbsp; [`undefine:`](#undefine)          |   Optional   | Remove define symbol settings for code generation.
&nbsp;&nbsp; [`add-path:`](#add-path)          |   Optional   | Additional include file paths.
&nbsp;&nbsp; [`del-path:`](#del-path)          |   Optional   | Remove specific include file paths.
&nbsp;&nbsp; [`misc:`](#misc)                  |   Optional   | Literal tool-specific controls.
&nbsp;&nbsp; [`groups:`](#groups)              |   Optional   | Start a nested list of groups.
&nbsp;&nbsp; [`files:`](#files)                |   Optional   | Start a list of files.

**Example:**

See [`files:`](#files) section.

### `files:`

`files:`                                             |              | Content
:----------------------------------------------------|--------------|:------------------------------------
`- file:                                             | **Required** | Name of the file.
&nbsp;&nbsp;&nbsp; [`for-type:`](#for-type)          |   Optional   | Include file for a list of *build* and *target* types.
&nbsp;&nbsp;&nbsp; [`not-for-type:`](#not-for-type)  |   Optional   | Exclude file for a list of *build* and *target* types.
&nbsp;&nbsp;&nbsp; [`for-compiler:`](#for-compiler)  |   Optional   | Include file for a list of compilers.
&nbsp;&nbsp;&nbsp; [`optimize:`](#optimize)          |   Optional   | Optimize level for code generation.
&nbsp;&nbsp;&nbsp; [`debug:`](#debug)                |   Optional   | Generation of debug information.
&nbsp;&nbsp;&nbsp; [`warnings:`](#warnings)          |   Optional   | Control generation of compiler diagnostics.
&nbsp;&nbsp;&nbsp; [`define:`](#define)              |   Optional   | Define symbol settings for code generation.
&nbsp;&nbsp;&nbsp; [`undefine:`](#undefine)          |   Optional   | Remove define symbol settings for code generation.
&nbsp;&nbsp;&nbsp; [`add-path:`](#add-path)          |   Optional   | Additional include file paths.
&nbsp;&nbsp;&nbsp; [`del-path:`](#del-path)          |   Optional   | Remove specific include file paths.
&nbsp;&nbsp;&nbsp; [`misc:`](#misc)                  |   Optional   | Literal tool-specific controls.

**Example:**

Add source files to a project or a software layer. Used in `*.cproject.yml` and `*.clayer.yml` files.

```yml
groups:
  - group:  "Main File Group"
    not-for-type:                        # includes this group not for the following: 
      - .Release+Virtual                 # build-type "Release" and target-type "Virtual"
      - .Test-DSP+Virtual                # build-type "Test-DSP" and target-type "Virtual"
      - +Board                           # target-type "Board"
    files: 
      - file: file1a.c
      - file: file1b.c
        define:
          - a: 1
        undefine:
          - b
        optimize: size

  - group: "Other File Group"
    files:
      - file: file2a.c
        for-type: +Virtual               # includes this file only for target-type "Virtual"
        define: 
          - test: 2
      - file: file2a.c
        not-for-type: +Virtual           # includes this file not for target-type "Virtual"
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

It is also possible to include a file group for a specific compiler using [`for-compiler:`](#for-compiler) or a specific
target-type and/or build-type using [`for-type:`](#for-type) or [`not-for-type:`](#not-for-type).

```yml
groups:
  - group:  "Main File Group"
    for-compiler: AC6                    # includes this group only for Arm Compiler 6
    files: 
      - file: file1a.c
      - file: file2a.c

  - group:  "Main File Group"
    for-compiler: GCC                    # includes this group only for GCC Compiler
    files: 
      - file: file1b.c
      - file: file2b.c
```

### `layers:`

Add a software layer to a project. Used in `*.cproject.yml` files.

`layers:`                                      |              | Content
:----------------------------------------------|--------------|:------------------------------------
[`- layer:`](#layer)                           | **Required** | Path to the `*.clayer.yml` file that defines the layer.
&nbsp;&nbsp; `type:`                           |   Optional   | Refers to an expected layer type.
&nbsp;&nbsp; [`for-type:`](#for-type)          |   Optional   | Include layer for a list of *build* and *target* types.
&nbsp;&nbsp; [`not-for-type:`](#not-for-type)  |   Optional   | Exclude layer for a list of *build* and *target* types.
&nbsp;&nbsp; [`optimize:`](#optimize)          |   Optional   | Optimize level for code generation.
&nbsp;&nbsp; [`debug:`](#debug)                |   Optional   | Generation of debug information.
&nbsp;&nbsp; [`warnings:`](#warnings)          |   Optional   | Control generation of compiler diagnostics.
&nbsp;&nbsp; [`define:`](#define)              |   Optional   | Define symbol settings for code generation.
&nbsp;&nbsp; [`undefine:`](#undefine)          |   Optional   | Remove define symbol settings for code generation.
&nbsp;&nbsp; [`add-path:`](#add-path)          |   Optional   | Additional include file paths.
&nbsp;&nbsp; [`del-path:`](#del-path)          |   Optional   | Remove specific include file paths.
&nbsp;&nbsp; [`misc:`](#misc)                  |   Optional   | Literal tool-specific controls.

**Example:**

```yml
  layers:
    # Socket
    - layer: ./Socket/FreeRTOS+TCP/Socket.clayer.yml
      for-type:
        - +IP-Stack
    - layer: ./Socket/WiFi/Socket.clayer.yml
      for-type:
        - +WiFi
    - layer: ./Socket/VSocket/Socket.clayer.yml
      for-type:
        - +AVH

    # Board
    - layer: ./Board/IMXRT1050-EVKB/Board.clayer.yml
      for-type: 
        - +IP-Stack
        # - +WiFi
    - layer: ./Board/B-U585I-IOT02A/Board.clayer.yml
      for-type: 
        - +WiFi
    - layer: ./Board/AVH_MPS3_Corstone-300/Board.clayer.yml
      for-type: 
        - +AVH
```

#### Proposals for `layers:`

1. `layer type:` for evaluation of compatible layers.

   The path to the `*.clayer.yml` file can be empty, but in this case a `type:` should be specified.
   The `csolution` project manager searches in the available software packs for compatible layers and notifies the user.
   The compatible layers are listed by evaluating the compatible [`interface:`](#interface).

   **Example:**

   ```yml
     layers:
       - layer:                    # no `*.clayer.yml` specified. Compatible layers are listed
         type: Socket              # layer of type `Socket` is expected

       - layer:                    # no `*.clayer.yml` specified. Compatible layers are listed
         type: Board               # layer of type `Board` is expected
   ```

2. `variables:` to values via access sequences

   With variables that are defined in the `*.csolution.yml` file, it could be avoided that a `*.cproject.yml` file requires modifications
   when new target types are introduced.  The required `layers:` could be instead specified in the `*.csolution.yml` file using a new node `variables:`.

   **Example:**
   
   *Example.csolution.yml*
   
   ```yml
   solution:
     target-types:
       - type: NXP Board
         board: IMXRT1050-EVKB
         variables:
           - Socket-Layer: ./Socket/FreeRTOS+TCP/Socket.clayer.yml
           - Board-Layer:  ./Board/IMXRT1050-EVKB/Board.clayer.yml
  
       - type: ST Board
         board: B-U585I-IOT02A
         variables:
           - Socket-Layer: ./Socket/WiFi/Socket.clayer.yml
           - Board-Layer:  ./Board/B-U585I-IOT02A/Board.clayer.yml
   ```
   
   *Example.cproject.yml*
   
   ```yml
     layers:
       - layer: $Socket-Layer$
         type: Socket
   
       - layer: $Board-Layer$      # no `*.clayer.yml` specified. Compatible layers are listed
         type: Board               # layer of type `Board` is expected
   ```

### `components:`

Add software components to a project or a software layer. Used in `*.cproject.yml` and `*.clayer.yml` files.

`components:`                                  |              | Content
:----------------------------------------------|--------------|:------------------------------------
`- component:`                                 | **Required** | Name of the software component.
&nbsp;&nbsp; [`for-type:`](#for-type)          |   Optional   | Include component for a list of *build* and *target* types.
&nbsp;&nbsp; [`not-for-type:`](#not-for-type)  |   Optional   | Exclude component for a list of *build* and *target* types.
&nbsp;&nbsp; [`optimize:`](#optimize)          |   Optional   | Optimize level for code generation.
&nbsp;&nbsp; [`debug:`](#debug)                |   Optional   | Generation of debug information.
&nbsp;&nbsp; [`warnings:`](#warnings)          |   Optional   | Control generation of compiler diagnostics.
&nbsp;&nbsp; [`define:`](#define)              |   Optional   | Define symbol settings for code generation.
&nbsp;&nbsp; [`undefine:`](#undefine)          |   Optional   | Remove define symbol settings for code generation.
&nbsp;&nbsp; [`add-path:`](#add-path)          |   Optional   | Additional include file paths.
&nbsp;&nbsp; [`del-path:`](#del-path)          |   Optional   | Remove specific include file paths.
&nbsp;&nbsp; [`misc:`](#misc)                  |   Optional   | Literal tool-specific controls.
&nbsp;&nbsp; [`instances:`](#instances)        |   Optional   | Add multiple instances of component configuration files (default: 1)

**Example:**

```yml
components:
    - component: ARM::CMSIS:RTOS2:FreeRTOS&Cortex-M

    - component: ARM::RTOS&FreeRTOS:Config&CMSIS RTOS2
    - component: ARM::RTOS&FreeRTOS:Core&Cortex-M
    - component: ARM::RTOS&FreeRTOS:Event Groups
    - component: ARM::RTOS&FreeRTOS:Heap&Heap_5
    - component: ARM::RTOS&FreeRTOS:Stream Buffer
    - component: ARM::RTOS&FreeRTOS:Timers

    - component: ARM::Security:mbed TLS
      define:
        - MBEDTLS_CONFIG_FILE="aws_mbedtls_config.h"
    - component: AWS::FreeRTOS:backoffAlgorithm
    - component: AWS::FreeRTOS:coreMQTT
    - component: AWS::FreeRTOS:coreMQTT Agent
    - component: AWS::FreeRTOS:corePKCS11&Custom
      define:
        - MBEDTLS_CONFIG_FILE="aws_mbedtls_config.h"
```

> **Note:** The name format for a software component is described under
  [Name Conventions - Component Name Conventions](#component-name-conventions).

### `instances:`

Allows to add multiple instances of a component and actually applies to configuration files.
Detailed description is [here](https://open-cmsis-pack.github.io/Open-CMSIS-Pack-Spec/main/html/pdsc_components_pg.html#Component_Instances)

**Example:**

```yml
components:
  - component: USB:Device
    instances: 2
```

If the user selects multiple instances of the same component, all files with  attribute `config` in the `*.PDSC` file
will be copied multiple times to the project. The name of the component (for example config_mylib.h) will get a postfix
`_n` whereby `n` is the instance number starting with 0.

Instance 0: config_usb_device_0.h  
Instance 1: config_usb_device_1.h

The availability of instances in a project can be made public in the `RTE_Components.h` file. The existing way to extend
the `%Instance%` with the instance number `n`.

## Pre/Post build steps

Tbd: potentially map to CMake add_custom_command.

### `execute:`

Execute and external command for pre- or post-build steps (such as code signing).

`- execute:`                |               | Content
:---------------------------|:--------------|:------------------------------------
`- execute:` description    |  **Required** | Execute an external command with description
&nbsp;&nbsp; `os:` name     |   Optional    | Executable on named operating systems (if omitted it is OS independent).
&nbsp;&nbsp; `run:` name    |   Optional    | Executable name, optionally with path to the tool.
&nbsp;&nbsp; `args:` name   |   Optional    | Executable arguments.
&nbsp;&nbsp; `stop:` name   |   Optional    | Stop on exit code.

Potential usage before/after build:

```yml
solution:
  :
  :
  projects:
    - execute: Generate Keys for TF-M
      os: Linux
      run: KeyGen.exe
    - project: /security/TFM.cproject.yml
    - project: /application/MQTT_AWS.cproject.yml
    - execute: Copy output files
      run: cp *.out .\output
```

Potential usage during build steps:

```yml
project:
  :
  :
  groups:
    - group:  "Main File Group"
      files: 
        - execute: Generate file1a.c
          run: xyz.exe
          ....
        - file: file1a.c
```

## `interface:`

Is a user-defined list of interfaces and can be applied to `project:` or `layer:`.  

`interface:`                         |              | Description
:------------------------------------|--------------|:------------------------------------
[`provides:`](#provides)             |   Optional   | List of interfaces (key/value pairs) that are provided by a `project:` or `layer:`
[`consumes:`](#consumes)             |   Optional   | List of interfaces (key/value pairs) that are consumed by a `project:` or  `layer:`

**Example:**

```yml
  interface:                         # interface description of a board layer
    consumes:
    - RTOS2:
    provides:
    - CMSIS Driver Ethernet: 0       # driver number
    - CMSIS Driver USART Print: 2    # driver number
    - IoT Socket:                    # available
    - Heap: 65536                    # heap size 
```

### `provides:`

A user-defined list of interfaces that are implemented or provided by the files or software components in a `project:` or `layer:`. 

#### Proposal: Export symbols to RTE_components.h

All provided interfaces in the current solution context are exported with `#define` symbols to the releated `RTE_components.h` header file and prefixed with `itf_`.

*Content of RTE_components.h:*

```c
#define itf_CMSIS_Driver_Ethernet 0
#define itf_CMSIS_Driver_USART_Print 2
#define itf_IoT_Socket
#define Heap 65536
```

### `consumes:`

#### Proposal: for interface matching

A user-defined list of interfaces that are requried or consumed by the files or software components in a `project:` or `layer:`. 
This list is used by the `csolution` tool to identify compatible software layers and evaluate the completeness of an project.

**Example:**

*MyProject.cproject.yml*

```yml
  interfaces:
    provides:
      - RTOS2:          # implements RTOS2 API interface
    consumes:
      - IoT_Socket:     # requires IoT_Socket interface
      - STDOUT:         # requires STDOUT interface
      - Heap:  +30000   # requires additional 30000 bytes memory heap
  :
  layers:
    - layer: MySocket.clayer.yml
    - layer: MyBoard.clayer.yml
```      

*MySocket.clayer.yml*

```yml
interfaces:
    consumes:
      - RTOS2:          # requires RTOS2 API interface
      - VSocket:        # requires VSocket interface
      - Heap: +20000    # requires additional 20000 bytes memory heap
    provides:
      - IoT_Socket:     # provides IoT_Socket interface
```

*MyBoard.clayer.yml*

```yml
  interfaces:
    consumes:
      - RTOS2:
    provides:
      - VSocket:
      - STDOUT:
      - Heap:  65536
```

The `csolution` tool combines all the interfaces that listed under `provides:` and matches it with the interfaces that are listed under `consumes:`.  

For interfaces listed under `provides:` the following rules exist:

- It is possible to omit the *value*.
- A *value* is interpreted as number. When the same interface is listed multiple times under `provides:` the value must be identical.

For interfaces listed under `consumed:` the following rules exist:

- A *value* that is prefixed with '+' is interpreted as number that is added together in case that the same interface is listed multiple times under `consumes:`.
- When no *value* is specified, it matches with any *value* of an interface listed under `provides:`.
- When a *value* (or an added *value*) is specified, the *value* (or the sum of the *values*) must be less or equal then the *value* of an interface that is listed under `provides:`.

## Generator (Proposal)

>Note: Superseeded by [Generator%20(Proposal).md](Generator%20(Proposal).md)

--> Requires Review

### Workflow assumptions

The composition of a solution of a solution should have the following steps:

- Create `*.cproject.yml` files and the `*.csolution.yml` container that refers the projects.
- Select `device:` or `board:` (optionally by using `target-types:`)
- Add `components:` or `layers:` to the `*.cproject.yml` file
- For components that have configuration, run the generator in configuration mode
  - change pinout, clock, resources, etc.
  - reflect configuration in *.gpdsc file (and related settings files)

> **Note:** Components can have multiple [instances](#instances).

### Steps for component selection and configuration

The following explains the generator workflow of CSolution / CBuild for configuration of components:

1. User selects components in `*.cproject.yml` under `components:`
   - When these components require generation, user is notified to run a generator.
   - "CSolution Run GenID” is invoked for a list of components.
   - CSolution generates `*.cgen.json` file that provides project context and a list of user-selected components.
  
2. Running the Generator (for Component Configuration, i.e. pin selection)
   - Generator reads `*.cgen.json` file
   - User performs the configuration is done.
     - Interactive mode (where a settings file is generated)
     - Remote mode (where a settings file is an input) **IS THIS REALLY REQUIRED**
   - Generator creates a `*.gpdsc` file that informs the CSolution tool about
     - (a) the fact that a component is configured and has generated code,
     - (b) additional components that are the result of some user configuration.

   Discussions:
   - is a component list or a dependency list
   - Generator might be VS Code plugin or web based

3. User creates CBuild output with CSolution Convert command
   - Both `*.cproject.yml` and `*.gpdsc` are read by Csolution and create the complete list of selected components.
   - If `*.gpdsc` does not contain component information about a component that has `genId` and selected in
     `*.cproject.yml` the generator configuration is incomplete. This can happen when a component is added at a later
     step.
   - Likewise the Generator can detect with the `<component User="1">` attribute that a component is longer required.
     In this case the user is notified to run a generator.

### Enhance Usability

Add Run Generator buttons to Cclass descriptions.

![Add Run Generator buttons to Cclass](./images/gen.png "Add Run Generator buttons to Cclass")

### Workflow

1. For `*.cproject.yml` files that contain selected `<components>` with a `generator` or `genid` attribute the
   `csolution` manager checks if a file with the name `./<project>/RTE+<target>/<genid>.cgen.json` exists.
   - When this file is missing, it is required to use the command `csolution run genid` to start the generator.
   - When this file exists, the `csolution` manager checks if the list of components with `genid` has changed. If this
     is the case it is required to use the command `csolution run genid` to reconfigure generated components.
2. The command `csolution run genid` creates the file `./<project>/RTE+<target>/<genid>.cgen.json` and starts the
   generator. The generator creates a `*.GDPSC` file along with other source files that are required
   [as specified](https://open-cmsis-pack.github.io/Open-CMSIS-Pack-Spec/main/html/pdsc_generators_pg.html#element_generators).

#### Example Content of `*.cgen.json` (in this case `STM32CubeMX.cgen.json`)

The `*.cgen.json` file is passed to generator as argument.

> **Note:** Shown is still a YML file, but the equivalent data would be formatted in *.JSON format

```yml
cgenerator:
  device: STM32F407IGHx
  board: NucleoF407
  solution: C:/tmp/MySolution/MySolution.csolution.yml 
  project: C:/tmp//MySolution/Blinky/Blinky.cproject.yml 
  context: .Debug+Nucelo    # build-type and target-type of the current generated context
  destination: C:/tmp/MySolution/Blinky/RTE+Nucelo/

  packs:                     # packs that are used for the project
    - pack: Keil::STM32F4xx_DFP@2.16.1
      path: C:/CMSIS-PACKS/Keil/STM32F4xx_DFP/2.16.1/
    - pack: Keil::STM32F4xx_BSP@2.1.1
      path: C:/CMSIS-PACKS/Keil/STM32F4xx_BSP/2.1.1/

  components:               # components that have a genid and are specified in *.cproject.yml
    - component: Device:STM32Cube HAL:Common
    - component: Board:LED
    - component: Device:HAL:UART
      instances:
        - WiFi: 0
          setup: wifi-config.json
          baudrate: 19200 
        - Debug: 2
          baudrate: 57600
```

### Changes to the *.GPDSC file

To indicate that a component was generated due to a user selection in `*.cproject.yml`, the `component` element is
extended with `User` attribute. When set to `1` it indicates that a component is included due to the selection in
`*.cproject.yml`.

When a user removes this component in the `*.cproject.yml`, the CSolution could detect that a Run command should be
executed.

### Changes to the *.PDSC file

- Add `<key>` to `<generator>` element. The `key` is used to invoke the generator and pass the `<genid>.cgen.json` file.
  - on Windows to a registry key to invoke the generator tool
  - on Linux and MacOS to an environment variable that specifies how to invoke the generator tool
  - todo: Web based tools?
- Add `genId` to `component` element. Indicates that a component is managed by the `<generator>`.
- Add `inherent` to `component` element. Indicates that a component is managed by the `<generator>`
  Components with inherent attribute are not selectable by the user (and could be managed by the `<generator>`).
  Components with `inherent` attribute have the following behavior:
  - Are selected when a condition requires this component.
  - Are de-selected when a no condition requires this component.
  - IDE's may choose to hide such components in the RTE selection (default might be to show it).
- Add new file category:
  - `genParms` template parameters
  - `genInput` source templates for the generator and other related input files for the generator

**Example:**

```xml
 <generators>
    <!-- This generator is launched if any component referencing this generator by 'id' is selected and the specified <gpdsc> file does not exist -->
    <generator id="STM32CubeMX">
      <description>ST Microelectronics: STCubeMX Environment</description>
      <key>STCubeMX_CGENFILE</key>          <!-- for windows registry key that opens a *.cgenerator.yml file -->
                                            <!-- for Linux / MacOS the key is an environment variable that can start the tool -->
      <web>url</web>                        <!-- for web based tools the url is used and appended by &Cgen=file -->
      <workingDir>$PRTE/Device</workingDir> <!-- path is specified either absolute or relative to gpdsc file -->
      <gpdsc name ="myGen.gpdsc"/>  <!-- PDSC or GPDSC file. If not specified it is the project directory configured by the environment -->
      <files>   <!-- does this make sense? -->
        <file category="genHelper" name="Drivers/STM32F4xx_HAL_Driver/Src/common-file-for-gen.xyz"/>
      </files>
    </generator>
  </generators>

  <taxonomy>
    <description Cclass="Device" generator="STM32CubeMX" doc="Documentation/DM00105879.pdf" >STM32F4xx Hardware Abstraction Layer (HAL) and Drivers</description>
    <description Cclass="Board" generator="STM32CubeMX" doc="Documentation/DM00105879.pdf" >STM32F412 Board Abstraction Layer (HAL) and Drivers</description>
  </taxonomy>


    <component Cclass="Device" Cgroup="STM32Cube HAL" Csub="Common"    Cversion="1.7.9" condition="STM32F4 HAL Common"  genid="STM32CubeMX" >
      <description>Common HAL driver</description>
      <RTE_Components_h>
        #define RTE_DEVICE_HAL_COMMON
      </RTE_Components_h>
      <files>
        <file category="include" name="Drivers/STM32F4xx_HAL_Driver/Inc/"/>
        <file category="header"  name="Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal.h"/>
        <file category="source"  name="Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c"/>
        <file category="genParams" name="Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.json"/>
        <file category="genHeader" name="Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.h.template"/>
        <file category="genSource" name="Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c.template"/>
      </files>
    </component>
    <component Cclass="Device" Cgroup="STM32Cube HAL" Csub="ADC"       Cversion="1.7.9" condition="STM32F4 HAL DMA"  genid="STM32CubeMX">
      <description>Analog-to-digital converter (ADC) HAL driver</description>
      <RTE_Components_h>
        #define RTE_DEVICE_HAL_ADC
      </RTE_Components_h>
      <files>
        <file category="source" name="Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc.c"/>
        <file category="source" name="Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc_ex.c"/>
        <file category="genParameter" name="Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc.json"/>
        <file category="genHeader" name="Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc.h.template"/>
        <file category="genSource" name="Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc.c.template"/>
      </files>
    </component>
      :
    <bundle Cbundle="STM32F4-Discovery" Cclass="Board Support" Cversion="2.0.0">
      <description>STMicroelectronics STM32F4 Discovery Kit</description>
      <doc>http://www.st.com/st-web-ui/static/active/en/resource/technical/document/data_brief/DM00037955.pdf</doc>
      <component Cgroup="LED" Capiversion="1.0.0" condition="STM32F4 HAL GPIO" genid="STM32CubeMX">
        <description>LED Interface for STMicroelectronics STM32F4-Discovery Kit</description>
        <files>
          <file category="source"  name="MDK/Boards/ST/STM32F4-Discovery/Common/LED_F4Discovery.c"/>
          <file category="genParams" name="MDK/Boards/ST/STM32F4-Discovery/Common/LED_F4Discovery.json"/>
          <file category="genHeader" name="MDK/Boards/ST/STM32F4-Discovery/Common/LED_F4Discovery.h.template"/>
          <file category="genSource" name="MDK/Boards/ST/STM32F4-Discovery/Common/LED_F4Discovery.c.template"/>
        </files>
      </component>
```

**Example *.gpdsc file**

```xml
<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<!-- ******************************************************************************
 * File Name   : FrameworkCubeMX.gpdsc
 * Date        : 23/09/2021 14:18:05
 * Description : Generator PDSC File generated by STM32CubeMX (DO NOT EDIT!)
 ****************************************************************************** -->
<package xmlns:xs="http://www.w3.org/2001/XMLSchema-instance" schemaVersion="1.0" xs:noNamespaceSchemaLocation="PACK.xsd">
  <vendor>Keil</vendor>
  <name>FrameworkCubeMX</name>
  <description>STM32CubeMX generated pack description</description>
  <url>project-path</url>
  <releases>
    <release version="1.0.0">
     - Generated: 23/09/2021 14:18:05
    </release>
  </releases>
  <generators>
    <generator id="STM32CubeMX" Gvendor="STMicroelectronics" Gtool="STM32CubeMX" Gversion="4.10.0">
      <description>STM32CubeMX Environment</description>
      <select Dname="STM32G474RETx" Dvendor="STMicroelectronics:13"/>
      <command>$SMDK\CubeMX\STM32CubeMXLauncher</command>
      <workingDir>$PRTE\Device\STM32G474RETx</workingDir>
      <project_files>
        <file category="include" name="STCubeGenerated/Inc/"/>
        <file category="source" name="STCubeGenerated/Src/main.c" />
        <file category="header" name="STCubeGenerated/Inc/stm32g4xx_it.h"/>
        <file category="source" name="STCubeGenerated/Src/stm32g4xx_it.c"/>
      </project_files>
    </generator>
  </generators>
  <taxonomy>
    <description Cclass="Device" Cgroup="STM32Cube Framework" generator="STM32CubeMX">STM32Cube Framework</description>
  </taxonomy>
  <conditions>
    <condition id="STCubeMX">
      <description>Condition to include CMSIS core, Device Startup and HAL Drivers components</description>
      <require Dvendor="STMicroelectronics:13" Dname="STM32G4*"/>
      <require Cclass="CMSIS"  Cgroup="CORE"/>
      <require Cclass="Device" Cgroup="Startup"/>
      <require Cclass="Device" Cgroup="STM32Cube HAL"/>
    </condition>
  </conditions>
  <components>
    <bundle Cbundle="STM32CubeMX" Cclass="Device" Cversion="1.4.0">
      <component generator="STM32CubeMX" Cvendor="Keil" Cgroup="STM32Cube Framework" Csub="STM32CubeMX" condition="STCubeMX">
        <description>Configuration via STM32CubeMX</description>
        <RTE_Components_h>
          #define RTE_DEVICE_FRAMEWORK_CUBE_MX
        </RTE_Components_h>
        <files>
          <file category="header" name="MX_Device.h"/>
          <file category="header" name="STCubeGenerated/Inc/stm32g4xx_hal_conf.h"/>
          <file category="source" name="STCubeGenerated/Src/stm32g4xx_hal_msp.c"/>
        </files>
      </component>
      <component  Cgroup="Startup" User="1">  <!-- user selected component in *.cproject.yml -->
        <description>System Startup for STMicroelectronics</description>
        <files>
          <file category="source" name="STCubeGenerated/MDK-ARM/startup_stm32g474xx.s" />
          <file category="source" name="STCubeGenerated/Src/system_stm32g4xx.c" />
        </files>
      </component>
    </bundle>
  </components>
</package>
```

## Resource Management (Proposal)

The **CSolution Project Manager** integrates an extended version of the Project Zone functionality of
[CMSIS-Zone](https://arm-software.github.io/CMSIS_5/Zone/html/index.html) with this nodes:

- [`resources:`](#resources) imports resource files (in
  [CMSIS-Zone RZone format](https://arm-software.github.io/CMSIS_5/Zone/html/xml_rzone_pg.html) or a compatible yml
  format tbd) and allows to split or combine memory regions.
- [`phases:`](#phases) defines the execution phases may be used to assign a life-time to memory or peripheral resources
  in the project zones.
- [`project-zones:`](#project-zones) collect and configure the memory or peripheral resources that are available to
  individual projects. These zones are assigned to the [`projects:`](#projects) of a `*.csolution.yml` file.
- [`requires:`](#requires) allows to specify additional resources at the level of a `*.cproject.yml` or `*.clayer.yml`
  file that are added to the related zone of the project.

The **CSolution Project Manager** generates for each project context (with build and/or target-type) a data file
(similar to the current [CMSIS FZone format](https://arm-software.github.io/CMSIS_5/Zone/html/GenDataModel.html), exact
format tbd could be also JSON) for post-processing with a template engine (Handlebars?).

### `resources:`

`resources:`                                   |              | Content
:----------------------------------------------|--------------|:------------------------------------
`- import:`                                    | **Required** | File/resource to be used.
`- split:`                                     |   Optional   | Split a resource/memory region.
`- combine:`                                   |   Optional   | Combine a resource/memory region.

#### `- import:`

`- import:`                                    |              | Content
:----------------------------------------------|--------------|:------------------------------------
`- import:` path_to_resource                   | **Required** | Path to the resource file.

#### `- split:`

Split a resource into subresources.

`- split:`                    |              | Content
:-----------------------------|--------------|:------------------------------------
`into:`                       | **Required** |
&nbsp;&nbsp; `- region:` name | **Required** | Name of the new resource.
&nbsp;&nbsp; `- size:` value  | **Required** | Size of the new resource.

#### `- combine`

Combine/merge resources into a new resource.

`- combine:` name             |              | Content
:-----------------------------|--------------|:------------------------------------
`from:`                       | **Required** |
&nbsp;&nbsp; `- region:` name | **Required** | Name of the resource to be combined.

**Example:**

Added to `*.csolution.yml` file

```yml
resources:
# depending on the device: and/or board: settings these resources may get added automatically
  - import: .\LPC55S69.rzone             # resource definitions of the device
  - import: .\MyEvalBoard.rzone          # add resource definitions of the Eval board

  - split: SRAM_NS                       # split a memory resource into two regions
    into:
    - region: DATA_NS
      size: 128k
    - region: DATA_BOOT
      size: 45k

  - combine: SRAM                        # combine two memory regions (contiguous, same permissions) 
    from:
      - region: SRAM1
      - region: SRAM2
```

> **Note:** Exact behavior for devices that have no RZone file is tbd. It could be that the memory resources are derived
  from device definitions

### `phases:`

**Example:**

Added to `*.csolution.yml` file

```yml
phases:    # define the life-time for resources in the project-zone definition
  - phase: Boot
  - phase: OTA
  - phase: Run
```

### `project-zones:`

**Example:**

Added to `*.csolution.yml` file

```yml
project-zones:   
  - zone: BootLoader
    requires:
    - memory: Code_Boot
      permission: rx, s
    - memory: Ram_Boot
      phase: Boot
      permission: rw, s
    - peripheral: UART1
    - peripheral: Watchdog
      phase: Boot      # only for phase Boot

  - zone: Application
    - memory: *        # all remaining memory
      permission: ns   # with permission ns
      phase: ~Boot     # for every phase except Boot

projects:
  - project: ./bootloader/Bootloader.cproject.yml           # relative path
    zone: Bootloader

  - project: ./application/MyApp1.yml                       # Application 1
    zone: Application

  - project: ./application/MyApp2.yml                       # relative path
    zone: Application
```

#### `requires:`

##### `- memory:`

##### `- peripheral:`

Added to `*.cproject.yml` or `*.clayer.yml` file

```yml
requires:
 - memory: Ram2
   permission: rx, s
 - peripheral: SPI2
   permission: ns, p
```
