# Global test data

This folder contains data required by integration as well as module and unit tests.
The ide is to provide central location for data required by various tests avoiding replications.
The directory should be referred in the code by specifying GLOBAL_TEST_DIR macro in `CMakeLists.txt`:

````cmake
add_definitions(-DGLOBAL_TEST_DIR="${CMAKE_SOURCE_DIR}/test")
````

## Subdirectories

### `packs`

CMSIS pack installation directory that contains test packs. The directory is supposed to be used as CMSIS_PACK_ROOT in
the tests. In C++ code it can be constructed as:

```c++
string CMSIS_PACK_ROOT = GLOBAL_TEST_DIR + "/packs";
```

`Note`: only specially designed test packs should be put in this directory.

The following packs are currently in use:

#### `ARM.RteTest_DFP.0.1.1`

This typical Device Family Pack contains device and board descriptions as well as typical DFP components like
`Device.Startup`.

#### `ARM.RteTest.0.1.0`

This pack contains data to test following RTE features:

- constructing component hierarchy tree
- component bundles
- support for pre-include files
- evaluating and resolving component dependencies
- checking API dependencies and conflicts
- processing conditional dependencies
- missing component dependencies and APIs

The pack contains `PreInclude.uvprojx` example that can be loaded an build in uVision.

### `projects`

This directory contains cprj-based projects to be used as test input

- the projects may only use CMSIS pack that are available the `packs` directory
- it is possible to refer to non-existing packs for negative tests
- no real projects from other repositories or customer cases may be put here

#### `RteTestM3`

`RteTestCM3.cprj` is exported from "Cortex-M3" target of `PreInclude.uvprojx` example. The project allows to test the
following functionality:

- successful project load (all is resolved)
- support for global and component-level pre-include files
- conditional dependency (component RteTest:G_B is not required)
- querying information about device, board, project files and components via grpc

The directory contais to more projects:

- `MissingComponent.cprj`: a simple project to test a missing component. It contains references to one resolvable and
  one missing component.
- `broken_xml.cprj`: a file with illegal XML content. Loading that project is expected to fail.

#### `RteTestM4`

RteTestCM4.cprj is exported from "Cortex-M4" target of `PreInclude.uvprojx` example.
It is similar to `RteTestM3.cprj`, but uses different device and no board information. Primary use is to validate
conditional dependency: component RteTest:G_B is required for Cortex-M4.
In this project component G_B is selected => condition result is FULFILLED (all dependencies are resolved).

`RteTestCM4_CompDev.cprj` is the same as `RteTestM3.cprj` but component G_B is NOT selected => condition result is
SELECTABLE

### `local`

This directory contains a dummy local repository index file ```.Local/local_repository.pidx``` only for local
repository test purposes.
