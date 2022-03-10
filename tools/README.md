# CMSIS-Toolbox

The **CMSIS-Toolbox** provides command-line tools that provide the foundation for software development flows.  The CMSIS-Toolbox contains the following tools:

**Package creation** and validation:
- [**packgen**](packgen/docs/packgen.md): create a software pack from a Cmake based software repository
- [**packchk**](packchk/docs/packchk.md): semantic validation of a software pack description and the archive content

**Package management** including discovery of components, devices, boards and examples:
- [**cpackget**](cpackget/docs/cpackget.md): download, add and remove packs and local repositories to CMSIS_PACK_ROOT
 
**Project management** for constructing projects from local files and software components:
- [**csolution**](projmgr/docs/Manual/Overview.md): manage complex applications with `*.yaml` user input files and content from CMSIS-Packs; output `*.cprj` files for reproduceable builds using **cbuild** in IDE and CI environments

**Build management** to compile and generate executable program images:
- [**cbuild**](buildmgr/docs/cbuild.md) (aka CMSIS-Build): convert a `*.cprj` file that describes a single target and single configuration of a project to a CMake input; start the build process.

The following diagram illustrates how these tools improve the software component reuse during the development cycle.

![CMSIS-Toolbox Overview](./projmgr/docs/images/CMSIS-Toolbox-Overview.png "CMSIS-Toolbox Overview")

## Download and Install

The **CMSIS-Toolbox** is currently under development. 

- [**Download Alpha Version**](https://github.com/Open-CMSIS-Pack/devtools/releases)
- [**Setup and Installation**](./toolbox/docs/installation.md)