# Project Manager

The **projmgr** directory contains the `csolution` utility that assists with the generation of CMSIS project
description files in [CPRJ format](https://arm-software.github.io/CMSIS_5/Build/html/cprjFormat_pg.html) that are used
by [*cbuild*](../buildmgr). It provides commands to search packs, devices, and components from installed packs as well
as unresolved component dependencies. It is distributed as part of the **CMSIS-Toolbox**.

## References

- [csolution Overview](./docs/Manual/Overview.md) for details about the CMSIS Project Manager.
- [CMSIS-Toolbox Overview](https://github.com/Open-CMSIS-Pack/cmsis-toolbox/blob/main/README.md)
- [CMSIS-Toolbox Installation](https://github.com/Open-CMSIS-Pack/cmsis-toolbox/blob/main/docs/installation.md)

## Python Interface

Python library interfaces are generated with SWIG and can be found among the release artifacts.
A Python CLI wrapper is also provided as an example:
[projmgr-cli.py](https://github.com/Open-CMSIS-Pack/devtools/blob/main/tools/projmgr/swig/python/examples/projmgr-cli.py).
