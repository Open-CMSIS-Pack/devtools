# SVDConv Test Development

This repository contains `svdconv` test development components.

```txt
    📦test
    ┣ 📂data
    ┣ 📂integtests
    ┗ 📂unittests
```

## data

[data](./data) is a common place to store all
the predefined test inputs required to run the tests. All future
test input extensions should be placed under this directory.

## Integration Tests

[integtests](./integtests) directory contains all the sources containing
integration tests and compiles into an executable. The tests make use of test
input SVD files from [test](./data/) directory containing the
example SVD.

## Unit Tests

[unittests](./UnitTests) holds the sources accommodating all the unit tests and
compiles into an executable. These unit tests uses **gtest** as a test framework.
