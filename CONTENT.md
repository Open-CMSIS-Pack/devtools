# Open-CMSIS-Pack Development Tools and Libraries

This repository contains components used to develop Open-CMSIS-Pack
command-line tools based on reusable libraries.

Files and directories in this tree follow a structure meant to:

- Make it easy to develop and maintain the components.
- Promote consistency throughout the repository.
- Make it easy to decide where in the tree new additions should be placed.

```txt
    📦devtools
    ┣ 📂.github
    ┣ 📂cmake
    ┣ 📂docs
    ┣ 📂external
    ┣ 📂libs
    ┣ 📂scripts
    ┣ 📂test
    ┣ 📂tools
    ┣ 📜.gitattributes
    ┣ 📜.gitignore
    ┣ 📜.gitmodules
    ┣ 📜.pre-commit-config.yaml
    ┣ 📜CONTENT.md
    ┣ 📜CMakeLists.txt
    ┣ 📜LICENSE
    ┣ 📜LICENSE.md
    ┗ 📜README.md
```

## Folders

### Github workflow

The [.github](./.github) directory contains the github workflow configurations for
continous integration environement.

### CMake Helpers

Open-CMSIS-pack uses cross-platform build environment `CMake`. The [./cmake](./cmake)
directory contains a list of `.cmake` files which are the reusable components used by
CMake configurations.

### Documentation

The [doc](./docs) directory contains all the documentation written using
Markdown and are documenting different steps and guidelines to used to develop
the tools and libraries.

### External Dependencies

The folder [external](./external) contains third party components
the tools and libraries depend on. These dependencies are pulled in as
submodules and remain unmodified.

### Library Components

Components not compiling into executable programs (i.e. static or dynamic
libraries) are kept in [libs](./libs) folder and are intended to be shared
between multiple components.

> **Note:** Each library directory contains a `test` sub-directory that holds
the source code to test the library.

```txt
    ┣ 📂libs
    ┃ ┣ 📂crossplatform
    ┃ ┣ 📂errlog
    ┃ ┣ 📂rtefsutils
    ┃ ┣ 📂rtemodel
    ┃ ┣ 📂rteutils
    ┃ ┣ 📂xmlreader
    ┃ ┣ 📂xmltree
    ┃ ┣ 📂xmltreeslim
    ┃ ┗ 📜CONTENT.md
```

### Scripts and Helpers

The [scripts](./scripts) folder stores helper script. The intent of this
directory is to store all future helper scripts.

### Test Data

The test data under [test](./test) directory is required for testing purposes
(i.e. not production code for release) and is intended to be shared by multiple
tests in this repository.

### Development Tools

Components compiling into one ore more executable programs (i.e. development
tools) are kept [tools](./tools) folder.

> **Note:** All tools contains tests respectively to validate their implementations.

```txt
    ┣ 📂tools
    ┃ ┣ 📂buildmgr
    ┃ ┣ 📂packchk
    ┃ ┣ 📂packgen
    ┃ ┣ 📂projmgr
    ┃ ┗ 📜CONTENT.md
```

## Files

<!-- markdownlint-capture -->
<!-- markdownlint-disable MD013 -->

| File                    | Usage                                                                                     |
| ----------------------- | ----------------------------------------------------------------------------------------- |
| .gitattributes          | This file allows you to specify the file and path attributes that should be used by git when performing git actions, such as git commit etc. |
| .gitignore              | This file contains configuration for git to ignore when committing your project to the GitHub repository. |
| .gitmodules             | The file contains information per submodule that needs to be downloaded.                  |
| .pre-commit-config.yaml | The file list [pre-commit](https://pre-commit.com/) configurations used by the CI environment. |
| CMakeLists.txt          | This top level script lists the metadata to generate build files for a specific environment. |
| LICENSE                 | Lists the license term and conditions for the usage of anything from this repository.     |
| LICENSE.md              | The file lists all the license agreements and the usage of external licenses.             |
| README.md               | The file contains information for the user about the software, project, code and instructions for the usage. |

<!-- markdownlint-restore -->
