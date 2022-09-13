# Development Tools

The tools folder contains components that are compiled into
executable programs.

```txt
    📂tools
    ┣ 📂buildmgr
    ┣ 📂packchk
    ┣ 📂packgen
    ┗ 📂projmgr
```

## buildmgr

The [buildmgr](./buildmgr) directory contains sources for a command line
utility `cbuildgen` and its components which are used for the build process.
The directory also consists of tests developed to validate `cbuildgen`.

## packchk

The [packchk](./packchk) directory contains sources for the utility `packchk`
and its components which are responsible for checking CMSIS-Packs.

## packgen

The [packgen](./packgen) directory contains sources for the utility `packgen`
and its components which are responsible for generating CMSIS-Packs.

## projmgr

The [projmgr](./projmgr) directory contains sources for the utility `projmgr`
and its components which are responsible for handling CMSIS projects and
generating CPRJ files.
