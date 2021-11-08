# CMSIS Project Manager - MVP Prototype

The utility `projmgr` assists the generation of a CMSIS Project Description file
according to the standard
[CPRJ format](https://arm-software.github.io/CMSIS_5/Build/html/cprjFormat_pg.html)
and provides commands to search packs, devices and components from installed packs
as well as unresolved component dependencies.

## Overview
[Overview](Overview.md) explains the top-level concept and the project file formats.

## Prototype
The initial MVP prototype proposal can be found in the following Open-CMSIS-Pack forked branch:
<br/>
https://github.com/brondani/devtools/tree/mvp-prototype

## Requirements

The CMSIS Pack repository must be present in the environment.

There are several ways to initialize and configure the pack repository, for example using the 
`cpackget` tool:
<br/>
https://github.com/Open-CMSIS-Pack/cpackget

## Quick Start

The `projmgr` binaries as well as python interfaces for all supported platforms can be downloaded
from the releases page:
<br/>
https://github.com/brondani/devtools/releases/tag/tools%2Fprojmgr%2F0.9.0

Before running `projmgr` the location of the pack repository shall be set via the environment variable
`CMSIS_PACK_ROOT` otherwise its default location will be taken.

## Usage

```
CMSIS Project Manager 0.9.0 (C) 2021 ARM
Usage:
  projmgr <command> [<args>] [OPTIONS...]

Commands:
  list packs        Print list of installed packs
       devices      Print list of available device names
       components   Print list of available components
       dependencies Print list of unresolved project dependencies
  convert           Convert cproject.yml in cprj file
  help              Print usage

Options:
  -i, --input arg   Input YAML file
  -f, --filter arg  Filter words
  -h, --help        Print usage
```

## Commands

- Print list of installed packs. The list can be filtered by words provided with the option `--filter`:
```
list packs [--filter "<filter words>"]
```

- Print list of available device names. The list can be filtered by words provided with the option `--filter`:
```
list devices [--filter "<filter words>"]
```

- Print list of available components. The list can be filtered by a selected device in the `cproject.yml` file with the option `--input` and/or by words provided with the option `--filter`:
```
list components [--input <example.cproject.yml> --filter "<filter words>"]
```

- Print list of unresolved project dependencies. The device and components selection must be provided in the `cproject.yml` file with the option `--input`. The list can be filtered by words provided with the option `--filter`:
```
list dependencies --input <example.cproject.yml> [--filter "<filter words>"]
```

Convert cproject.yml in cprj file:
```
convert --input <example.cproject.yml>
```


## Input .cproject.yml file

The YAML structure is described below.

``` yml
project:
  device: <value>
  attributes: {<key:value list>}
  type: <value>
  output: <value>

compiler:
  name: <value>
  version: <value>

components:
  - filter: <value>
    attributes: {<key:value list>}
    
files:
  - file: <value>
    category: <value>
  - group: <value>
    files: 
      - file: <value>
        category: <value>
```

### project
| Argument        | Description
|:----------------|:----------------------------------------
| device          | Device name
| attributes      | Device attributes: [Dfpu](https://arm-software.github.io/CMSIS_5/Build/html/cprj_types.html#DfpuEnum), [Dmpu](https://arm-software.github.io/CMSIS_5/Build/html/cprj_types.html#DmpuEnum), [Dendian](https://arm-software.github.io/CMSIS_5/Build/html/cprj_types.html#DendianEnum), [Dsecure](https://arm-software.github.io/CMSIS_5/Build/html/cprj_types.html#DsecureEnum), [Dmve](https://arm-software.github.io/CMSIS_5/Build/html/cprj_types.html#DmveEnum)
| type            | Output type: `exe` or `lib`
| output          | Output build folder
<br/>

### compiler
| Argument        | Description
|:----------------|:----------------------------------------
| name            | Compiler name
| version         | Compiler version
<br/>

### components
| Argument        | Description
|:----------------|:----------------------------------------
| filter          | Filter words
| attributes      | Components attributes
<br/>

### files
| Argument        | Description
|:----------------|:----------------------------------------
| file            | File path and type according to [file category](https://arm-software.github.io/CMSIS_5/Build/html/cprj_types.html#FileCategoryEnum)
| group           | Child group name. It accepts nested `files` nodes
<br/>

## Public functions
```
  static int RunProjMgr(int argc, char **argv);
  void PrintUsage();
  bool LoadPacks();
  bool CheckRteErrors();
  bool ListPacks(set<string>& packs);
  bool ListDevices(set<string>& devices);
  bool ListComponents(set<string>& components);
  bool ListDependencies(set<string>& dependencies);
  bool ParseInput();
  bool ProcessProject();
  bool GenerateCprj();
  bool ProcessDevice();
  bool ProcessComponents();
  bool ProcessDependencies();
```

## Python interface

Python library interfaces are generated with SWIG and can be found among the release artifacts.
A Python CLI wrapper is also provided as an example: [projmgr-cli.py](https://github.com/brondani/devtools/blob/mvp-prototype/tools/projmgr/swig/projmgr-cli.py).

## Revision History
| Version  | Description
|:---------|:----------------------------------------
| 0.9.0    | Release for alpha review
