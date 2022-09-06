# csolution - CMSIS Project Manager

The **projmgr** directory contains the **csolution** tool that assists with the generation of  CMSIS Project Description files in 
[CPRJ format](https://arm-software.github.io/CMSIS_5/Build/html/cprjFormat_pg.html).
It d provides commands to search packs, devices and components from installed packs
as well as unresolved component dependencies.

**Refer to:**
- [csolution Overview](./docs/Manual/Overview.md) for details and usage information for the CMSIS Project Manager
- [README.md](https://github.com/Open-CMSIS-Pack/cmsis-toolbox/blob/main/README.md) for an overview of the CMSIS-Toolbox
- [Installation](https://github.com/Open-CMSIS-Pack/cmsis-toolbox/blob/main/docs/Installation.md) for details on how to setup the CMSIS-Toolbox

## Python interface

Python library interfaces are generated with SWIG and can be found among the release artifacts.
A Python CLI wrapper is also provided as an example: [projmgr-cli.py](https://github.com/devtools/blob/main/tools/projmgr/swig/projmgr-cli.py).
