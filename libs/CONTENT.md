# Library Components

The libs directory contain library components compiled into
static or dynamic libraries and are intended to be shared between multiple
components.

> **Note:** Each library directory contains a `test` sub-directory that holds
the source code to test the library.

```txt
    📂libs
    ┣ 📂crossplatform
    ┣ 📂errlog
    ┣ 📂rtefsutils
    ┣ 📂rtemodel
    ┣ 📂rteutils
    ┣ 📂xmlreader
    ┣ 📂xmltree
    ┗ 📂xmltreeslim
```

## crossplatform

The [crossplatform](./crossplatform) directory contains all the
platform-specific code that allows the CMSIS-Build project to be constructed
and run on multiple platforms. The intent is to eliminate platform-specific
switching inside the main codebase.

## errlog

The [errlog](./errlog) directory contains the consolidated error messages
in order to log errors.

## rtefsutils

The [rtefsutils](./rtefsutils) directory contains the sources of a library which
contains all RTE filesystem utility functions to be used across multiple components.

## rtemodel

The [rtemodel](./rtemodel) directory contains the sources to represent CMSIS RTE
data model.

## rteutils

The [rteutils](./rteutils) directory contains the sources of a library which
contains all RTE utility functions to be used across multiple components.

## xmlreader

The [xmlreader](./xmlreader) directory contains the sources of library used to parse
XML formatted files.

## xmltree

The [xmltree](./xmltree) directory contains the sources of a library which
represents XML tree in memory.

## xmltreeslim

The [xmltreeslim](./xmltreeslim) directory contains the sources of a library which
contains XML interface that reads data into a tree structure.
