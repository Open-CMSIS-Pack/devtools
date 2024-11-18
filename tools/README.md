# Open-CMSIS-Pack Tools

These command-line tools provide the foundation for Open-CMSIS-Pack-based software development flows and are part of
the [**CMSIS-Toolbox**](https://github.com/Open-CMSIS-Pack/cmsis-toolbox).

## SVD file validation

- [**svdconv**](https://open-cmsis-pack.github.io/svd-spec/main/svd_SVDConv_pg.html) validates CMSIS-SVD files and
  generates CMSIS-compliant device header files.

## Package creation and validation

- [**packgen**](./packgen/README.md) creates a software pack from a `CMake` based software repository.
- [**packchk**](./packchk/README.md) validates a software pack description and the archive content.
- [**gen-pack**](https://github.com/Open-CMSIS-Pack/gen-pack) is a library with helper function to assemble a
  `gen_pack.sh` script that creates a Open-CMSIS-Pack.

## Project management

- [**csolution**](https://github.com/Open-CMSIS-Pack/cmsis-toolbox/blob/main/docs/build-tools.md#csolution-invocation)
  creates build information for embedded applications that consist of one or more related projects.

## Build management

- [**cbuild**](https://github.com/Open-CMSIS-Pack/cmsis-toolbox/blob/main/docs/build-tools.md#cbuild-invocation)
  orchestrates the overall build steps utilizing the various tools of the CMSIS-Toolbox and a CMake-based compilation process.
