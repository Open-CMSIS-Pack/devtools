# External Dependencies

The [external](./external/) folder is used to store third party external
dependencies. These dependencies are fetch as a `git submodule`. No components
should be committed into this folder.

```
    📂external
    ┣ 📂cxxopts
    ┣ 📂googletest
    ┗ 📂yaml-cpp
```

## cxxopts

The [cxxopts](./cxxopts) directory contains sources of C++ option parser
library fetched from [here](https://github.com/jarro2783/cxxopts).

## googletest

The [googletest](./googletest) directory contains sources of unit test
framework used to develop tests and is fetched from
[here](https://github.com/google/googletest).

## yaml-cpp

The [yaml-cpp](./yaml-cpp) directory contains sources of YAML parser
library fetched from [here](https://github.com/jbeder/yaml-cpp).

## json

The [json](./json) directory contains sources of JSON parser
library fetched from [here](https://github.com/nlohmann/json).

## json-schema-validator

The [son-schema-validator](./son-schema-validator) directory contains sources
of json-schema-validator library fetched from
[here](https://github.com/pboettch/json-schema-validator).
