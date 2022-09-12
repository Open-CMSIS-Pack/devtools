# External Dependencies

The external folder is used to store third party external
dependencies. These dependencies are fetch as a `git submodule`. No components
should be committed into this folder.

```txt
    📂external
    ┣ 📂cxxopts
    ┣ 📂googletest
    ┣ 📂json
    ┣ 📂json-schema-validator
    ┗ 📂yaml-cpp
```

## cxxopts

<!-- markdown-link-check-disable-next-line -->
The [cxxopts](./cxxopts) directory contains sources of C++ option parser
library fetched from [here](https://github.com/jarro2783/cxxopts).

## googletest

<!-- markdown-link-check-disable-next-line -->
The [googletest](./googletest) directory contains sources of unit test
framework used to develop tests and is fetched from
[here](https://github.com/google/googletest).

## json

<!-- markdown-link-check-disable-next-line -->
The [json](./json) directory contains sources of JSON parser
library fetched from [here](https://github.com/nlohmann/json).

## json-schema-validator

<!-- markdown-link-check-disable-next-line -->
The [son-schema-validator](./son-schema-validator) directory contains sources
of json-schema-validator library fetched from
[here](https://github.com/pboettch/json-schema-validator).

## yaml-cpp

<!-- markdown-link-check-disable-next-line -->
The [yaml-cpp](./yaml-cpp) directory contains sources of YAML parser
library fetched from [here](https://github.com/jbeder/yaml-cpp).
