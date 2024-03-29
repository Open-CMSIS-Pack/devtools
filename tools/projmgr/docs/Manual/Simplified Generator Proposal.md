# Generator Workflow (Proposal) - Revised 7. Sept 2023
<!-- markdownlint-disable MD013 -->

- [Generator Workflow (Proposal) - Revised 7. Sept 2023](#generator-workflow-proposal---revised-7-sept-2023)
  - [User Steps](#user-steps)
  - [Generator Interface](#generator-interface)
  - [Project Types](#project-types)
    - [Project-Type: `single-core`](#project-type-single-core)
    - [Project-Type: `multi-core` or `trustzone`](#project-type-multi-core-or-trustzone)
  - [Generator Start via `generator component`](#generator-start-via-generator-component)
  - [Global Generator Registry](#global-generator-registry)
  - [Additions to `.cbuild-gen-idx.yml` and `.cbuild-idx.yml`](#additions-to-cbuild-gen-idxyml-and-cbuild-idxyml)
  - [Impact to PDSC files and Packs](#impact-to-pdsc-files-and-packs)
  - [`cgen.yml` Format](#cgenyml-format)
  - [Potential Execution Sequence](#potential-execution-sequence)

>**NOTE:**
>
> This document is a refined [Generator Proposal](https://github.com/Open-CMSIS-Pack/devtools/blob/main/tools/projmgr/docs/Manual/Generator%20\(Proposal\).md). It focuses on existing generators such as STM32CubeMX and MCUxpresso.

## User Steps

The user steps for creating a `csolution` based application with a Generator are:

- Create the `*.csolution.yml` container that refers the projects and selects `device:` or `board:`  (by using `target-types:`)
- Create `*.cproject.yml` files that are referred by the `*.csolution.yml` container.
- Add `components:` and/or `layers:` to the `*.cproject.yml` file
- For components that have a `<generator-id>`, run the related generator.

Typically generators are used to:

- Configure device and/or board settings, for example clock configuration or pinout.
- Add and configure software drivers, for example for UART, SPI, or I/O ports.
- Configure parameters of an algorithm, for example DSP filter design or motor control parameters.

## Generator Interface

The diagram below shows how the Generator is integrated into the CMSIS-Build tools. The data flow is exemplified on STM32CubeMX (Generator ID for this example is `CubeMX`). The information about the project is delivered in `<csolution-name>.cbuild-gen-idx.yml` and `<context>.cbuild-gen.yml` files. This information provides CubeMX with the project context, such as selected board or device, and CPU mode such as TrustZone disabled/enabled.

![Generator Information](./images/Generator-Information.png "Generator Information")

CubeMX/CubeMXBridge generates:

- `*.ioc` CubeMX project file with current project settings
- `*.c/.h` source files, i.e. for interfacing with drivers
- `cgen.yml` which provides the data for project import into the csolution build process.

> **Note:**  As CubeMX itself does not have the required interfaces to the csolution project format, there is a utility `CubeMX Bridge` that converts the `*.cbuild-gen*.yml` files into command-line options for CubeMX. `CubeMX Bridge` also generates the `CubeMX.cgen.yml` based on the information generated by CubeMX.

## Project Types

The following is an analysis of the different project scenarios, again exemplified with STM32CubeMX.
For a CubeMX integration two different project types should be considered:

- `project-type`: **single-core**: one `*.ioc` project file maps to one `*.cproject.yml` (potentially to one or more `target-types:`). In case that a device layer is used, it maps to one `*.clayer.yml` file.
- `project-type`: **multi-core** or **trustzone**: one `*.ioc` project file maps to several processors or to secure/non-secure projects. In csolution this is requires that multiple `*.cproject.yml` (or in case of layers multiple `*.clayer.yml`) files are considered.

CubeMX requires a generator output directory (that stores the *.ioc project along with generated files).

> **Note:**
>
> The generator output directory can be controlled at `csolution` level with [`generators:`](https://github.com/Open-CMSIS-Pack/cmsis-toolbox/blob/main/docs/YML-Input-Format.md#generators).

To integrate the generated files in a `csolution` based project a `<generator-id>.cgen.yml` is generated. As the generator is associated with one `generator component`, the location of this file is in the [`RTE` directory](https://github.com/Open-CMSIS-Pack/cmsis-toolbox/blob/main/docs/build-overview.md#rte-directory-structure) of the related `Cclass`.  The `<generator-id>.cgen.yml` should use relative paths, which requires that the relationship to the generator output directory is somewhat defined.

> **Note:**
>
> - Generator output directory can be controlled at `csolution` level with [`generators:`](https://github.com/Open-CMSIS-Pack/cmsis-toolbox/blob/main/docs/YML-Input-Format.md#generators).
> - RTE directory can be controlled at `cproject` level with [`rte:`](https://github.com/Open-CMSIS-Pack/cmsis-toolbox/blob/main/docs/YML-Input-Format.md#rte)
> - This proposal recommends to make the generator directory configurable via a generator registration file. In case of missing functionality, the existing options should be reviewed.

The proposed directory structure for CubeMX is:

### Project-Type: `single-core`

The `generator component` belongs to `cproject.yml`:

- `CubeMX.cgen.yml` file: `$ProjectDir()$/RTE/Device/$Dname$`
- Generator output files: `$ProjectDir()$/RTE/Device/$Dname$/generated/CubeMX`

The `generator component` belongs to `clayer.yml` (`<layer-dir>` refers to the directory that contains the `clayer.yml` file):

- `CubeMX.cgen.yml` file: `<layer-dir>/RTE/Device/$Dname$`
- Generator output files: `<layer-dir>/RTE/Device/$Dname$/generated/CubeMX`

### Project-Type: `multi-core` or `trustzone`

Generator files belong to `csolution.yml` (or to a `<layer-base-dir>` file that needs to be defined).

The `generator component` belongs to `cproject.yml`:

- In each related `cproject.yml` a `CubeMX.cgen.yml` file is added in: `$ProjectDir()$/RTE/Device/$Dname$`
- Generator output files: `$SolutionDir()$/RTE/Device/$Dname$/generated/CubeMX`

The `generator component` belongs to `clayer.yml`. A fixed directory structure is assumed with:

```txt
<layer-base-dir>            # stores multiple layers
<layer-base-dir>/layer1     # stores multiple layer1
<layer-base-dir>/layer2     # stores multiple layer2
```

> **Note:**
>
> The directory structure for multiple related layers needs potentially more work.

## Generator Start via `generator component`

This proposal requires that only one `generator component` with the same `<genrator-id>` is defined. There is more work required to allow multiple components using the same `<genrator-id>`. For the first `csolution` implementation no checks for consistency are required, but a warning could be issued when on attempt to run a generator with multiple `generator component` definitions.

**Example component for CubeMX:**

```xml
  <component Cclass="Device" Cgroup="STM32CubeMX" generator="STM32CubeMX">
    <description>Run STM32CubeMX for device configuration</description>
  </component>
```

## Global Generator Registry

As there is typically only one Generator for a `<generator-id>` is installed, a global registry could be introduced.

- In CMSIS-Toolbox `./etc` directory with a file `<generator-id>.generator.yml` could be introduced that registers the generators.
- In CMSIS-Toolbox `./bin` directory the 'Bridge programs' for CubeMX and MCUxpresso are added.

**Potential format of `STM32CubeMX.generator.yml`:**

```yml
generator:
  - id: STM32CubeMX                # potentially with version added by `@` notation
    download-url: https://www.st.com/en/development-tools/stm32cubemx.html
    run: ../bin/CubeMX2cgen     # the related bridge program
    output:
      - base-dir: RTE
        sub-dir: /generated/CubeMX
        project-type: single-core
      - base-dir: multi
        sub-dir: /generated/CubeMX
        project-type: multi-core, trustzone    
```

> **Note:**
>
> A reason for making output directories configurable is to support backwards compatibility with uVision.

The format of `*.generator.yml` supports the following keys:

Key              | Description
:----------------|:------------------
`generator:`     | start of a `*.generator.yml` file
`id:`            | `<generator-id>` referred in the `*.PDSC` file
`download-url:`  | URL for downloading the generator; is displayed when the bridge program cannot find the generator (but might be hard coded in the first release)
`run:`           | Name and location of the bridge program
`output:`        | List of output directories

`output:`        | Description
:----------------|:------------------
`base-dir:`      | Base directory location when not overwritten by `generator:` in the csolution.yml file. `RTE` uses the RTE directory, `solution` uses the base directory of the csolution.yml file (or `..\clayer`).
`sub-dir:`       | Additional directory extension; could be used to achieve backward compatibility with uVision.

The argument to the Bridge Program is the filename of `<csolution-name>.cbuild-gen-idx.yml`.

## Additions to `.cbuild-gen-idx.yml` and `.cbuild-idx.yml`

It might make that `csolution` lists the following information for the user and the bridge program:

- `project-type:` with values `single-core`, `multi-core`, `trustzone`
- `generator:` with directory structure

```yml
  generators:
    - id: STM32CubeMX
      for-context: <list of context types>
      output: <directory for generator output>
      cgen-dirs:
        - path: <cgen location>
          for-project-part: <secure | non-secure | processor-core<x> > 
  ```

**Example:**

```yml
  project-type: trustzone
  
  generator:
    - id: STM32CubeMX
      for-context: +B-U585I-IOT02A    # not required for `.cbuild-gen-idx.yml` as run gets the context 
      device: STM32U585AIIx
      board: B-U585I-IOT02A           # only present when board: is specified
      output: .\generated\CubeMX
      cgen-dirs:
        - path: .\secure-project\RTE\Device\STM32U585AIIx
          for-project-part: secure
  
        - path: .\nosecure-project\RTE\Device\STM32U585AIIx
          for-project-part: non-secure
```

> **Note:**
>
> Providing the `generator:` information in `.cbuild-idx.yml` gives the user a protocol file that shows how generators are used in the `csolution` project.

## Impact to PDSC files and Packs

The following changes might be implemented in the PDSC files:

- Element [`<generators>`](https://open-cmsis-pack.github.io/Open-CMSIS-Pack-Spec/main/html/pdsc_generators_pg.html) is optional as there is a [global registration of generators](#global-generator-registry).
- Element [`<component>`](https://open-cmsis-pack.github.io/Open-CMSIS-Pack-Spec/main/html/pdsc_components_pg.html#element_component)
  - Attribute `generator` checks if a `cgen.yml` or `*.gpdsc` file is present.
  - Add new attribute `generator-type` with option `single` (default) and `multi` to define the [project scenarios](Project Scenarios). As a `<component>` can have conditions, it is possible to use a `single` structure when for example TrustZone is disabled.
  - Allow that a `<component>` with a `<generator>` can be empty (has no source files).

The proposed changes allow to use:

- Standard STM32CubeMX device and board firmware packs. Except adding the STM32CubeMX component shown above, there is **no special work** in the Packs required. Specifically the reformatting of the STM32CubeMX firmware in components is not longer required.
- It is possible to start from a `device` or `board` which utilizes the feature set of STM32CubeMX.

## `cgen.yml` Format

The `*.cgen.yml` format needs formal definition. Starting proposal is to use `*.clayer.yml`. Extensions are possible as discussed in [637](https://github.com/Open-CMSIS-Pack/devtools/issues/637). However suggestion is to implement only the currently used feature set.

This are [Example Projects](https://github.com/DavidLesnjak/cgen_mockup) that use this proposal.

For the first implementation of the `*.cgen.yml` format the following keys should be supported:
`generated-by:`, `for-device:`, `for-board:`, `groups:`, `components:`. `generated-by:` is a descriptive text. Other definitions are identical with `clayer.yml`.

```yml
generator-import:       start of a cgen.yml file
  generated-by: STM32CubeMX via CubeMX2cgen
  for-device  : STM32U585AIIx
  for-board   : B-U585I-IOT02A
  groups:             # List of source file groups along with source files; identical to clayer.yml
    group: STM32CubeMX
      ...
  components:         # List of components that should be added; requirement of MCUxpressos
      ...
```

## Potential Execution Sequence

1. User starts a generator with: `csolution run my.csolution.yml -g STM32CubeMX -c +B-U585I-IOT02A`

2. `csolution` program performs the following operations:
   - create `my.cbuild-gen-idx.yml` and other related files.
   - read `STM32Cube.generator.yml` to start bridge program (`run:` key)
  
3. STM32CubeMX2cgen bridge performs the following operations:
   - read `my.cbuild-gen-idx.yml` to get the location for `generator output` and `*.cgen.yml` files along with device and board information.
   - check if *.ioc file in `generator output` exists and depending on that call STM32CubeMX with parameters to start or update a project.
   - Once STM32CubeMX finishes, generated the required `*.cgen.yml` files.
