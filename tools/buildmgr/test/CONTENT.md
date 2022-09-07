# CMSIS-Build Test Development

This repository contains CMSIS-Build test
development components for testing of CMSIS-Build utilities.

```txt
    📦test
    ┣ 📂integrationtests
    ┣ 📂scripts
    ┣ 📂testinput
    ┗ 📂unittests
```

## Scripts

In [scripts](./scripts) a collection of helper shell
scripts are stored for testing purpose and intended to be shared
between multiple tests.

## TestInput

[testinput](./testinput) is a common place to store all
the predefined test inputs required to run the tests. All future
test input extensions should be placed under this directory.

## IntegrationTests

[integrationtests](./integrationtests) directory
contains all the sources containing integration tests and compiles
into an executable. It also contains the defined examples on
which the integration tests are performed. The [Pack](./integrationtests/pack)
directory contains the pack required as a prerequisite for integration tests.

## UnitTests

[unittests](./unittests) holds the sources accommodating
all the unit tests and compiles into an executable. These unit
tests uses **gtest** as a test framework and tests the CMSIS-Build sources.
