
# csolution: Proposals (on hold)

<!-- markdownlint-disable MD013 -->
<!-- markdownlint-disable MD036 -->

This document is a collection of proposals that are currently on hold and will not get implemented for CMSIS-Toolbox 2.0.
The proposals in this document will be discussed and potentially progressed after release of CMSIS-Toolbox 2.0.

## Implemented Features but not exposed to users

For CMSIS-Toolbox 2.0 the goal is to have stable input YML formats. The following features are implemented already, but will be not exposed to users as (a) use cases are unclear, and/or (b) it is unclear if the feature set needs extensions.

The `processor:` keyword defines attributes such as FPU register usage or endianness selection.

`processor:`                   | Content
:------------------------------|:------------------------------------
&nbsp;&nbsp; `fpu:`            | Control usage of FPU registers (S-Register for MVE/Helium, float/double hardware FPU) (default: on for devices with FPU registers).
&nbsp;&nbsp; `endian:`         | Select `endian:` little \| big (only available when devices are configurable).

`output-dirs:`               |              | Content
:----------------------------|--------------|:------------------------------------
&nbsp;&nbsp; `rtedir:`       |  Optional    | Specifies the directory for the RTE files (component configuration files).

The default setting for the `output-dirs:` are:

```yml
rtedir:  <csolution.yml base directory>/%Project$/RTE
```

## Feature Overview (on hold)

- Manage the resources (memory, peripherals, and user defined) across the entire application to:
  - Partition the resources of the system and create related system and linker configuration.
  - Support in the configuration of software stacks (such as RTOS threads).
  - Hint the user for inclusion of software layers that are pre-configured for typical use cases.

## ASM Specific nodes

In CMSIS-Toolbox 2.0 the `define:` and `add-path:` will be applied only to C and C++ source files.
For assembler files, `misc:` can be used to add sepcific controls

In a later version of the CMSIS-Toolbox, ASM specific controls may be added.
`define-asm:` only applies to assembly source files (new)
`add-path-asm:` only applies to assembly source files (new)

## Linker Script

In CMSIS-Toolbox 2.0 the [Linker Script Management](Linker-Script-Management.md) will be implemented.

In a later iteration of the tools it should be possible to generate the `regions_<device_or_board>.h` with a workflow that is similar to "CMSIS-Zone" or "DeviceTree".

## Resource Management (Proposal)  (out-of-scope)

The **csolution - CMSIS Project Manager**  integrates an extended version of the Project Zone functionality of
[CMSIS-Zone](https://arm-software.github.io/CMSIS_5/Zone/html/index.html) with this nodes:

- [`resources:`](#resources) imports resource files (in
  [CMSIS-Zone RZone format](https://arm-software.github.io/CMSIS_5/Zone/html/xml_rzone_pg.html) or a compatible yml
  format tbd) and allows to split or combine memory regions.
- [`phases:`](#phases) defines the execution phases may be used to assign a life-time to memory or peripheral resources
  in the project zones.
- [`project-zones:`](#project-zones) collect and configure the memory or peripheral resources that are available to
  individual projects. These zones are assigned to the [`projects:`](YML-Input-Format.md#projects) of a `*.csolution.yml` file.
- [`requires:`](#requires) allows to specify additional resources at the level of a `*.cproject.yml` or `*.clayer.yml`
  file that are added to the related zone of the project.

The **csolution - CMSIS Project Manager** generates for each project context (with build and/or target-type) a data file
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

> **Note:**
>
> Exact behavior for devices that have no RZone file is tbd. It could be that the memory resources are derived from device definitions

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

## Directory Control

### `rte-dirs:`

**(Proposal)**  Out-of-scope (review if this is really required)

The default RTE directory structure can be modified with [`rte-dirs:`](YML-Input-Format.md#rte-dirs).
This list node allows to specify for each software component `Cclass` the directory that should be used to partly share a common configuration across a `project:` or `layer:`.

The `rte-dirs:` list allows to control the location of configuration files for each [component `Cclass`](YML-Input-Format.md#component-name-conventions).  A list of `Cclass` names can be assigned to specific directories that store the related configuration files.

**Example:**

```yml
  rte-dirs:
    - Board_Support: ..\common\RTE\Board
    - CMSIS Driver:  ..\common\RTE\CMSIS_Driver
    - Device:        ..\common\RTE\Device\Core1
    - Compiler:      ..\RTE\Compiler\Debug
      for-context:      .Debug
```

- With [RTE directory settings](./YML-Input-Format.md#rte-dirs) that are specific to software component `Cclass` names it is possible partly share a common configuration across layers.

## CMSIS-Zone Integration

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
