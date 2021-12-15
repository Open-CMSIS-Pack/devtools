# CMSIS Project Manager [Draft]

The utility `projmgr` assists the generation of a CMSIS Project Description file
according to the standard
[CPRJ format](https://arm-software.github.io/CMSIS_5/Build/html/cprjFormat_pg.html)
and provides commands to search packs, devices and components from installed packs
as well as unresolved component dependencies.

## Requirements

The CMSIS Pack repository must be present in the environment.

There are several ways to initialize and configure the pack repository, for example using the 
`cpackget` tool:
https://github.com/Open-CMSIS-Pack/cpackget

## Quick Start

The `projmgr` binaries as well as python interfaces for all supported platforms can be downloaded
from the releases page:
https://github.com/Open-CMSIS-Pack/devtools/releases/tag/tools%2Fprojmgr%2F0.9.0

Before running `projmgr` the location of the pack repository shall be set via the environment variable
`CMSIS_PACK_ROOT` otherwise its default location will be taken.

## Usage

```
CMSIS Project Manager 0.9.0 (C) 2021 ARM
Usage:
  projmgr <command> [<args>] [OPTIONS...]

Commands:
  list packs          Print list of installed packs
       devices        Print list of available device names
       components     Print list of available components
       dependencies   Print list of unresolved project dependencies
  convert             Convert cproject.yml in cprj file
  help                Print usage

Options:
  -p, --project arg   Input cproject.yml file
  -s, --solution arg  Input csolution.yml file (TBD)
  -f, --filter arg    Filter words
  -o, --output arg    Output directory
  -h, --help          Print usage
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
list components [--project <example.cproject.yml> --filter "<filter words>"]
```

- Print list of unresolved project dependencies. The device and components selection must be provided in the `cproject.yml` file with the option `--input`. The list can be filtered by words provided with the option `--filter`:
```
list dependencies --project <example.cproject.yml> [--filter "<filter words>"]
```

Convert cproject.yml in cprj file:
```
convert --project <example.cproject.yml>
```


## Input .cproject.yml file

The YAML structure is described below.

``` yml
project:
  name: <value>
  description: <value>

  target:
    device: <vendor>::<device-name>:<processor-name>

  toolchain: <name>@<version>

  packages:
    - package: <vendor>::<name>@<version>

  components:
    - component: <identifier TBD>

  groups:
    - group: <group-name>
      files: 
        - file: <file-name>
      groups: 
        - group: <nested-group-name>
          files: 
            - file: <nested-file-name>
```

## Public functions
```
  static int RunProjMgr(int argc, char **argv);
  void PrintUsage(void);
  ProjMgrParser& GetParser(void);
  ProjMgrWorker& GetWorker(void);
  ProjMgrGenerator& GetGenerator(void);

  Parser:
  bool ParseCproject(const std::string& input, CprojectItem& cproject);

  Worker:
  bool ProcessProject(ProjMgrProjectItem& project, bool resolveDependencies);
  bool ListPacks(const string& filter, set<string>& packs);
  bool ListDevices(ProjMgrProjectItem& project, const string& filter, set<string>& devices);
  bool ListComponents(ProjMgrProjectItem& project, const string& filter, set<string>& components);
  bool ListDependencies(ProjMgrProjectItem& project, const string& filter, std::set<string>& dependencies);

  Generator:
  bool GenerateCprj(ProjMgrProjectItem& project, const string& filename);
```

## Python interface

Python library interfaces are generated with SWIG and can be found among the release artifacts.
A Python CLI wrapper is also provided as an example: [projmgr-cli.py](https://github.com/devtools/blob/main/tools/projmgr/swig/projmgr-cli.py).

## Revision History
| Version  | Description
|:---------|:----------------------------------------
| 0.9.0    | Release for alpha review
