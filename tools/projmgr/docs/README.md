# CMSIS Project Manager [Draft]

The utility `projmgr` assists the generation of a CMSIS Project Description file
according to the standard
[CPRJ format](https://arm-software.github.io/CMSIS_5/Build/html/cprjFormat_pg.html)
and provides commands to search packs, devices and components from installed packs
as well as unresolved component dependencies.

## Requirements

The CMSIS Pack repository must be present in the environment.

There are several ways to initialize and configure the pack repository, for example using the 
[cpackget](https://github.com/Open-CMSIS-Pack/cpackget) tool.

## Quick Start

The `projmgr` binaries as well as python interfaces for all supported platforms can be downloaded
from the releases page.

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

- Print list of project contexts. Each combination of cproject.yml, `build-type` and `target-type` given under a `solution` is a different context. The list can be filtered by words provided with the option `--filter`:
```
list contexts --solution <example.csolution.yml> [--filter "<filter words>"]
```

Convert cproject.yml or csolution.yml in cprj files:
```
convert --project <example.cproject.yml>
convert --solution <example.csolution.yml>
```


## Input files

The YAML structure draft is described in: [Overview.md](Manual/Overview.md)

## Public functions
TBD

## Python interface

Python library interfaces are generated with SWIG and can be found among the release artifacts.
A Python CLI wrapper is also provided as an example: [projmgr-cli.py](https://github.com/devtools/blob/main/tools/projmgr/swig/projmgr-cli.py).

## Revision History
| Version  | Description
|:---------|:----------------------------------------
| 0.9.0    | Release for alpha review
